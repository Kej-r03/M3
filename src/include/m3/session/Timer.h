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

#pragma once

#include <m3/com/Gate.h>
#include <m3/com/RecvGate.h>
#include <m3/session/ClientSession.h>
#include <m3/pes/VPE.h>

namespace m3 {

class Timer : public ClientSession {
public:
    explicit Timer(const String &service, int buford = nextlog2<256>::val,
                                          int msgord = nextlog2<64>::val)
        : ClientSession(service),
          _rgate(RecvGate::create(buford, msgord)),
          _sgate(SendGate::create(&_rgate)) {
        delegate_obj(_sgate.sel());
    }

    RecvGate &rgate() noexcept {
        return _rgate;
    }

private:
    RecvGate _rgate;
    SendGate _sgate;
};

}
