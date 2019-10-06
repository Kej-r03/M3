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

#include <base/Common.h>
#include <base/CPU.h>
#include <base/DTU.h>

#include "pes/VPE.h"
#include "pes/VPEManager.h"
#include "DTUState.h"
#include "DTU.h"
#include "Platform.h"

namespace kernel {

void *DTUState::get_ep(epid_t ep) {
    return _regs._eps + ep * m3::DTU::EP_REGS;
}

void DTUState::restore(const VPEDesc &vpe, size_t headers) {
    // re-enable pagefaults, if we have a valid pagefault EP (the abort operation disables it)
    m3::DTU::reg_t features = 0;
    if(_regs.get(m3::DTU::DtuRegs::PF_EP) != static_cast<epid_t>(-1))
        features |= m3::DTU::StatusFlags::PAGEFAULTS;
    _regs.set(m3::DTU::DtuRegs::FEATURES, features);

    m3::CPU::compiler_barrier();
    size_t regsSize = sizeof(_regs._dtu) + sizeof(_regs._cmd) + sizeof(_regs._eps);
    DTU::get().write_mem(vpe, m3::DTU::BASE_ADDR, this, regsSize);

    DTU::get().write_mem(vpe,
                         m3::DTU::BASE_ADDR + regsSize + sizeof(_regs._eps),
                         _regs._header,
                         sizeof(_regs._header[0]) * headers);
}

void DTUState::invalidate_eps(epid_t first) {
    size_t total = sizeof(m3::DTU::reg_t) * m3::DTU::EP_REGS * (EP_COUNT - first);
    memset(get_ep(first), 0, total);
}

void DTUState::read_ep(const VPEDesc &vpe, epid_t ep) {
    DTU::get().read_ep_remote(vpe, ep, get_ep(ep));
}

void DTUState::config_recv(epid_t ep, goff_t buf, int order, int msgorder, uint header) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    m3::DTU::reg_t bufSize = static_cast<m3::DTU::reg_t>(order - msgorder);
    m3::DTU::reg_t msgSize = static_cast<m3::DTU::reg_t>(msgorder);
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::RECEIVE) << 61) |
            ((msgSize & 0xFFFF) << 32) | ((bufSize & 0x3F) << 26) | (header << 6);
    r[1] = buf;
    r[2] = 0;
}

void DTUState::config_send(epid_t ep, label_t lbl, peid_t pe, epid_t dstep,
                           size_t msgsize, word_t credits) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::SEND) << 61) | (msgsize & 0xFFFF);
    r[1] = (static_cast<m3::DTU::reg_t>(pe & 0xFF) << 40) |
            (static_cast<m3::DTU::reg_t>(dstep & 0xFF) << 32) |
            (credits << 16) |
            (credits << 0);
    r[2] = lbl;
}

void DTUState::config_mem(epid_t ep, peid_t pe, goff_t addr, size_t size, int perm) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    r[0] = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::MEMORY) << 61) | (size & 0x1FFFFFFFFFFFFFFF);
    r[1] = addr;
    r[2] = ((pe & 0xFF) << 4) | (perm & 0x7);
}

bool DTUState::config_mem_cached(epid_t ep, peid_t pe) {
    m3::DTU::reg_t *r = reinterpret_cast<m3::DTU::reg_t*>(get_ep(ep));
    m3::DTU::reg_t r0, r2;
    r0 = (static_cast<m3::DTU::reg_t>(m3::DTU::EpType::MEMORY) << 61) | 0x1FFFFFFFFFFFFFFF;
    r2 = ((pe & 0xFF) << 4) | m3::DTU::RW;
    bool res = false;
    if(r0 != r[0]) {
        r[0] = r0;
        res = true;
    }
    if(r[1] != 0) {
        r[1] = 0;
        res = true;
    }
    if(r2 != r[2]) {
        r[2] = r2;
        res = true;
    }
    return res;
}

void DTUState::config_pf(gaddr_t rootpt, epid_t sep, epid_t rep) {
    uint features = 0;
    if(sep != static_cast<epid_t>(-1))
        features |= static_cast<uint>(m3::DTU::StatusFlags::PAGEFAULTS);
    _regs.set(m3::DTU::DtuRegs::FEATURES, features);
    _regs.set(m3::DTU::DtuRegs::ROOT_PT, rootpt);
    _regs.set(m3::DTU::DtuRegs::PF_EP, sep | (rep << 8));
}

void DTUState::reset(gaddr_t entry, bool flushInval) {
    m3::DTU::reg_t value = static_cast<m3::DTU::reg_t>(m3::DTU::ExtCmdOpCode::RESET) | (entry << 4);
    value |= static_cast<m3::DTU::reg_t>(flushInval) << 63;
    _regs.set(m3::DTU::DtuRegs::EXT_CMD, value);
}

}
