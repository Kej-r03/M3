/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/log/Lib.h>
#include <base/util/Time.h>

#include <m3/com/GateStream.h>
#include <m3/session/M3FS.h>
#include <m3/vfs/FileTable.h>
#include <m3/vfs/GenericFile.h>
#include <m3/vfs/VFS.h>
#include <m3/Syscalls.h>

namespace m3 {

GenericFile::GenericFile(int flags, capsel_t caps)
    : File(flags),
      _sess(caps + 0),
      _sg(SendGate::bind(caps + 1)),
      // we need a selector to use DTUIf::switch_gate
      _mg(MemGate::bind(VPE::self().alloc_sel())),
      _memoff(),
      _goff(),
      _off(),
      _pos(),
      _len(),
      _writing() {
}

void GenericFile::close() noexcept {
    // file sessions are not known to our resource manager; thus close them manually
    LLOG(FS, "GenFile[" << fd() << "]::close()");
    try {
        send_receive_vmsg(_sg, M3FS::CLOSE);
    }
    catch(...) {
        // ignore
    }
}

void GenericFile::stat(FileInfo &info) const {
    LLOG(FS, "GenFile[" << fd() << "]::stat()");

    GateIStream reply = send_receive_vmsg(_sg, STAT);
    reply.pull_result();
    reply >> info;
}

size_t GenericFile::seek(size_t offset, int whence) {
    LLOG(FS, "GenFile[" << fd() << "]::seek(" << offset << ", " << whence << ")");

    // handle SEEK_CUR as SEEK_SET
    if(whence == M3FS_SEEK_CUR) {
        offset = _goff + _pos + offset;
        whence = M3FS_SEEK_SET;
    }

    // try to seek locally first
    if(whence == M3FS_SEEK_SET) {
        // no change?
        if(offset == _goff + _pos)
            return offset;

        // first submit the written data
        if(_writing)
            submit();

        if(offset >= _goff && offset <= _goff + _len) {
            _pos = offset - _goff;
            return offset;
        }
    }
    else {
        // first submit the written data
        if(_writing)
            submit();
    }

    // now seek on the server side
    size_t off;
    GateIStream reply = send_receive_vmsg(_sg, SEEK, offset, whence);
    reply.pull_result();

    reply >> _goff >> off;
    _pos = _len = 0;
    return _goff + off;
}

size_t GenericFile::read(void *buffer, size_t count) {
    delegate_ep();
    if(_writing)
        submit();

    LLOG(FS, "GenFile[" << fd() << "]::read(" << count << ", pos=" << (_goff + _pos) << ")");

    if(_pos == _len) {
        Time::start(0xbbbb);
        GateIStream reply = send_receive_vmsg(_sg, NEXT_IN);
        reply.pull_result();
        Time::stop(0xbbbb);

        _goff += _len;
        reply >> _off >> _len;
        _pos = 0;
    }

    size_t amount = Math::min(count, _len - _pos);
    if(amount > 0) {
        Time::start(0xaaaa);
        if(flags() & FILE_NODATA) {
            if(count > 2)
                CPU::compute(count / 2);
        }
        else
            _mg.read(buffer, amount, _memoff + _off + _pos);
        Time::stop(0xaaaa);
        _pos += amount;
    }
    return amount;
}

size_t GenericFile::write(const void *buffer, size_t count) {
    delegate_ep();

    LLOG(FS, "GenFile[" << fd() << "]::write(" << count << ", pos=" << (_goff + _pos) << ")");

    if(_pos == _len) {
        Time::start(0xbbbb);
        GateIStream reply = send_receive_vmsg(_sg, NEXT_OUT);
        reply.pull_result();
        Time::stop(0xbbbb);

        _goff += _len;
        reply >> _off >> _len;
        _pos = 0;
    }

    size_t amount = Math::min(count, _len - _pos);
    if(amount > 0) {
        Time::start(0xaaaa);
        if(flags() & FILE_NODATA) {
            if(count > 4)
                CPU::compute(count / 4);
        }
        else
            _mg.write(buffer, amount, _memoff + _off + _pos);
        Time::stop(0xaaaa);
        _pos += amount;
    }
    _writing = true;
    return amount;
}

EP GenericFile::evict(bool closing) {
    LLOG(FS, "GenFile[" << fd() << "]::evict()");

    // submit read/written data
    if(!closing || _writing)
        submit();

    EP ep = _mg.take_ep();

    if(ep.valid())
        VPE::self().revoke(KIF::CapRngDesc(KIF::CapRngDesc::OBJ, ep.sel()), true);

    return ep;
}

void GenericFile::submit() {
    if(_pos > 0) {
        LLOG(FS, "GenFile[" << fd() << "]::submit("
            << (_writing ? "write" : "read") << ", " << _pos << ")");

        GateIStream reply = send_receive_vmsg(_sg, COMMIT, _pos);
        reply.pull_result();

        // if we append, the file was truncated
        _goff += _pos;
        _pos = _len = 0;
    }
    _writing = false;
}

void GenericFile::delegate_ep() {
    if(_mg.ep() == MemGate::UNBOUND) {
        EP ep = VPE::self().fds()->request_ep(this);
        LLOG(FS, "GenFile[" << fd() << "]::delegate_ep(" << ep.id() << ")");
        _sess.delegate_obj(ep.sel());
        _mg.put_ep(std::move(ep));
    }
}

}
