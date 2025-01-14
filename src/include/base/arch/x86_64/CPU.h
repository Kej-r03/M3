/*
 * Copyright (C) 2015-2018 Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2019-2020 Nils Asmussen, Barkhausen Institut
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

#include <base/CPU.h>
#include <base/Common.h>

#define NEED_ALIGNED_MEMACC 0

namespace m3 {

inline uint64_t CPU::read8b(uintptr_t addr) {
    uint64_t res;
    asm volatile("mov (%1), %0" : "=r"(res) : "r"(addr));
    return res;
}

inline void CPU::write8b(uintptr_t addr, uint64_t val) {
    asm volatile("mov %0, (%1)" : : "r"(val), "r"(addr));
}

ALWAYS_INLINE word_t CPU::base_pointer() {
    word_t val;
    asm volatile("mov %%rbp, %0;" : "=r"(val));
    return val;
}

ALWAYS_INLINE word_t CPU::stack_pointer() {
    word_t val;
    asm volatile("mov %%rsp, %0;" : "=r"(val));
    return val;
}

inline cycles_t CPU::elapsed_cycles() {
    uint32_t u, l;
    asm volatile("rdtsc" : "=a"(l), "=d"(u) : : "memory");
    return static_cast<cycles_t>(u) << 32 | l;
}

inline uintptr_t CPU::backtrace_step(uintptr_t bp, uintptr_t *func) {
    *func = reinterpret_cast<uintptr_t *>(bp)[1];
    return reinterpret_cast<uintptr_t *>(bp)[0];
}

inline void CPU::compute(cycles_t cycles) {
    cycles_t iterations = cycles / 2;
    asm volatile(
        ".align 16;"
        "1: dec %0;"
        "test   %0, %0;"
        "ja     1b;"
        // let the compiler know that we change the value of iterations
        // as it seems, inputs are not expected to change
        : "=r"(iterations)
        : "0"(iterations));
}

inline void CPU::memory_barrier() {
    asm volatile("mfence" : : : "memory");
}

inline cycles_t CPU::gem5_debug(uint64_t msg) {
    cycles_t res;
    asm volatile(
        ".byte 0x0F, 0x04;"
        ".word 0x63;"
        : "=a"(res)
        : "D"(msg));
    return res;
}

}
