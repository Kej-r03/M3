/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

use base::cell::StaticCell;
use base::dtu;
use base::kif::PageFlags;
use base::libc;
use core::fmt;
use core::ptr;

use vma;
use vpe;

type IsrFunc = extern "C" fn(state: &mut State) -> *mut libc::c_void;

extern "C" {
    fn isr_init();
    fn isr_reg(idx: usize, func: IsrFunc);
    fn isr_enable();

    static idle_stack: libc::c_void;
}

pub const DPL_USER: u64 = 3;

pub const SEG_UCODE: u64 = 3;
pub const SEG_UDATA: u64 = 4;

pub const PEXC_ARG0: usize = 14; // rax
pub const PEXC_ARG1: usize = 12; // rcx

#[repr(C, packed)]
pub struct State {
    // general purpose registers
    pub r: [usize; 15],
    // interrupt-number
    pub irq: usize,
    // error-code (for exceptions); default = 0
    pub error: usize,
    // pushed by the CPU
    pub rip: usize,
    pub cs: usize,
    pub rflags: usize,
    pub rsp: usize,
    pub ss: usize,
}

fn vec_name(vec: usize) -> &'static str {
    match vec {
        0x00 => "Divide by zero",
        0x01 => "Single step",
        0x02 => "Non maskable",
        0x03 => "Breakpoint",
        0x04 => "Overflow",
        0x05 => "Bounds check",
        0x06 => "Invalid opcode",
        0x07 => "Co-proc. n/a",
        0x08 => "Double fault",
        0x09 => "Co-proc seg. overrun",
        0x0A => "Invalid TSS",
        0x0B => "Segment not present",
        0x0C => "Stack exception",
        0x0D => "Gen. prot. fault",
        0x0E => "Page fault",
        0x10 => "Co-processor error",
        _ => "<unknown>",
    }
}

impl fmt::Debug for State {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        writeln!(fmt, "State @ {:#x}", self as *const State as usize)?;
        writeln!(fmt, "  vec: {:#x} ({})", { self.irq }, vec_name(self.irq))?;
        writeln!(fmt, "  error:  {:#x}", { self.error })?;
        writeln!(fmt, "  rip:    {:#x}", { self.rip })?;
        writeln!(fmt, "  rflags: {:#x}", { self.rflags })?;
        writeln!(fmt, "  rsp:    {:#x}", { self.rsp })?;
        writeln!(fmt, "  cs:     {:#x}", { self.cs })?;
        writeln!(fmt, "  ss:     {:#x}", { self.ss })?;
        for (idx, r) in { self.r }.iter().enumerate() {
            writeln!(fmt, "  r[{:02}]:  {:#x}", idx, r)?;
        }
        Ok(())
    }
}

static STOPPED: StaticCell<bool> = StaticCell::new(false);

impl State {
    pub fn came_from_user(&self) -> bool {
        (self.cs & DPL_USER as usize) == DPL_USER as usize
    }

    pub fn nested(&self) -> bool {
        !self.came_from_user()
    }

    pub fn init(&mut self, entry: usize, sp: usize) {
        self.rip = entry;
        self.rsp = sp;
        self.r[8] = 0; // rbp
        self.r[14] = 0xDEAD_BEEF; // set rax to tell crt0 that we've set the SP

        self.rflags = 0x200; // enable interrupts
                             // run in user mode
        self.cs = ((SEG_UCODE << 3) | DPL_USER) as usize;
        self.ss = ((SEG_UDATA << 3) | DPL_USER) as usize;
    }

    pub fn stop(&mut self) {
        if self.nested() {
            // prevent us from sleeping by setting the user event
            dtu::DTU::set_event().ok();
            *STOPPED.get_mut() = true;
        }
        else {
            self.rip = crate::sleep as *const fn() as usize;
            self.rsp = unsafe { &idle_stack as *const libc::c_void as usize };
            self.r[8] = self.rsp; // rbp and rsp

            // remove user event again
            let our = vpe::our();
            our.set_vpe_reg(our.vpe_reg() & !dtu::EventMask::USER.bits());
            *STOPPED.get_mut() = false;
        }
    }

    pub fn finalize(&mut self) -> *mut libc::c_void {
        if *STOPPED {
            self.stop();
        }
        self as *mut Self as *mut libc::c_void
    }
}

pub fn enable_ints() -> bool {
    let prev = unsafe {
        let mut flags: usize;
        asm!("pushf; pop $0" : "=r"(flags) : : "memory");
        (flags & 0x200) != 0
    };
    unsafe { asm!("sti" : : : "memory") };
    prev
}

pub fn restore_ints(prev: bool) {
    if !prev {
        unsafe { asm!("cli" : : : "memory") };
    }
}

pub fn is_stopped() -> bool {
    // use volatile because STOPPED may have changed via a nested IRQ
    unsafe { ptr::read_volatile(STOPPED.get_mut()) }
}

pub fn init() {
    unsafe {
        isr_init();
        for i in 0..=64 {
            match i {
                14 => isr_reg(i, crate::mmu_pf),
                63 => isr_reg(i, crate::pexcall),
                64 => isr_reg(i, crate::dtu_irq),
                i => isr_reg(i, crate::unexpected_irq),
            }
        }
        isr_enable();
    }
}

pub fn handle_mmu_pf(state: &mut State) {
    let cr2: usize;
    unsafe {
        asm!( "mov %cr2, $0" : "=r"(cr2));
    }

    let perm = paging::MMUFlags::from_bits_truncate(state.error & PageFlags::RW.bits() as usize);
    // the access is implicitly no-exec
    let perm = paging::to_page_flags(perm | paging::MMUFlags::NX);

    vma::handle_pf(state.came_from_user(), cr2, perm, state.rip);
}
