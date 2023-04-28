/*
 * Copyright (C) 2023 Nils Asmussen, Barkhausen Institut
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

#include <base/Types.h>

namespace m3 {
namespace opcodes {

struct General {
    enum Operation : uint64_t {
        CONNECT = static_cast<uint64_t>(1) << 63,
    };
};

struct File {
    enum Operation {
        STAT,
        SEEK,
        NEXT_IN,
        NEXT_OUT,
        COMMIT,
        TRUNCATE,
        SYNC,
        CLOSE,
        CLONE,
        GET_PATH,
        GET_TMODE,
        SET_TMODE,
        SET_DEST,
        ENABLE_NOTIFY,
        REQ_NOTIFY,
    };
};

struct FileSystem {
    enum Operation {
        FSTAT = File::STAT,
        SEEK = File::SEEK,
        NEXT_IN = File::NEXT_IN,
        NEXT_OUT = File::NEXT_OUT,
        COMMIT = File::COMMIT,
        TRUNCATE = File::TRUNCATE,
        SYNC = File::SYNC,
        CLOSE = File::CLOSE,
        CLONE = File::CLONE,
        GET_TMODE = File::GET_TMODE,
        SET_TMODE = File::SET_TMODE,
        SET_DEST = File::SET_DEST,
        ENABLE_NOTIFY = File::ENABLE_NOTIFY,
        REQ_NOTIFY = File::REQ_NOTIFY,
        STAT,
        MKDIR,
        RMDIR,
        LINK,
        UNLINK,
        RENAME,
        OPEN,
        GET_MEM,
        DEL_EP,
        OPEN_PRIV,
    };
};

struct Net {
    enum Operation {
        STAT = File::STAT,
        SEEK = File::SEEK,
        NEXT_IN = File::NEXT_IN,
        NEXT_OUT = File::NEXT_OUT,
        COMMIT = File::COMMIT,
        TRUNCATE = File::TRUNCATE,
        CLOSE = File::CLOSE,
        CLONE = File::CLONE,
        GET_TMODE = File::GET_TMODE,
        SET_TMODE = File::SET_TMODE,
        SET_DEST = File::SET_DEST,
        ENABLE_NOTIFY = File::ENABLE_NOTIFY,
        REQ_NOTIFY = File::REQ_NOTIFY,
        BIND,
        LISTEN,
        CONNECT,
        ABORT,
        CREATE,
        GET_IP,
        GET_NAMESRV,
        GET_SGATE,
        OPEN_FILE,
    };
};

struct Pipe {
    enum Operation {
        OPEN_PIPE = File::REQ_NOTIFY + 1,
        OPEN_CHAN,
        SET_MEM,
        CLOSE_PIPE,
    };
};

struct ResMng {
    enum Operation {
        REG_SERV,
        UNREG_SERV,

        OPEN_SESS,
        CLOSE_SESS,

        ADD_CHILD,
        REM_CHILD,

        ALLOC_MEM,
        FREE_MEM,

        ALLOC_TILE,
        FREE_TILE,

        USE_RGATE,
        USE_SGATE,
        USE_SEM,
        USE_MOD,
    };
};

struct Pager {
    enum Operation {
        PAGEFAULT,
        INIT,
        ADD_CHILD,
        CLONE,
        MAP_ANON,
        MAP_DS,
        MAP_MEM,
        UNMAP,
        CLOSE,
        COUNT,
    };
};

struct LoadGen {
    enum Operation {
        START,
        RESPONSE,
        COUNT
    };
};

}
}