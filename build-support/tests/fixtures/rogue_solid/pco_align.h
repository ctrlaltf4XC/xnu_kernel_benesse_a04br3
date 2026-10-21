/*
 * Copyright © 2022, 2024 Imagination Technologies Ltd.
 * Copyright (c) 2026 A04BR3 contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/* SPDX-License-Identifier: MIT
 * Exact pinned pco_group_instrs.c calc_align_padding source excerpt.
 * Typedef/layout scaffolding in the test, not a substituted alignment rule.
 */
static inline void calc_align_padding(pco_igrp *last_igrp,
                                      unsigned *offset_bytes)
{
   /* We should never end up with a completely empty shader. */
   assert(last_igrp);

   unsigned total_align = last_igrp->enc.len.total % ROGUE_ICACHE_ALIGN;
   unsigned offset_align = last_igrp->enc.offset % ROGUE_ICACHE_ALIGN;

   if (total_align) {
      unsigned padding = ROGUE_ICACHE_ALIGN - total_align;
      *offset_bytes += padding;

      /* Pad the size of the last igrp. */
      last_igrp->enc.len.align_padding += padding;
      last_igrp->enc.len.total += padding;

      /* Update the last igrp header length. */
      last_igrp->hdr.length = last_igrp->enc.len.total / 2;
   }

   if (offset_align) {
      unsigned padding = ROGUE_ICACHE_ALIGN - offset_align;
      *offset_bytes += padding;

      /* Pad the size of the penultimate igrp. */
      pco_igrp *penultimate_igrp = pco_prev_igrp(last_igrp);

      /* If we only have one igrp then its offset will be zero. */
      assert(penultimate_igrp);

      penultimate_igrp->enc.len.align_padding += padding;
      penultimate_igrp->enc.len.total += padding;

      /* Update the penultimate igrp header length. */
      penultimate_igrp->hdr.length = penultimate_igrp->enc.len.total / 2;

      /* Update the offset of the last igrp. */
      last_igrp->enc.offset += padding;
   }
}
