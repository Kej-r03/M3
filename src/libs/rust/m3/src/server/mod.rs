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

//! Contains server-related abstractions.

mod reqhdl;
#[allow(clippy::module_inception)]
mod server;
mod sesscon;

pub use self::reqhdl::{RequestHandler, DEF_MAX_CLIENTS, DEF_MSG_SIZE};
pub use self::server::{CapExchange, Handler, Server};
pub use self::sesscon::{SessId, SessionContainer};

use errors::Error;
use tcu::TCUIf;

/// Executes the server loop, calling `func` in every iteration.
pub fn server_loop<F: FnMut() -> Result<(), Error>>(mut func: F) -> Result<(), Error> {
    loop {
        TCUIf::sleep().ok();

        func()?;
    }
}

#[macro_export]
macro_rules! handle_ctrl_chan {
    ($serv:expr, $hdl:expr) => {
        match $serv.handle_ctrl_chan($hdl) {
            Ok(true) => Err(Error::new(Code::EndOfFile)),
            Ok(_) => Ok(()),
            Err(e) => {
                llog!(SERV, "Control channel request failed: {:?}", e);
                Ok(())
            }
        }
    };
}
