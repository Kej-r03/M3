/*
 * Copyright (C) 2018 Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2019-2020 Nils Asmussen, Barkhausen Institut
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

use core::cmp;
use m3::col::{ToString, Vec};
use m3::test::WvTester;
use m3::vfs::VFS;
use m3::{wv_assert_eq, wv_assert_ok, wv_run_test};

pub fn run(t: &mut dyn WvTester) {
    wv_run_test!(t, list_dir);
}

fn list_dir(t: &mut dyn WvTester) {
    // read a dir with known content
    let dirname = "/largedir";
    let mut vec = Vec::new();
    for e in wv_assert_ok!(VFS::read_dir(dirname)) {
        vec.push(e);
    }
    wv_assert_eq!(t, vec.len(), 82);

    // sort the entries; keep "." and ".." at the front
    vec.sort_unstable_by(|a, b| {
        let aname = a.file_name();
        let bname = b.file_name();
        let aspec = aname == "." || aname == "..";
        let bspec = bname == "." || bname == "..";
        match (aspec, bspec) {
            (true, true) => aname.cmp(bname),
            (true, false) => cmp::Ordering::Less,
            (false, true) => cmp::Ordering::Greater,
            (false, false) => {
                // cut off ".txt"
                let anum = aname[0..aname.len() - 4].parse::<i32>().unwrap();
                let bnum = bname[0..bname.len() - 4].parse::<i32>().unwrap();
                anum.cmp(&bnum)
            },
        }
    });

    // now check file names
    wv_assert_eq!(t, vec[0].file_name(), ".");
    wv_assert_eq!(t, vec[1].file_name(), "..");
    for i in 0..80 {
        wv_assert_eq!(t, i.to_string() + ".txt", vec[i + 2].file_name());
    }
}
