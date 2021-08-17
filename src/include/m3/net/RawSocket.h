/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Copyright (C) 2021, Tendsin Mende <tendsin.mende@mailbox.tu-dresden.de>
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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

#include <m3/net/Socket.h>
#include <m3/net/UdpSocket.h>
#include <m3/session/NetworkManager.h>

namespace m3 {

/**
 * Represents a raw IP socket
 */
class RawSocket : public Socket {
    friend class Socket;

    explicit RawSocket(int sd, capsel_t caps, NetworkManager &nm);

public:
    /**
     * Creates a new raw IP sockets with given arguments.
     *
     * By default, the socket is in blocking mode, that is, all functions (send_to, recv_from, ...)
     * do not return until the operation is complete. This can be changed via set_blocking.
     *
     * @param nm the network manager
     * @param protocol the IP protocol
     * @param args optionally additional arguments that define the buffer sizes
     */
    static Reference<RawSocket> create(NetworkManager &nm,
                                       uint8_t protocol,
                                       const DgramSocketArgs &args = DgramSocketArgs());

    ~RawSocket();

    /**
     * Sends <amount> bytes to the socket.
     *
     * @param src the data to send
     * @param amount the number of bytes to send
     * @return the number of sent bytes (-1 if it would block and the socket is non-blocking)
     */
    ssize_t send(const void *src, size_t amount);

    /**
     * Receives <amount> or a smaller number of bytes into <dst>.
     *
     * @param dst the destination buffer
     * @param amount the number of bytes to receive
     * @return the number of received bytes (-1 if it would block and the socket is non-blocking)
     */
    ssize_t recv(void *dst, size_t amount);
};

}