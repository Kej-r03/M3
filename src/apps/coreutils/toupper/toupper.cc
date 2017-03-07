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

#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>

#include <accel/stream/Stream.h>

using namespace m3;

int main(int argc, char **argv) {
    File *in = VPE::self().fds()->get(STDIN_FD);
    File *out = VPE::self().fds()->get(STDOUT_FD);

    accel::Stream str(PEISA::ACCEL_TOUP);
    if(argc == 1)
        str.execute(in, out);
    else {
        for(int i = 1; i < argc; ++i) {
            fd_t fd = VFS::open(argv[i], FILE_R);
            if(fd == FileTable::INVALID) {
                errmsg("Unable to open " << argv[i]);
                continue;
            }

            str.execute(VPE::self().fds()->get(fd), out);
            VFS::close(fd);
        }
    }
    return 0;
}
