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

use core::mem;
use m3::cap::Selector;
use m3::cell::StaticCell;
use m3::cfg;
use m3::com::MemGate;
use m3::errors::Error;
use m3::goff;
use m3::kif::Perm;
use m3::rc::Rc;

use addrspace::ASMem;

static ZEROS: [u8; cfg::PAGE_SIZE] = [0u8; cfg::PAGE_SIZE];
static BUF: StaticCell<[u8; cfg::PAGE_SIZE]> = StaticCell::new([0u8; cfg::PAGE_SIZE]);

pub fn copy_block(src: &MemGate, dst: &MemGate, src_off: goff, size: goff) {
    let pages = size / cfg::PAGE_SIZE as goff;
    for i in 0..pages {
        src.read(BUF.get_mut(), src_off + i * cfg::PAGE_SIZE as goff)
            .unwrap();
        dst.write(BUF.get(), i * cfg::PAGE_SIZE as goff).unwrap();
    }
}

fn clear_block(mem: &MemGate, size: goff) {
    let pages = size / cfg::PAGE_SIZE as goff;
    for i in 0..pages {
        mem.write(&ZEROS, i * cfg::PAGE_SIZE as goff).unwrap();
    }
}

pub struct PhysMem {
    mgate: MemGate,
    owner_mem: Option<Rc<ASMem>>,
    owner_virt: goff,
}

impl PhysMem {
    pub fn new(
        owner_mem: Rc<ASMem>,
        owner_virt: goff,
        size: goff,
        perm: Perm,
    ) -> Result<Self, Error> {
        Ok(PhysMem {
            mgate: MemGate::new(size as usize, perm)?,
            owner_mem: Some(owner_mem),
            owner_virt,
        })
    }

    pub fn new_with_mem(owner_mem: Rc<ASMem>, owner_virt: goff, mem: MemGate) -> Self {
        PhysMem {
            mgate: mem,
            owner_mem: Some(owner_mem),
            owner_virt,
        }
    }

    pub fn new_bind(owner_mem: Rc<ASMem>, owner_virt: goff, sel: Selector) -> Self {
        PhysMem {
            mgate: MemGate::new_bind(sel),
            owner_mem: Some(owner_mem),
            owner_virt,
        }
    }

    pub fn gate(&self) -> &MemGate {
        &self.mgate
    }

    pub fn replace_gate(&mut self, mem: MemGate) -> MemGate {
        mem::replace(&mut self.mgate, mem)
    }

    pub fn owner_mem(&self) -> Option<&Rc<ASMem>> {
        self.owner_mem.as_ref()
    }

    pub fn owner_virt(&self) -> goff {
        self.owner_virt
    }

    pub fn set_owner(&mut self, mem: Rc<ASMem>, virt: goff) {
        self.owner_mem = Some(mem);
        self.owner_virt = virt;
    }

    pub fn remove_owner(&mut self) {
        self.owner_mem = None;
    }

    pub fn clear(&self, size: goff) {
        clear_block(&self.mgate, size);
    }
}