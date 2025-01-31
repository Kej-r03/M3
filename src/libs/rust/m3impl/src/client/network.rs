/*
 * Copyright (C) 2021-2022 Nils Asmussen, Barkhausen Institut
 * Copyright (C) 2021, Tendsin Mende <tendsin.mende@mailbox.tu-dresden.de>
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

use crate::client::ClientSession;
use crate::com::{opcodes, RecvGate, SendGate};
use crate::errors::Error;
use crate::net::{BaseSocket, Endpoint, IpAddr, NetEventChannel, Port, Sd, SocketArgs, SocketType};
use crate::rc::Rc;

/// Represents a session at the network server, allowing to create and use sockets
///
/// To exchange events and data with the server, the [`NetEventChannel`] is used, which allows to
/// send and receive multiple messages. Events are used to receive connected or closed events from
/// the server and to send close requests to the server. Transmitted and received data is exchanged
/// via the [`NetEventChannel`] in both directions.
pub struct Network {
    sess: ClientSession,
    sgate: SendGate,
}

impl Network {
    /// Creates a new instance for `service`
    pub fn new(service: &str) -> Result<Rc<Self>, Error> {
        let sess = ClientSession::new(service)?;
        let sgate = sess.connect()?;
        Ok(Rc::new(Network { sess, sgate }))
    }

    /// Returns the local IP address
    pub fn ip_addr(&self) -> Result<IpAddr, Error> {
        let mut reply = send_recv_res!(&self.sgate, RecvGate::def(), opcodes::Net::GetIP)?;
        let addr = IpAddr(reply.pop::<u32>()?);
        Ok(addr)
    }

    pub(crate) fn create(
        &self,
        ty: SocketType,
        protocol: Option<u8>,
        args: &SocketArgs,
    ) -> Result<BaseSocket, Error> {
        let mut sd = 0;
        let crd = self.sess.obtain(
            2,
            |sink| {
                sink.push(opcodes::Net::Create);
                sink.push(ty);
                sink.push(protocol.unwrap_or(0));
                sink.push(args.rbuf_size);
                sink.push(args.rbuf_slots);
                sink.push(args.sbuf_size);
                sink.push(args.sbuf_slots);
            },
            |source| {
                sd = source.pop()?;
                Ok(())
            },
        )?;

        let chan = NetEventChannel::new_client(crd.start())?;
        Ok(BaseSocket::new(sd, ty, chan))
    }

    pub(crate) fn nameserver(&self) -> Result<IpAddr, Error> {
        let mut reply = send_recv_res!(&self.sgate, RecvGate::def(), opcodes::Net::GetNameSrv)?;
        let addr = IpAddr(reply.pop::<u32>()?);
        Ok(addr)
    }

    pub(crate) fn bind(&self, sd: Sd, port: Port) -> Result<(IpAddr, Port), Error> {
        let mut reply = send_recv_res!(&self.sgate, RecvGate::def(), opcodes::Net::Bind, sd, port)?;
        let addr = IpAddr(reply.pop::<u32>()?);
        let port = reply.pop::<Port>()?;
        Ok((addr, port))
    }

    pub(crate) fn listen(&self, sd: Sd, port: Port) -> Result<IpAddr, Error> {
        let mut reply =
            send_recv_res!(&self.sgate, RecvGate::def(), opcodes::Net::Listen, sd, port)?;
        let addr = IpAddr(reply.pop::<u32>()?);
        Ok(addr)
    }

    pub(crate) fn connect(&self, sd: Sd, endpoint: Endpoint) -> Result<Endpoint, Error> {
        let mut reply = send_recv_res!(
            &self.sgate,
            RecvGate::def(),
            opcodes::Net::Connect,
            sd,
            endpoint.addr.0,
            endpoint.port
        )?;
        let addr = reply.pop::<u32>()?;
        let port = reply.pop::<Port>()?;
        Ok(Endpoint::new(IpAddr(addr), port))
    }

    pub(crate) fn abort(&self, sd: Sd, remove: bool) -> Result<(), Error> {
        send_recv_res!(
            &self.sgate,
            RecvGate::def(),
            opcodes::Net::Abort,
            sd,
            remove
        )
        .map(|_| ())
    }
}
