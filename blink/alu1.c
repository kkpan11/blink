/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include <limits.h>
#include <stdatomic.h>

#include "blink/alu.h"
#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/modrm.h"
#include "blink/swap.h"

static void AluEb(struct Machine *m, uint32_t rde, aluop_f op) {
  uint8_t *p;
  p = GetModrmRegisterBytePointerWrite(m, rde);
  if (!Lock(rde)) {
    Write8(p, op(Read8(p), 0, &m->flags));
  } else {
#if !defined(__riscv) && !defined(__MICROBLAZE__)
    uint8_t x, z;
    x = Read8(p);
    do {
      z = op(x, 0, &m->flags);
    } while (!atomic_compare_exchange_weak_explicit(
        (atomic_uchar *)p, &x, z, memory_order_release, memory_order_relaxed));
#else
    OpUd(m, rde);
#endif
  }
}

void OpNotEb(struct Machine *m, uint32_t rde) {
  AluEb(m, rde, Not8);
}

void OpNegEb(struct Machine *m, uint32_t rde) {
  AluEb(m, rde, Neg8);
}

void Op0fe(struct Machine *m, uint32_t rde) {
  switch (ModrmReg(rde)) {
    case 0:
      AluEb(m, rde, Inc8);
      break;
    case 1:
      AluEb(m, rde, Dec8);
      break;
    default:
      OpUd(m, rde);
  }
}

static void AluEvqp(struct Machine *m, uint32_t rde, const aluop_f ops[4]) {
  uint8_t *p;
  if (Rexw(rde)) {
    p = GetModrmRegisterWordPointerWrite(m, rde, 8);
    if (Lock(rde) && !((intptr_t)p & 7)) {
#if LONG_BIT == 64
      uint64_t x, z;
      x = atomic_load((atomic_ulong *)p);
      do {
        z = ops[ALU_INT64](SWAP64LE(x), 0, &m->flags);
        z = SWAP64LE(z);
      } while (!atomic_compare_exchange_weak_explicit((atomic_ulong *)p, &x, z,
                                                      memory_order_release,
                                                      memory_order_relaxed));
#else
      OpUd(m, rde);
#endif
    } else {
      Write64(p, ops[ALU_INT64](Read64(p), 0, &m->flags));
    }
  } else if (!Osz(rde)) {
    uint32_t x, z;
    p = GetModrmRegisterWordPointerWrite(m, rde, 4);
    if (Lock(rde) && !((intptr_t)p & 3)) {
      x = atomic_load((atomic_uint *)p);
      do {
        z = ops[ALU_INT32](SWAP32LE(x), 0, &m->flags);
        z = SWAP32LE(z);
      } while (!atomic_compare_exchange_weak_explicit(
          (atomic_uint *)p, &x, z, memory_order_release, memory_order_relaxed));
    } else {
      Write32(p, ops[ALU_INT32](Read32(p), 0, &m->flags));
    }
    if (IsModrmRegister(rde)) {
      Write32(p + 4, 0);
    }
  } else {
    unassert(!Lock(rde));
    p = GetModrmRegisterWordPointerWrite(m, rde, 2);
    Write16(p, ops[ALU_INT16](Read16(p), 0, &m->flags));
  }
}

void OpNotEvqp(struct Machine *m, uint32_t rde) {
  AluEvqp(m, rde, kAlu[ALU_NOT]);
}

void OpNegEvqp(struct Machine *m, uint32_t rde) {
  AluEvqp(m, rde, kAlu[ALU_NEG]);
}

void OpIncEvqp(struct Machine *m, uint32_t rde) {
  AluEvqp(m, rde, kAlu[ALU_INC]);
}

void OpDecEvqp(struct Machine *m, uint32_t rde) {
  AluEvqp(m, rde, kAlu[ALU_DEC]);
}