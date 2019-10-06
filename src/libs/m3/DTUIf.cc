/*
 * Copyright (C) 2015-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <m3/DTUIf.h>
#include <m3/VPE.h>

namespace m3 {

void DTUIf::remove_gate(Gate &gate, bool invalidate) noexcept {
    if(USE_PEXCALLS)
        PEXCalls::call2(Operation::REMOVE_GATE, gate.sel(), invalidate);
    else
        VPE::self().epmng().remove(&gate, invalidate);
}

}
