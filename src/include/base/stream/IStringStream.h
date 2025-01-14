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

#pragma once

#include <base/stream/IStream.h>

#include <string.h>
#include <string_view>

namespace m3 {

/**
 * Input-stream that reads from a string
 */
class IStringStream : public IStream {
public:
    /**
     * Reads a value of type <T> from the given string
     *
     * @param str the string
     * @return the read value
     */
    template<typename T>
    static T read_from(const std::string_view &str) {
        IStringStream is(str);
        T t;
        is >> t;
        return t;
    }

    /**
     * Constructor
     *
     * @param str the string
     */
    explicit IStringStream(const std::string_view &str) : IStream(), _str(str), _pos() {
    }

    virtual char read() override {
        if(_pos < _str.length())
            return _str[_pos++];
        _state |= FL_EOF;
        return '\0';
    }
    virtual bool putback(char c) override {
        if(_pos == 0 || _str[_pos - 1] != c)
            return false;
        _pos--;
        return true;
    }

private:
    const std::string_view _str;
    size_t _pos;
};

}
