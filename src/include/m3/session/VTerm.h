/*
 * Copyright (C) 2015-2018 Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2019-2022 Nils Asmussen, Barkhausen Institut
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

#include <base/Errors.h>
#include <base/KIF.h>

#include <m3/com/GateStream.h>
#include <m3/vfs/GenericFile.h>
#include <m3/ObjCap.h>
#include <m3/tiles/Activity.h>

namespace m3 {

class VTerm : public ClientSession {
public:
    explicit VTerm(const String &name) : ClientSession(name) {
    }

    Reference<File> create_channel(bool read) {
        capsel_t sels = Activity::self().alloc_sels(2);
        KIF::ExchangeArgs args;
        ExchangeOStream os(args);
        os << GenericFile::CLONE << (read ? 0 : 1);
        args.bytes = os.total();
        obtain_for(Activity::self(), KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sels, 2), &args);
        auto flags = FILE_NEWSESS | (read ? FILE_R : FILE_W);
        return Reference<File>(new GenericFile(flags, sels));
    }
};

}
