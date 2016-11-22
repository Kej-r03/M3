/**
 * Copyright (C) 2016, René Küttner <rene.kuettner@.tu-dresden.de>
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

#include <base/DTU.h>
#include <base/Env.h>
#include <base/Exceptions.h>
#include <base/RCTMux.h>

#include "../../RCTMux.h"
#include "Exceptions.h"

EXTERN_C void _save(void *state);

namespace RCTMux {

void init() {
    Exceptions::init();
    Exceptions::get_table()[64] = reinterpret_cast<m3::Exceptions::isr_func>(_save);
}

void *init_state() {
    m3::Env *senv = m3::env();
    senv->isrs = reinterpret_cast<uintptr_t>(Exceptions::get_table());

    // put state at the stack top
    m3::Exceptions::State *state = reinterpret_cast<m3::Exceptions::State*>(senv->sp) - 1;

    // init State
    state->rax = 0xDEADBEEF;    // tell crt0 that we've set the SP
    state->rbx = 0;
    state->rcx = 0;
    state->rdx = 0;
    state->rsi = 0;
    state->rdi = 0;
    state->r8  = 0;
    state->r9  = 0;
    state->r10 = 0;
    state->r11 = 0;
    state->r12 = 0;
    state->r13 = 0;
    state->r14 = 0;
    state->r15 = 0;

    state->cs  = (Exceptions::SEG_CODE << 3) | 3;
    state->ss  = (Exceptions::SEG_DATA << 3) | 3;
    state->rip = senv->entry;
    state->rsp = reinterpret_cast<uintptr_t>(state);
    state->rbp = 0;
    state->rflags = 0x200;  // enable interrupts

    return state;
}

} /* namespace RCTMux */
