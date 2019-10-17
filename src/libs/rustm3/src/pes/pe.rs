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

use cap::{CapFlags, Capability, Selector};
use core::fmt;
use errors::Error;
use kif::PEDesc;
use pes::VPE;
use rc::Rc;

pub struct PE {
    cap: Capability,
    desc: PEDesc,
}

impl PE {
    pub fn new(desc: PEDesc) -> Result<Rc<Self>, Error> {
        let sel = VPE::cur().alloc_sel();
        let ndesc = VPE::cur().resmng().alloc_pe(sel, desc)?;
        Ok(Rc::new(PE {
            cap: Capability::new(sel, CapFlags::empty()),
            desc: ndesc,
        }))
    }

    pub fn new_bind(desc: PEDesc, sel: Selector) -> Self {
        PE {
            cap: Capability::new(sel, CapFlags::KEEP_CAP),
            desc,
        }
    }

    pub fn sel(&self) -> Selector {
        self.cap.sel()
    }

    pub fn desc(&self) -> PEDesc {
        self.desc
    }

    pub(crate) fn set_sel(&self, sel: Selector) {
        self.cap.set_sel(sel);
    }
}

impl Drop for PE {
    fn drop(&mut self) {
        if !self.cap.flags().contains(CapFlags::KEEP_CAP) {
            VPE::cur().resmng().free_pe(self.sel()).ok();
        }
        // don't revoke cap
        self.cap.set_flags(CapFlags::KEEP_CAP);
    }
}

impl fmt::Debug for PE {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "PE[sel: {}, desc: {:?}]", self.sel(), self.desc())
    }
}