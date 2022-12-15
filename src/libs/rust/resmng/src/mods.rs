/*
 * Copyright (C) 2019-2022 Nils Asmussen, Barkhausen Institut
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

use m3::cell::LazyReadOnlyCell;
use m3::col::{String, ToString, Vec};
use m3::com::MemGate;
use m3::goff;
use m3::kif::boot;
use m3::mem::GlobAddr;

use crate::subsys::Subsystem;

pub struct Mod {
    addr: GlobAddr,
    size: goff,
    name: String,
    mgate: MemGate,
}

impl Mod {
    pub fn addr(&self) -> GlobAddr {
        self.addr
    }

    pub fn size(&self) -> goff {
        self.size
    }

    pub fn name(&self) -> &String {
        &self.name
    }

    pub fn memory(&self) -> &MemGate {
        &self.mgate
    }
}

pub struct ModManager {
    mods: Vec<Mod>,
}

static MNG: LazyReadOnlyCell<ModManager> = LazyReadOnlyCell::default();

pub fn create(mods: &Vec<boot::Mod>) {
    let mut mmods = Vec::new();
    for (idx, m) in mods.iter().enumerate() {
        mmods.push(Mod {
            addr: m.addr(),
            size: m.size,
            name: m.name().to_string(),
            mgate: Subsystem::get_mod(idx),
        })
    }
    MNG.set(ModManager { mods: mmods });
}

pub fn get() -> &'static ModManager {
    MNG.get()
}

impl ModManager {
    pub const fn new() -> Self {
        ModManager { mods: Vec::new() }
    }

    pub fn find(&self, name: &str) -> Option<&Mod> {
        self.mods.iter().find(|bm| bm.name == name)
    }
}