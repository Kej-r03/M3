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

//! Contains the modules for serial output, logging, etc.

pub mod log;
mod rdwr;
mod serial;

pub use self::rdwr::{read_object, Read, Write};
pub use self::serial::Serial;
use arch;
use util;

/// Macro for logging (includes a trailing newline)
///
/// The arguments are printed if `$type` is enabled.
///
/// # Examples
///
/// ```
/// log!(SYSC, "my log entry: {}, {}", 1, "test");
/// ```
#[macro_export]
macro_rules! log {
    ($type:expr, $fmt:expr)                   => (
        llog!(@log_impl $type, concat!($fmt, "\n"))
    );

    ($type:expr, $fmt:expr, $($arg:tt)*)      => (
        llog!(@log_impl $type, concat!($fmt, "\n"), $($arg)*)
    );
}

/// Macro for library-internal logging (includes a trailing newline)
///
/// The arguments are printed if `$crate::io::log::$type` is enabled.
#[macro_export]
macro_rules! llog {
    ($type:tt, $fmt:expr)                   => (
        llog!(@log_impl $crate::io::log::$type, concat!($fmt, "\n"))
    );

    ($type:tt, $fmt:expr, $($arg:tt)*)      => (
        llog!(@log_impl $crate::io::log::$type, concat!($fmt, "\n"), $($arg)*)
    );

    (@log_impl $type:expr, $($args:tt)*)    => ({
        if $type {
            #[allow(unused_imports)]
            use $crate::io::Write;
            if let Some(l) = $crate::io::log::Log::get() {
                l.write_fmt(format_args!($($args)*)).unwrap();
            }
        }
    });
}

#[no_mangle]
pub extern "C" fn init_rust_io(pe_id: u32, name: *const i8) {
    init(pe_id, unsafe { util::cstr_to_str(name) });
}

/// Initializes the I/O module
pub fn init(pe_id: u32, name: &str) {
    arch::serial::init();
    log::init(pe_id, name);
}

/// Reinitializes the I/O module (for VPE::run)
pub fn reinit(pe_id: u32, name: &str) {
    log::reinit(pe_id, name);
}