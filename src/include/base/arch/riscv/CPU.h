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

#include <base/Common.h>
#include <base/CPU.h>

#define NEED_ALIGNED_MEMACC     0

namespace m3 {

inline uint64_t CPU::read8b(uintptr_t addr) {
    uint64_t res;
    asm volatile (
        "ld %0, (%1)"
        : "=r"(res)
        : "r"(addr)
    );
    return res;
}

inline void CPU::write8b(uintptr_t addr, uint64_t val) {
    asm volatile (
        "sd %0, (%1)"
        : : "r"(val), "r"(addr)
    );
}

inline word_t CPU::get_sp() {
    word_t val;
    asm volatile (
        "mv %0, sp;"
        : "=r" (val)
    );
    return val;
}

inline void CPU::compute(cycles_t cycles) {
    cycles_t iterations = cycles / 2;
    asm volatile (
        ".align 4;"
        "1: addi %0, %0, -1;"
        "bnez %0, 1b;"
        // let the compiler know that we change the value of cycles
        // as it seems, inputs are not expected to change
        : "=r"(iterations) : "0"(iterations)
    );
}

inline void CPU::memory_barrier() {
    asm volatile ("fence" : : : "memory");
}

}