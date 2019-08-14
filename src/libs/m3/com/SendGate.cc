/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/com/SendGate.h>
#include <m3/DTUIf.h>
#include <m3/Exception.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#include <thread/ThreadManager.h>

#include <assert.h>

namespace m3 {

SendGate SendGate::create(RecvGate *rgate, const SendGateArgs &args) {
    auto replygate = args._replygate == nullptr ? &RecvGate::def() : args._replygate;
    auto sel = args._sel == INVALID ? VPE::self().alloc_sel() : args._sel;
    Syscalls::create_sgate(sel, rgate->sel(), args._label, args._credits);
    return SendGate(sel, args._flags, replygate);
}

void SendGate::activate_for(VPE &vpe, epid_t ep) {
    Syscalls::activate(vpe.ep_to_sel(ep), sel(), 0);
}

void SendGate::send(const void *msg, size_t len, label_t reply_label) {
    Errors::Code res = try_send(msg, len, reply_label);
    if(res != Errors::NONE)
        throw DTUException(res);
}

Errors::Code SendGate::try_send(const void *msg, size_t len, label_t reply_label) {
    ensure_activated();

    return DTUIf::send(ep(), msg, len, reply_label, _replygate->ep());
}

const DTU::Message *SendGate::call(const void *msg, size_t len) {
    ensure_activated();

    const DTU::Message *reply = nullptr;
    Errors::Code res = DTUIf::call(ep(), msg, len, _replygate->ep(), &reply);
    if(res != Errors::NONE)
        throw DTUException(res);
    return reply;
}

}
