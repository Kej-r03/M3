/*
 * Copyright (C) 2021, Tendsin Mende <tendsin.mende@mailbox.tu-dresden.de>
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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

use core::cmp;

use m3::cap::Selector;
use m3::cell::RefCell;
use m3::col::{Vec, VecDeque};
use m3::com::{GateIStream, RecvGate, SendGate};
use m3::errors::{Code, Error};
use m3::net::{event, IpAddr, NetEvent, NetEventChannel, NetEventType, Port, Sd, SocketType};
use m3::rc::Rc;
use m3::server::CapExchange;
use m3::session::ServerSession;
use m3::tcu;
use m3::vfs::OpenFlags;
use m3::{log, reply_vmsg, vec};

use smoltcp::socket::SocketSet;

use crate::sess::file::FileSession;
use crate::smoltcpif::socket::{to_m3_addr, SendNetEvent, Socket};

pub const MAX_INCOMING_BATCH_SIZE: usize = 4;

pub const MAX_SOCKETS: usize = 16;

pub struct SocketSession {
    sgate: Option<SendGate>,
    rgate: Rc<RecvGate>,
    server_session: ServerSession,
    sockets: Vec<Option<Rc<RefCell<Socket>>>>, // All sockets registered to this socket session.
    /// Used to communicate with the client
    channel: Option<Rc<NetEventChannel>>,
    send_queue: VecDeque<NetEvent>,
}

impl Drop for SocketSession {
    fn drop(&mut self) {
        for i in 0..MAX_SOCKETS {
            self.remove_socket(i)
        }
    }
}

impl SocketSession {
    pub fn new(_crt: usize, server_session: ServerSession, rgate: Rc<RecvGate>) -> Self {
        SocketSession {
            sgate: None,
            rgate,
            server_session,
            sockets: vec![None; MAX_SOCKETS], // TODO allocate correct amount up front?
            channel: None,
            send_queue: VecDeque::new(),
        }
    }

    pub fn obtain(
        &mut self,
        crt: usize,
        srv_sel: Selector,
        xchg: &mut CapExchange,
    ) -> Result<(), Error> {
        log!(
            crate::LOG_DEF,
            "SocketSession::obtain with {} in caps",
            xchg.in_caps()
        );

        if xchg.in_caps() == 1 {
            self.get_sgate(xchg)
        }
        // TODO we only need 2
        else if xchg.in_caps() == 3 {
            // establish a connection with the network manager in that session
            self.connect_nm(xchg)
        }
        else if xchg.in_caps() == 2 {
            self.open_file(crt, srv_sel, xchg)
        }
        else {
            Err(Error::new(Code::InvArgs))
        }
    }

    /// Creates a SendGate that is used to send data to this socket session.
    /// keeps the Sendgate alive and sends back the selector that needs to be binded to.
    fn get_sgate(&mut self, xchg: &mut CapExchange) -> Result<(), Error> {
        if self.sgate.is_some() {
            return Err(Error::new(Code::InvArgs));
        }

        let label = self.server_session.ident() as tcu::Label;

        log!(
            crate::LOG_DEF,
            "SocketSession::get_sgate with label={}",
            label
        );

        self.sgate = Some(SendGate::new_with(
            m3::com::SGateArgs::new(&self.rgate).label(label).credits(1),
        )?);

        xchg.out_caps(m3::kif::CapRngDesc::new(
            m3::kif::CapType::OBJECT,
            self.sgate.as_ref().unwrap().sel(),
            1,
        ));
        Ok(())
    }

    fn connect_nm(&mut self, xchg: &mut CapExchange) -> Result<(), Error> {
        log!(crate::LOG_DEF, "Establishing channel for socket session");

        // 2 caps for us, 2 for the client
        let caps = m3::pes::VPE::cur().alloc_sels(4);
        self.channel = Some(NetEventChannel::new_server(caps)?);

        // Send capabilities back to caller so it can connect to the created gates
        xchg.out_caps(m3::kif::CapRngDesc::new(
            m3::kif::CapType::OBJECT,
            caps + 2,
            2,
        ));

        Ok(())
    }

    fn open_file(
        &mut self,
        crt: usize,
        srv_sel: Selector,
        xchg: &mut CapExchange,
    ) -> Result<(), Error> {
        let sd = xchg.in_args().pop::<Sd>().expect("Failed to get sd");
        let mode = xchg.in_args().pop::<u32>().expect("Failed to get mode");
        let rmemsize = xchg
            .in_args()
            .pop::<usize>()
            .expect("Failed to get rmemsize");
        let smemsize = xchg
            .in_args()
            .pop::<usize>()
            .expect("Failed to get smemsize");

        log!(
            crate::LOG_DEF,
            "socket_session::open_file(sd={}, mode={}, rmemsize={}, smemsize={})",
            sd,
            mode,
            rmemsize,
            smemsize
        );
        // Create socket for file
        if let Some(socket) = self.get_socket(sd) {
            if (mode & OpenFlags::RW.bits()) == 0 {
                log!(crate::LOG_DEF, "open_file failed: invalid mode");
                return Err(Error::new(Code::InvArgs));
            }

            if (socket.borrow().recv_file().is_some() && ((mode & OpenFlags::R.bits()) > 0))
                || (socket.borrow().send_file().is_some() && ((mode & OpenFlags::W.bits()) > 0))
            {
                log!(
                    crate::LOG_DEF,
                    "open_file failed: socket already has a file session attached"
                );
                return Err(Error::new(Code::InvArgs));
            }
            let file = FileSession::new(
                crt,
                srv_sel,
                socket.clone(),
                &self.rgate,
                mode,
                rmemsize,
                smemsize,
            )?;
            if file.borrow().is_recv() {
                socket.borrow_mut().set_recv_file(Some(file.clone()));
            }
            if file.borrow().is_send() {
                socket.borrow_mut().set_send_file(Some(file.clone()));
            }

            xchg.out_caps(file.borrow().caps());

            log!(
                crate::LOG_DEF,
                "open_file: {}@{}{}",
                sd,
                if file.borrow().is_recv() { "r" } else { "" },
                if file.borrow().is_send() { "s" } else { "" }
            );
            Ok(())
        }
        else {
            log!(
                crate::LOG_DEF,
                "open_file failed: invalud socket descriptor"
            );
            Err(Error::new(Code::InvArgs))
        }
    }

    fn get_socket(&self, sd: Sd) -> Option<Rc<RefCell<Socket>>> {
        if let Some(s) = self.sockets.get(sd) {
            s.clone()
        }
        else {
            None
        }
    }

    fn add_socket(
        &mut self,
        ty: SocketType,
        protocol: u8,
        socket_set: &mut SocketSet<'static>,
    ) -> Result<Sd, Error> {
        for (i, s) in self.sockets.iter_mut().enumerate() {
            if s.is_none() {
                *s = Some(Rc::new(RefCell::new(Socket::new(
                    i, ty, protocol, socket_set,
                )?)));
                return Ok(i);
            }
        }
        Err(Error::new(Code::NoSpace))
    }

    fn remove_socket(&mut self, sd: Sd) {
        // if there is a socket, delete it.
        if self.sockets.get(sd).is_some() {
            self.sockets[sd] = None;
        }
    }

    pub fn create(
        &mut self,
        is: &mut GateIStream,
        socket_set: &mut SocketSet<'static>,
    ) -> Result<(), Error> {
        let ty_id: usize = is.pop()?;
        let ty = SocketType::from_usize(ty_id);
        let protocol: u8 = is.pop()?;

        log!(
            crate::LOG_DEF,
            "net::create(type={:?}, protocol={})",
            ty,
            protocol
        );

        // Create the abstract socket from some created socket instance
        let sd = match self.add_socket(ty, protocol, socket_set) {
            Ok(sd) => sd,
            Err(_e) => {
                // TODO release socket
                log!(
                    crate::LOG_DEF,
                    "create failed: maximum number of sockets reached"
                );
                return Err(Error::new(Code::NoSpace));
            },
        };

        log!(crate::LOG_DEF, "-> sd={}", sd);
        reply_vmsg!(is, 0 as u32, sd)
    }

    pub fn bind(
        &mut self,
        is: &mut GateIStream,
        socket_set: &mut SocketSet<'static>,
    ) -> Result<(), Error> {
        let sd: Sd = is.pop()?;
        let addr = IpAddr(is.pop::<u32>()?);
        let port: Port = is.pop()?;

        log!(
            crate::LOG_DEF,
            "net::bind(sd={}, addr={}, port={})",
            sd,
            addr,
            port
        );

        if let Some(sock) = self.get_socket(sd) {
            sock.borrow_mut().bind(addr, port, socket_set)?;
            reply_vmsg!(is, Code::None as i32)
        }
        else {
            log!(crate::LOG_DEF, "bind failed, invalid socket descriptor");
            Err(Error::new(Code::InvArgs))
        }
    }

    pub fn listen(
        &mut self,
        is: &mut GateIStream,
        socket_set: &mut SocketSet<'static>,
    ) -> Result<(), Error> {
        let sd: Sd = is.pop()?;
        let addr = IpAddr(is.pop::<u32>()?);
        let port: Port = is.pop()?;

        log!(
            crate::LOG_DEF,
            "net::listen(sd={}, addr={}, port={})",
            sd,
            addr,
            port
        );

        if let Some(socket) = self.get_socket(sd) {
            socket.borrow_mut().listen(socket_set, addr, port)?;
            reply_vmsg!(is, Code::None as i32)
        }
        else {
            log!(crate::LOG_DEF, "listen failed: invalud socket descriptor");
            Err(Error::new(Code::InvArgs))
        }
    }

    pub fn connect(
        &mut self,
        is: &mut GateIStream,
        socket_set: &mut SocketSet<'static>,
    ) -> Result<(), Error> {
        let sd: Sd = is.pop()?;
        let remote_addr = IpAddr(is.pop::<u32>()?);
        let remote_port: Port = is.pop()?;
        let local_port: Port = is.pop()?;

        log!(
            crate::LOG_DEF,
            "net::connect(sd={}, remote={}:{}, local={})",
            sd,
            remote_addr,
            remote_port,
            local_port
        );

        if let Some(sock) = self.get_socket(sd) {
            // TODO verify that the bigEndian is indeed the correct byte order
            sock.borrow_mut()
                .connect(remote_addr, remote_port, local_port, socket_set)?;
            reply_vmsg!(is, Code::None as i32) // all went good
        }
        else {
            log!(crate::LOG_DEF, "connect failed: invalid socket descriptor");
            Err(Error::new(Code::InvArgs))
        }
    }

    pub fn abort(
        &mut self,
        is: &mut GateIStream,
        socket_set: &mut SocketSet<'static>,
    ) -> Result<(), Error> {
        let sd: Sd = is.pop()?;
        let remove: bool = is.pop()?;
        log!(crate::LOG_DEF, "net::abort(sd={}, remove={})", sd, remove);

        if let Some(socket) = self.get_socket(sd) {
            socket.borrow_mut().abort(socket_set);
            if remove {
                self.remove_socket(sd);
            }
            reply_vmsg!(is, Code::None as i32)
        }
        else {
            log!(crate::LOG_DEF, "close failed: invalid socket descriptor");
            Err(Error::new(Code::InvArgs))
        }
    }

    pub fn process_incoming(&mut self, socket_set: &mut SocketSet<'static>) -> bool {
        if self.channel.is_none() {
            // Cannot send yet since the channel is not active.
            return false;
        }

        self.channel.as_ref().unwrap().fetch_replies();

        let mut num_sent = 0;

        while let Some(event) = self.send_queue.pop_front() {
            num_sent += 1;

            log!(crate::LOG_DEF, "re-processing packet from queue");
            if !self.process_event(socket_set, event) || num_sent > MAX_INCOMING_BATCH_SIZE {
                return true;
            }
        }

        // receive everything in the channel
        while let Some(event) = self.channel.as_ref().unwrap().receive_event() {
            num_sent += 1;

            if !self.process_event(socket_set, event) || num_sent > MAX_INCOMING_BATCH_SIZE {
                return true;
            }
        }

        false
    }

    fn process_event(&mut self, socket_set: &mut SocketSet<'static>, event: NetEvent) -> bool {
        match event.msg_type() {
            NetEventType::DATA => {
                let data = event.msg::<event::DataMessage>();
                if let Some(socket) = self.get_socket(data.sd as Sd) {
                    log!(crate::LOG_DEF, "got packet of {} bytes to send", data.size);

                    let succeeded = socket.borrow_mut().send(
                        &data.data[0..data.size as usize],
                        IpAddr(data.addr as u32),
                        data.port as Port,
                        socket_set,
                    );
                    if succeeded.is_err() {
                        // if no buffers are available, remember the event for later
                        log!(
                            crate::LOG_DEF,
                            "no buffer space, delaying send of {} bytes",
                            data.size
                        );
                        self.send_queue.push_back(event);
                    }
                }
            },

            NetEventType::CLOSE_REQ => {
                let req = event.msg::<event::CloseReqMessage>();
                log!(crate::LOG_DEF, "net::close(sd={})", req.sd);

                if let Some(socket) = self.get_socket(req.sd as Sd) {
                    // ignore error
                    socket.borrow_mut().close(socket_set).ok();
                }
            },

            _ => panic!("Unexpected message from client"),
        }
        true
    }

    pub fn process_outgoing(&mut self, socket_set: &mut SocketSet<'static>) {
        if self.channel.is_none() {
            // Cannot receive yet since the channel is not active.
            return;
        }

        let chan = self.channel.as_ref().unwrap();

        chan.fetch_replies();

        // iterate over all sockets and try to receive
        for socket in self.sockets.iter() {
            if let Some(socket) = socket {
                let socket_sd = socket.borrow().sd();

                // if we don't have credits anymore to send events, stop here. we'll get a reply
                // to one of our earlier events and get credits back with this, so that we'll wake
                // up from a potential sleep and call receive again.
                if !chan.can_send().unwrap() {
                    break;
                }

                if let Some(event) = socket.borrow_mut().fetch_event(socket_set) {
                    log!(crate::LOG_DEF, "Socket got event {:?}", event);
                    match event {
                        SendNetEvent::Connected(e) => chan.send_event(e).unwrap(),
                        SendNetEvent::Closed(e) => {
                            // remove all pending events from queue
                            self.send_queue.retain(|e| e.sd() != socket_sd);
                            chan.send_event(e).unwrap()
                        },
                        SendNetEvent::CloseReq(e) => chan.send_event(e).unwrap(),
                    }
                }

                socket.borrow_mut().receive(socket_set, |data, addr| {
                    let (ip, port) = to_m3_addr(addr);
                    log!(
                        crate::LOG_DEF,
                        "Received package with size={} from {}:{}",
                        data.len(),
                        ip,
                        port
                    );

                    let amount = cmp::min(event::MTU, data.len());
                    chan.send_data(socket_sd, ip, port, amount, |buf| {
                        buf[0..amount].copy_from_slice(&data[0..amount]);
                    })
                    .unwrap();
                    amount
                });
            }
        }
    }
}
