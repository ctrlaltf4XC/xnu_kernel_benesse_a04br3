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
/* Test-only selected exact source branches of pinned Mesa. Narrow wrappers
 * select clear=true, packed=false, even destination, kick_usc=false.
 * Other original branches are deliberately not a test dependency. */
#include "mesa_pixel_event.h"
#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_AO_CLRMSK (0xfc001fffU)
struct shared_program {uint32_t *clear_color;unsigned clear_color_dest_reg;bool kick_usc;};
static uint32_t *mesa_shared_code(struct shared_program *program,uint32_t *buffer){
 uint32_t next_constant=0,data_size=0,code_size=0;
 uint32_t *instruction=buffer;
 enum pvr_pds_generate_mode gen_mode=PDS_GENERATE_CODE_SEGMENT;
 uint32_t color_constant1,color_constant2;
         uint32_t control_word_constant, control_word_last_constant;

         /* Put the clear color and control words into the first 8
          * constants.
          */
         color_constant1 = pvr_pds_get_constants(&next_constant, 2, &data_size);
         color_constant2 = pvr_pds_get_constants(&next_constant, 2, &data_size);
         control_word_constant =
            pvr_pds_get_constants(&next_constant, 2, &data_size);
         control_word_last_constant =
            pvr_pds_get_constants(&next_constant, 2, &data_size);

         if (gen_mode == PDS_GENERATE_CODE_SEGMENT) {
            /* DOUTW the clear color to the USSE constants. Predicate with
             * uniform loading flag (IF0).
             */
            *instruction++ = pvr_pds_encode_doutw64(
               /* cc */ 1, /* Only for Uniform Loading program */
               /* END */ 0,
               /* SRC1 */ control_word_constant, /* DOUTW 32-bit Src1 */
               /* SRC0 */ color_constant1 >> 1); /* DOUTW 64-bit Src0 */

            *instruction++ = pvr_pds_encode_doutw64(
               /* cc */ 1, /* Only for uniform loading program */
               /* END */ program->kick_usc ? 0 : 1, /* Last
                                                     * instruction
                                                     * for a clear.
                                                     */
               /* SRC1 */ control_word_last_constant, /* DOUTW 32-bit Src1
                                                       */
               /* SRC0 */ color_constant2 >> 1); /* DOUTW 64-bit Src0 */
         }

         code_size += 2;
 (void)code_size;
 /* Original clear branch always appends HALT(0), even after DOUTW.END. */
 *instruction++=0xd3000000U;
 return instruction;
}
static uint32_t *mesa_shared_data(struct shared_program *program,uint32_t *constants,
 const struct pvr_device_info *dev_info){
 uint32_t next_constant=0,data_size=0,color_constant1,color_constant2;
 enum pvr_pds_generate_mode gen_mode=PDS_GENERATE_DATA_SEGMENT;
         uint32_t control_word_constant, control_word_last_constant;

         /* Put the clear color and control words into the first 8
          * constants.
          */
         color_constant1 = pvr_pds_get_constants(&next_constant, 2, &data_size);
         color_constant2 = pvr_pds_get_constants(&next_constant, 2, &data_size);
         control_word_constant =
            pvr_pds_get_constants(&next_constant, 2, &data_size);
         control_word_last_constant =
            pvr_pds_get_constants(&next_constant, 2, &data_size);

         if (gen_mode == PDS_GENERATE_DATA_SEGMENT) {
            uint32_t doutw;
            pvr_pds_write_constant64(constants,
                                     color_constant1,
                                     program->clear_color[0],
                                     program->clear_color[1]);

            pvr_pds_write_constant64(constants,
                                     color_constant2,
                                     program->clear_color[2],
                                     program->clear_color[3]);

            /* Load into first constant in common store. */
            doutw = pvr_pds_encode_doutw_src1(
               program->clear_color_dest_reg,
               PVR_PDS_DOUTW_LOWER64,
               PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_DEST_COMMON_STORE,
               false,
               dev_info);

            pvr_pds_write_constant64(constants, control_word_constant, doutw, 0);

            /* Move the destination register along. */
            doutw &= PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_AO_CLRMSK;
            doutw |= (program->clear_color_dest_reg + 2)
                     << PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_AO_SHIFT;

            /* Set the last flag. */
            doutw |= PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_LAST_EN;
            pvr_pds_write_constant64(constants,
                                     control_word_last_constant,
                                     doutw,
                                     0);
         }
 return constants+next_constant;
}
