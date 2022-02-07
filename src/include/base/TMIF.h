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

#include <base/Common.h>
#include <base/time/Duration.h>

namespace m3 {

typedef uint32_t irq_t;

static constexpr irq_t INVALID_IRQ = static_cast<irq_t>(-1);

enum Operation : word_t {
    WAIT,
    EXIT,
    YIELD,
    MAP,
    REG_IRQ,
    TRANSL_FAULT,
    FLUSH_INV,
    NOOP,
};

}

#if defined(__x86_64__)
#   include "arch/x86_64/TMABI.h"
#elif defined(__arm__)
#   include "arch/arm/TMABI.h"
#elif defined(__riscv)
#   include "arch/riscv/TMABI.h"
#else
#   error "Unsupported ISA"
#endif

namespace m3 {

struct TMIF {
    static void wait(epid_t ep, irq_t irq, TimeDuration timeout) {
        TMABI::call3(Operation::WAIT, ep, irq, timeout.as_nanos());
    }

    static void exit(int code) {
        TMABI::call1(Operation::EXIT, static_cast<word_t>(code));
    }

    static void map(uintptr_t virt, goff_t phys, size_t pages, uint perm) {
        TMABI::call4(Operation::MAP, virt, phys, pages, perm);
    }

    static void reg_irq(irq_t irq) {
        TMABI::call1(Operation::REG_IRQ, irq);
    }

    static void flush_invalidate() {
        TMABI::call2(Operation::FLUSH_INV, 0, 0);
    }
};

}
