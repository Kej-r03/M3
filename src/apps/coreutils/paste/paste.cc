/*
 * Copyright (C) 2015-2018 Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2019 Nils Asmussen, Barkhausen Institut
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

#include <m3/stream/FStream.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>

using namespace m3;

int main(int argc, char **argv) {
    FStream *files[static_cast<size_t>(argc) - 1];
    size_t count = 0;

    for(int i = 1; i < argc; ++i) {
        try {
            files[i - 1] = new FStream(argv[i], FILE_R);
        }
        catch(const Exception &e) {
            files[i - 1] = nullptr;
            errmsg("Open failed: " << e.what());
            continue;
        }

        count++;
    }

    char buffer[256];
    while(count > 0) {
        for(int i = 0; i < argc - 1; ++i) {
            if(!files[i])
                continue;

            size_t bytes = files[i]->getline(buffer, sizeof(buffer) - 1);
            buffer[bytes] = '\0';
            cout << buffer;
            if(i + 1 < argc - 1)
                cout << '\t';

            if(files[i]->eof()) {
                delete files[i];
                files[i] = nullptr;
                count--;
            }
        }
        cout << '\n';
    }
    return 0;
}
