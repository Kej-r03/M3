/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/util/Math.h>
#include <base/CPU.h>
#include <base/DTU.h>
#include <base/Init.h>
#include <base/KIF.h>

namespace m3 {

INIT_PRIO_DTU DTU DTU::inst;

void DTU::try_sleep(bool yield, uint64_t cycles) {
    // check for messages a few times
    for(int i = 0; i < 100; ++i) {
        if(read_reg(MasterRegs::MSG_CNT) > 0)
            return;
    }

    uint64_t yield_time = *reinterpret_cast<uint64_t*>(RCTMUX_YIELD);
    if(yield && yield_time > 0) {
        // if we want to wait longer than our yield time, sleep first for a while until we yield
        if(cycles == 0 || cycles > yield_time) {
            // sleep a bit
            uint64_t now = read_reg(MasterRegs::CUR_TIME);
            CPU::memory_barrier();
            sleep(yield_time);
            CPU::memory_barrier();
            uint64_t sleep_time = read_reg(MasterRegs::CUR_TIME) - now;

            // if we were waked up early, there is something to do
            if(sleep_time < yield_time)
                return;

            // adjust the remaining sleep time
            if(cycles >= sleep_time)
                cycles -= sleep_time;
            else if(cycles)
                return;
        }

        // if we still want to sleep, yield
        m3::env()->backend()->yield();
    }

    // note that the DTU checks again whether there actually are no messages, because we might
    // have received something after the check above
    sleep(cycles);
}

Errors::Code DTU::send(epid_t ep, const void *msg, size_t size, label_t replylbl, epid_t reply_ep) {
    static_assert(KIF::Perm::R == DTU::R, "DTU::R does not match KIF::Perm::R");
    static_assert(KIF::Perm::W == DTU::W, "DTU::W does not match KIF::Perm::W");

    static_assert(KIF::Perm::R == DTU::PTE_R, "DTU::PTE_R does not match KIF::Perm::R");
    static_assert(KIF::Perm::W == DTU::PTE_W, "DTU::PTE_W does not match KIF::Perm::W");
    static_assert(KIF::Perm::X == DTU::PTE_X, "DTU::PTE_X does not match KIF::Perm::X");

    write_reg(CmdRegs::DATA, reinterpret_cast<uintptr_t>(msg) | (static_cast<reg_t>(size) << 48));
    if(replylbl)
        write_reg(CmdRegs::REPLY_LABEL, replylbl);
    CPU::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::SEND, 0, reply_ep));

    return get_error();
}

Errors::Code DTU::reply(epid_t ep, const void *msg, size_t size, size_t off) {
    write_reg(CmdRegs::DATA, reinterpret_cast<uintptr_t>(msg) | (size << 48));
    CPU::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::REPLY, 0, off));

    return get_error();
}

Errors::Code DTU::read(epid_t ep, void *data, size_t size, size_t off, uint flags) {
    write_reg(CmdRegs::DATA, reinterpret_cast<uintptr_t>(data) | (size << 48));
    CPU::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::READ, flags, off));

    Errors::Code res = get_error();
    CPU::memory_barrier();
    return res;
}

Errors::Code DTU::write(epid_t ep, const void *data, size_t size, size_t off, uint flags) {
    write_reg(CmdRegs::DATA, reinterpret_cast<uintptr_t>(data) | (size << 48));
    CPU::compiler_barrier();
    write_reg(CmdRegs::COMMAND, buildCommand(ep, CmdOpCode::WRITE, flags, off));

    return get_error();
}

}
