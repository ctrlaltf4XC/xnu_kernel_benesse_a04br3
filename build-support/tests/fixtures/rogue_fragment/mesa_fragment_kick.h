/*
 * Copyright © 2022 Imagination Technologies Ltd.
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
/* Exact selected pinned Mesa producer. Dependency encoders/macros use the
 * existing pinned mesa_pixel_event.h fixture; no native-byte interpreter. */
#include "mesa_pixel_event.h"
#define ROGUE_PDS_FIXED_PIXEL_SHADER_DATA_SIZE 8U
struct pvr_pds_kickusc_program {
   uint32_t *data_segment;
   struct pvr_pds_usc_task_control usc_task_control;

   uint32_t doutu_offset;

   uint32_t data_size;
   uint32_t code_size;
};
uint32_t *pvr_pds_kick_usc(struct pvr_pds_kickusc_program *restrict program,
                           uint32_t *restrict buffer,
                           uint32_t start_next_constant,
                           bool cc_enabled,
                           enum pvr_pds_generate_mode gen_mode)
{
   uint32_t constant = 0;

   /* Automatically get a data size of 2 128bit chunks. */
   uint32_t data_size = ROGUE_PDS_FIXED_PIXEL_SHADER_DATA_SIZE;
   uint32_t code_size = 1; /* Single doutu */
   uint32_t dummy_count = 0;

   /* Setup the data part. */
   uint32_t *constants = buffer; /* Constants placed at front of buffer. */
   uint32_t next_constant = PVR_PDS_CONSTANTS_BLOCK_BASE; /* Constants count in
                                                           * dwords.
                                                           */

   /* Update the program sizes. */
   program->data_size = data_size;
   program->code_size = code_size;
   program->data_segment = constants;

   if (gen_mode == PDS_GENERATE_SIZES)
      return NULL;

   if (gen_mode == PDS_GENERATE_DATA_SEGMENT ||
       gen_mode == PDS_GENERATE_CODEDATA_SEGMENTS) {
      /* Copy the USC task control words to constants. */

      constant = pvr_pds_get_constants(&next_constant, 2, &dummy_count);
      program->doutu_offset = constant;

      pvr_pds_write_wide_constant(constants,
                                  constant + 0,
                                  program->usc_task_control.src0); /* 64-bit
                                                                    * Src0.
                                                                    */
      buffer += data_size;

      if (gen_mode == PDS_GENERATE_DATA_SEGMENT)
         return buffer;
   }

   if (gen_mode == PDS_GENERATE_CODE_SEGMENT ||
       gen_mode == PDS_GENERATE_CODEDATA_SEGMENTS) {
      /* Generate the PDS pixel shader code. */

      /* Setup the instruction pointer. */
      uint32_t *instruction = buffer;

      /* Issue the task to the USC.
       *
       * dout ds1[constant_use], ds0[constant_use], ds1[constant_use], emit ;
       * halt halt
       */

      *instruction++ = pvr_pds_encode_doutu(
         /* cc */ cc_enabled,
         /* END */ 1,
         /* SRC0 */ (constant + start_next_constant) >> 1); /* DOUTU
                                                             * 64-bit Src0
                                                             */

      /* Return pointer to just after last instruction. */
      return instruction;
   }

   /* Execution should never reach here; keep compiler happy. */
   return NULL;
}
