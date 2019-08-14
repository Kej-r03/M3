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

use cap::{CapFlags, Selector};
use com::gate::Gate;
use com::stream::GateIStream;
use com::RecvGate;
use core::fmt;
use dtu;
use errors::Error;
use kif::INVALID_SEL;
use syscalls;
use util;
use vpe;

/// A send gate (`SendGate`) can send message via DTU to an associated `RecvGate`.
pub struct SendGate {
    gate: Gate,
}

/// The arguments for [`SendGate`] creations.
pub struct SGateArgs {
    rgate_sel: Selector,
    label: dtu::Label,
    credits: u64,
    sel: Selector,
    flags: CapFlags,
}

impl SGateArgs {
    /// Creates a new `SGateArgs` to send messages to `rgate` with default settings.
    pub fn new(rgate: &RecvGate) -> Self {
        SGateArgs {
            rgate_sel: rgate.sel(),
            label: 0,
            credits: 0,
            sel: INVALID_SEL,
            flags: CapFlags::empty(),
        }
    }

    /// Sets the credits to `credits`.
    pub fn credits(mut self, credits: u64) -> Self {
        self.credits = credits;
        self
    }

    /// Sets the label to `label`.
    pub fn label(mut self, label: dtu::Label) -> Self {
        self.label = label;
        self
    }

    /// Sets the capability selector to use for the [`SendGate`]. Otherwise and by default,
    /// [`vpe::VPE::alloc_sel`] will be used.
    pub fn sel(mut self, sel: Selector) -> Self {
        self.sel = sel;
        self
    }
}

impl SendGate {
    /// Creates a new `SendGate` that can send messages to `rgate`.
    pub fn new(rgate: &RecvGate) -> Result<Self, Error> {
        Self::new_with(SGateArgs::new(rgate))
    }

    /// Creates a new `SendGate` with given arguments.
    pub fn new_with(args: SGateArgs) -> Result<Self, Error> {
        let sel = if args.sel == INVALID_SEL {
            vpe::VPE::cur().alloc_sel()
        }
        else {
            args.sel
        };

        syscalls::create_sgate(sel, args.rgate_sel, args.label, args.credits)?;
        Ok(SendGate {
            gate: Gate::new(sel, args.flags),
        })
    }

    /// Binds a new `SendGate` to the given capability selector.
    pub fn new_bind(sel: Selector) -> Self {
        SendGate {
            gate: Gate::new(sel, CapFlags::KEEP_CAP),
        }
    }

    /// Returns the capability selector.
    pub fn sel(&self) -> Selector {
        self.gate.sel()
    }

    /// Returns the endpoint of the gate. If the gate is not activated, `None` is returned.
    pub fn ep(&self) -> Option<dtu::EpId> {
        self.gate.ep()
    }

    /// Rebinds this `SendGate` to the given capability selector.
    pub fn rebind(&mut self, sel: Selector) -> Result<(), Error> {
        self.gate.rebind(sel)
    }

    /// Activates this `SendGate` on the given endpoint.
    pub fn activate_for(&self, ep: Selector) -> Result<(), Error> {
        syscalls::activate(ep, self.sel(), 0)
    }

    /// Sends `msg` to the associated [`RecvGate`] and uses `reply_gate` to receive a reply.
    #[inline(always)]
    pub fn send<T>(&self, msg: &[T], reply_gate: &RecvGate) -> Result<(), Error> {
        self.send_bytes(
            msg.as_ptr() as *const u8,
            msg.len() * util::size_of::<T>(),
            reply_gate,
            0,
        )
    }

    /// Sends `msg` to the associated [`RecvGate`], uses `reply_gate` to receive the reply, and
    /// lets the communication partner use the label `rlabel` for the reply.
    pub fn send_with_rlabel<T>(
        &self,
        msg: &[T],
        reply_gate: &RecvGate,
        rlabel: dtu::Label,
    ) -> Result<(), Error> {
        self.send_bytes(
            msg.as_ptr() as *const u8,
            msg.len() * util::size_of::<T>(),
            reply_gate,
            rlabel,
        )
    }

    /// Sends `msg` of length `len` to the associated [`RecvGate`] and receives the reply from the
    /// set reply gate. Returns the received reply.
    pub fn call<'r, T>(
        &self,
        msg: &[T],
        reply_gate: &'r RecvGate,
    ) -> Result<GateIStream<'r>, Error> {
        let ep = self.gate.activate()?;
        dtu::DTUIf::call(
            ep,
            msg.as_ptr() as *const u8,
            msg.len() * util::size_of::<T>(),
            reply_gate.ep().unwrap(),
        )
        .map(|m| GateIStream::new(m, reply_gate))
    }

    #[inline(always)]
    fn send_bytes(
        &self,
        msg: *const u8,
        size: usize,
        reply_gate: &RecvGate,
        rlabel: dtu::Label,
    ) -> Result<(), Error> {
        let ep = self.gate.activate()?;
        dtu::DTUIf::send(ep, msg, size, rlabel, reply_gate.ep().unwrap())
    }
}

impl fmt::Debug for SendGate {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "SendGate[sel: {}, ep: {:?}]", self.sel(), self.gate.ep())
    }
}
