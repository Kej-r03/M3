/*
 * Copyright (C) 2018 Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
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

#![no_std]
#![feature(core_intrinsics)]

mod bboxlist;
mod bdlist;
mod bipc;
mod bmemmap;
mod bmgate;
mod bpipe;
mod bregfile;
mod bstream;
mod bsyscall;
#[cfg(not(target_vendor = "host"))]
mod btilemux;
mod btreap;
mod btreemap;

use m3::cell::StaticCell;
use m3::test::WvTester;
use m3::{println, wv_run_suite};

// TODO that's hacky, but the only alternative I can see is to pass the WvTester to every single
// test case and every single wv_assert_* call, which is quite inconvenient.
static FAILED: StaticCell<u32> = StaticCell::new(0);

extern "C" fn wvtest_failed() {
    FAILED.set(FAILED.get() + 1);
}

struct MyTester {}

impl WvTester for MyTester {
    fn run_suite(&mut self, name: &str, f: &dyn Fn(&mut dyn WvTester)) {
        println!("Running benchmark suite {} ...\n", name);
        f(self);
        println!();
    }

    fn run_test(&mut self, name: &str, file: &str, f: &dyn Fn()) {
        println!("Testing \"{}\" in {}:", name, file);
        f();
        println!();
    }
}

#[no_mangle]
pub fn main() -> i32 {
    let mut tester = MyTester {};
    wv_run_suite!(tester, bboxlist::run);
    wv_run_suite!(tester, bdlist::run);
    wv_run_suite!(tester, bmemmap::run);
    wv_run_suite!(tester, bmgate::run);
    wv_run_suite!(tester, bipc::run);
    #[cfg(not(target_vendor = "host"))]
    wv_run_suite!(tester, btilemux::run);
    wv_run_suite!(tester, bpipe::run);
    wv_run_suite!(tester, bregfile::run);
    wv_run_suite!(tester, bstream::run);
    wv_run_suite!(tester, bsyscall::run);
    wv_run_suite!(tester, btreap::run);
    wv_run_suite!(tester, btreemap::run);

    if FAILED.get() > 0 {
        println!("\x1B[1;31m{} tests failed\x1B[0;m", FAILED.get());
    }
    else {
        println!("\x1B[1;32mAll tests successful!\x1B[0;m");
    }
    0
}
