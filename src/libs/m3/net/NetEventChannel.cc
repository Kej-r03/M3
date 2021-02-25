/*
 * Copyright (C) 2018, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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

#include <base/log/Lib.h>
#include <m3/net/NetEventChannel.h>
#include <m3/pes/VPE.h>

namespace m3 {

void NetEventChannel::prepare_caps(capsel_t caps, size_t size) {
    RecvGate rgate_srv(RecvGate::create(caps + 0, nextlog2<MSG_BUF_SIZE>::val,
            nextlog2<MSG_SIZE>::val, RecvGate::KEEP_CAP));
    RecvGate rgate_cli(RecvGate::create(caps + 3, nextlog2<MSG_BUF_SIZE>::val,
                nextlog2<MSG_SIZE>::val, RecvGate::KEEP_CAP));
    SendGate sgate_srv(SendGate::create(&rgate_cli, SendGateArgs().reply_gate(&rgate_srv)
                                                                  .sel(caps + 1)
                                                                  .flags(MemGate::KEEP_CAP)));
    SendGate sgate_cli(SendGate::create(&rgate_srv, SendGateArgs().reply_gate(&rgate_cli)
                                                                  .sel(caps + 4)
                                                                  .flags(MemGate::KEEP_CAP)
                                                                  .credits(MSG_CREDITS)));
    MemGate mem_srv(MemGate::create_global(2 * size, MemGate::RW, caps + 2, MemGate::KEEP_CAP));
    MemGate mem_cli(mem_srv.derive_for(VPE::self().sel(), caps + 5, 0, 2 * size, mem_srv.RW, MemGate::KEEP_CAP));
}

NetEventChannel::NetEventChannel(capsel_t caps, bool ret_credits) noexcept
    : _ret_credits(ret_credits),
      _rgate(RecvGate::bind(caps + 0, nextlog2<MSG_BUF_SIZE>::val, nextlog2<MSG_SIZE>::val)),
      _rplgate(RecvGate::create(nextlog2<REPLY_BUF_SIZE>::val, nextlog2<REPLY_SIZE>::val)),
      _sgate(SendGate::bind(caps + 1, &_rplgate)),
      _workitem(nullptr),_credit_event(0), _waiting_credit(0) {
}

void NetEventChannel::data_transfer(int sd, size_t pos, size_t size) {
    LLOG(NET, "NetEventChannel::data_transfer(sd=" << sd << ", pos=" << pos << ", size=" << size << ")");
    MsgBuf msg_buf;
    auto &msg = msg_buf.cast<NetEventChannel::DataTransferMessage>();
    msg.type = DataTransfer;
    msg.sd = sd;
    msg.pos = pos;
    msg.size = size;
    send_message(msg_buf);
}

void NetEventChannel::ack_data_transfer(int sd, size_t pos, size_t size) {
    LLOG(NET, "NetEventChannel::ack_data_transfer(sd=" << sd << ", pos=" << pos << ", size=" << size << ")");
    MsgBuf msg_buf;
    auto &msg = msg_buf.cast<NetEventChannel::AckDataTransferMessage>();
    msg.type = AckDataTransfer;
    msg.sd = sd;
    msg.pos = pos;
    msg.size = size;
    send_message(msg_buf);
}

bool NetEventChannel::inband_data_transfer(int sd, size_t size, std::function<void(uchar *)> cb_data) {
    LLOG(NET, "NetEventChannel::inband_data_transfer(sd=" << sd << ", size=" << size << ")");

    // make sure that the message does not contain a page boundary
    ALIGNED(2048) char msg_buf[2048];
    auto msg = reinterpret_cast<InbandDataTransferMessage*>(msg_buf);
    msg->type = InbandDataTransfer;
    msg->sd = sd;
    msg->size = size;
    // TODO: avoid copy
    cb_data(msg->data);

    fetch_replies();

    // TODO: Send via seperate send/receive gate?
    return _sgate.try_send_aligned(msg_buf, size + sizeof(InbandDataTransferMessage)) == Errors::NONE;
}

void NetEventChannel::socket_accept(int sd, int new_sd, IpAddr remote_addr, uint16_t remote_port) {
    LLOG(NET, "NetEventChannel::socket_accept(sd=" << sd << ", new_sd=" << new_sd << ")");
    MsgBuf msg_buf;
    auto &msg = msg_buf.cast<NetEventChannel::SocketAcceptMessage>();
    msg.type = SocketAccept;
    msg.sd = sd;
    msg.new_sd = new_sd;
    msg.remote_addr = remote_addr;
    msg.remote_port = remote_port;
    send_message(msg_buf);
}


void NetEventChannel::socket_connected(int sd) {
    LLOG(NET, "NetEventChannel::socket_connected(sd=" << sd << ")");
    MsgBuf msg_buf;
    auto &msg = msg_buf.cast<NetEventChannel::SocketConnectedMessage>();
    msg.type = SocketConnected;
    msg.sd = sd;
    send_message(msg_buf);
}

void NetEventChannel::socket_closed(int sd, Errors::Code cause) {
    LLOG(NET, "NetEventChannel::socket_closed(sd=" << sd << ")");
    MsgBuf msg_buf;
    auto &msg = msg_buf.cast<NetEventChannel::SocketClosedMessage>();
    msg.type = SocketClosed;
    msg.sd = sd;
    msg.cause = cause;
    send_message(msg_buf);
}

void NetEventChannel::send_message(const MsgBuf &msg) {
    _sgate.send(msg);
}

void NetEventChannel::start(WorkLoop *wl, evhandler_t evhandler, crdhandler_t crdhandler) {
    if(!_workitem) {
        _evhandler = evhandler;
        _crdhandler = crdhandler;
        _workitem = std::make_unique<EventWorkItem>(this);
        wl->add(_workitem.get(), false);
    }
}

void NetEventChannel::stop() {
    _workitem.reset();
}

NetEventChannel::Event NetEventChannel::recv_message() {
    return Event(_rgate.fetch(), this);
}

bool NetEventChannel::has_credits() noexcept {
    return _sgate.can_send();
}

void NetEventChannel::set_credit_event(event_t event) noexcept {
    _credit_event = event;
}

event_t NetEventChannel::get_credit_event() noexcept {
    return _credit_event;
}

void NetEventChannel::wait_for_credit() noexcept {
    _waiting_credit++;
}

bool NetEventChannel::has_events(evhandler_t &evhandler, crdhandler_t &crdhandler) {
    bool res = false;
    Event event = recv_message();
    if(event.is_present()) {
        evhandler(event);
        res = true;
    }

    fetch_replies();

    if(has_credits()) {
        if(crdhandler) {
            auto waiting_credit = _waiting_credit;
            _waiting_credit = 0;
            crdhandler(_credit_event, waiting_credit);
        }
        res = true;
    }
    return res;
}

void NetEventChannel::fetch_replies() {
    auto reply = _rplgate.fetch();
    while(reply != nullptr) {
        _rplgate.ack_msg(reply);
        reply = _rplgate.fetch();
    }
}

NetEventChannel::Event::Event() noexcept
    : _msg(nullptr),
       _channel(nullptr),
       _ack(false) {
}

NetEventChannel::Event::~Event() {
    try {
        finish();
    }
    catch(...) {
        // ignore
    }
}

NetEventChannel::Event::Event(NetEventChannel::Event&& e) noexcept
    : _msg(e._msg),
      _channel(e._channel),
      _ack(e._ack) {
    e._ack = false;
}

NetEventChannel::Event& NetEventChannel::Event::operator =(NetEventChannel::Event&& e) noexcept {
    _msg = e._msg;
    _channel = e._channel;
    _ack = e._ack;
    e._ack = false;
    return *this;
}

bool NetEventChannel::Event::is_present() noexcept {
    return _msg;
}

void NetEventChannel::Event::finish() {
    if(is_present() && _ack) {
        if(_channel->_ret_credits) {
            // pass credits back to client using an empty message
            MsgBuf empty_msg;
            _channel->_rgate.reply(empty_msg, _msg);
        }
        else {
            // Only acknowledge message
            _channel->_rgate.ack_msg(_msg);
        }
        _ack = false;
    }
}

GateIStream NetEventChannel::Event::to_stream() noexcept {
    GateIStream stream(_channel->_rgate, _msg);
    stream.claim();
    return stream;
}


const NetEventChannel::ControlMessage* NetEventChannel::Event::get_message() noexcept {
    return reinterpret_cast<const NetEventChannel::ControlMessage *>(_msg->data);
}

NetEventChannel::Event::Event(const TCU::Message *msg, NetEventChannel *channel) noexcept
    : _msg(msg),
      _channel(channel),
      _ack(true) {
}

void NetEventChannel::EventWorkItem::work() {
    _channel->has_events(_channel->_evhandler, _channel->_crdhandler);
}

}
