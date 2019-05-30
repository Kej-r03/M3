/**
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
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
#include <base/util/Time.h>
#include <base/Panic.h>

#include <m3/server/RemoteServer.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>
#include <m3/VPE.h>

#define VERBOSE     0

using namespace m3;

struct App {
    explicit App(const char *name, int argc, const char *argv[], bool tmux)
        : argc(argc),
          argv(argv),
          vpe(name, VPEArgs().flags(tmux ? VPE::MUXABLE : 0)) {
        if(Errors::last != Errors::NONE)
            exitmsg("Unable to create VPE");
    }

    int argc;
    const char **argv;
    VPE vpe;
};

int main() {
    if(VERBOSE) cout << "Creating VPEs...\n";

    App *apps[3];

    const char *args1[] = {"/bin/ctx-service", "-s", ""};
    apps[0] = new App("service", ARRAY_SIZE(args1), args1, true);

    const char *args2[] = {"/bin/ctx-client", "2"};
    apps[1] = new App("client1", ARRAY_SIZE(args2), args2, true);

    const char *args3[] = {"/bin/ctx-client", "2"};
    apps[2] = new App("client2", ARRAY_SIZE(args3), args3, true);

    if(VERBOSE) cout << "Starting server...\n";

    RemoteServer *srv = new RemoteServer(apps[0]->vpe, "srv1");
    String srv_args = srv->sel_arg();
    apps[0]->argv[2] = srv_args.c_str();

    if(VERBOSE) cout << "Starting VPEs...\n";

    for(size_t i = 0; i < ARRAY_SIZE(apps); ++i) {
        apps[i]->vpe.mounts(*VPE::self().mounts());
        apps[i]->vpe.obtain_mounts();
        Errors::Code res = apps[i]->vpe.exec(apps[i]->argc, apps[i]->argv);
        if(res != Errors::NONE)
            PANIC("Cannot execute " << apps[i]->argv[0] << ": " << Errors::to_string(res));
    }

    if(VERBOSE) cout << "Waiting for VPEs...\n";

    // don't wait for the service
    for(size_t i = 1; i < 3; ++i) {
        int res = apps[i]->vpe.wait();
        if(VERBOSE) cout << apps[i]->argv[0] << " exited with " << res << "\n";
    }

    if(VERBOSE) cout << "Shutdown server...\n";

    srv->request_shutdown();
    apps[0]->vpe.wait();

    if(VERBOSE) cout << "Deleting VPEs...\n";

    for(size_t i = 0; i < ARRAY_SIZE(apps); ++i)
        delete apps[i];

    if(VERBOSE) cout << "Done\n";
    return 0;
}
