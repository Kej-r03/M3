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

#include <base/Common.h>
#include <base/stream/Serial.h>

#include "../assert.h"
#include "../pes.h"
#include "../tcuif.h"

using namespace m3;

static constexpr size_t MSG_SIZE = 64;

static ALIGNED(8) uint8_t rbuf[64];
static uint8_t msg[32];

int main() {
    kernel::TCU::config_send(0, 0x1234, pe_id(PE::PE0), 0, nextlog2<MSG_SIZE>::val, 1);
    size_t size = nextlog2<sizeof(rbuf)>::val;
    uintptr_t rbuf_addr = reinterpret_cast<uintptr_t>(rbuf);
    kernel::TCU::config_recv(1, rbuf_addr, size, size, TCU::NO_REPLIES);

    Serial::get() << "Hello World from sender!\n";

    // initial send; wait until receiver is ready
    Errors::Code res;
    while((res = kernel::TCU::send(0, msg, sizeof(msg), 0x2222, 1)) != Errors::NONE) {
        Serial::get() << "send failed: " << res << "\n";
        // get credits back
        kernel::TCU::config_send(0, 0x1234, pe_id(PE::PE0), 0, nextlog2<MSG_SIZE>::val, 1);
    }

    for(int count = 0; ; ++count) {
        if(count % 100000 == 0)
            Serial::get() << "sent " << count << " messages\n";

        // wait for reply
        const TCU::Message *rmsg;
        while((rmsg = kernel::TCU::fetch_msg(1, rbuf_addr)) == nullptr)
            ;
        ASSERT_EQ(rmsg->label, 0x2222);

        // ack reply
        ASSERT_EQ(kernel::TCU::ack_msg(1, rbuf_addr, rmsg), Errors::NONE);

        // send message
        ASSERT_EQ(kernel::TCU::send(0, msg, sizeof(msg), 0x2222, 1), Errors::NONE);
        msg[0]++;
    }
    return 0;
}
