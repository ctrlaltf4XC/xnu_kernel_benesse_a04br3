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
/* Exact selected Mesa functions/declarations/macros. Test-only device feature
 * shim supplies just the queried SLC feature; native board chooses false. */
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#define ALWAYS_INLINE inline
#define restrict __restrict
#define PVR_HAS_FEATURE(d,f) ((d)->f)
#define PVR_PDS_PRINT_INST(...) ((void)0)
#define PVR_PDS_PRINT_DATA(...) ((void)0)
#define DIV_ROUND_UP(a,b) (((a)+(b)-1)/(b))
#define L32(a) ((uint32_t)(a))
#define H32(a) ((uint32_t)((a)>>32))
struct pvr_device_info {bool slc_mcu_cache_controls;};
#define PVR_PDS_CONSTANTS_BLOCK_BASE 0

#define PVR_PDS_CONSTANTS_BLOCK_SIZE 128

#define PVR_PDS_DOUTW_LOWER64 0x2

#define PVR_PDS_DOUTW_MAXMASK 0x4

#define PVR_ROGUE_PDSINST_DOUT_CC_SHIFT (27U)

#define PVR_ROGUE_PDSINST_DOUT_DST_SHIFT (0U)

#define PVR_ROGUE_PDSINST_DOUT_END_SHIFT (26U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_DUAL_PHASE_EN \
   (UINT64_C(0x0000020000000000))

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_EXE_OFF_ALIGNSHIFT (2U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_EXE_OFF_CLRMSK \
   (UINT64_C(0xFFFFFFFF00000003))

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_EXE_OFF_SHIFT (2U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_SAMPLE_RATE_SHIFT (33U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_TEMPS_ALIGNSIZE (4U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_TEMPS_SHIFT (35U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_TEMPS_MASK (0x0000003FU)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_AO_SHIFT (13U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_BSIZE_ALL64 (0x00000002U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_BSIZE_LOWER (0x00000000U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_BSIZE_UPPER (0x00000001U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_CMODE_BYPASS (0x04000000U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_CMODE_CACHED (0x00000000U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_DEST_COMMON_STORE (0x10000000U)

#define PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_LAST_EN (0x80000000U)

#define PVR_ROGUE_PDSINST_DOUT_OPCODE_SHIFT (28U)

#define PVR_ROGUE_PDSINST_DOUT_SRC0_SHIFT (8U)

#define PVR_ROGUE_PDSINST_DOUT_SRC1_SHIFT (16U)

#define PVR_ROGUE_PDSINST_DSTDOUT_DOUTU (0x00000002U)

#define PVR_ROGUE_PDSINST_DSTDOUT_DOUTW (0x00000001U)

#define PVR_ROGUE_PDSINST_DSTDOUT_MASK (0x00000007U)

#define PVR_ROGUE_PDSINST_OPCODEC_DOUT UINT32_C(0x0000000f)

#define PVR_ROGUE_PDSINST_REGS32_CONST32_LOWER (0U)

#define PVR_ROGUE_PDSINST_REGS32_CONST32_UPPER (127U)

#define PVR_ROGUE_PDSINST_REGS32_MASK (0x000000FFU)

#define PVR_ROGUE_PDSINST_REGS64_CONST64_LOWER (0U)

#define PVR_ROGUE_PDSINST_REGS64_CONST64_UPPER (63U)

#define PVR_ROGUE_PDSINST_REGS64_MASK (0x0000007FU)

enum pvr_pds_generate_mode {
   PDS_GENERATE_SIZES,
   PDS_GENERATE_CODE_SEGMENT,
   PDS_GENERATE_DATA_SEGMENT,
   PDS_GENERATE_CODEDATA_SEGMENTS
};

struct pvr_pds_usc_task_control {
   uint64_t src0;
};

struct pvr_pds_event_program {
   uint32_t *data_segment;
   struct pvr_pds_usc_task_control task_control;

   uint32_t num_emit_word_pairs;
   uint32_t *emit_words;

   uint32_t data_size;
   uint32_t code_size;
};

static const uint32_t dword_mask_const[PVR_PDS_DOUTW_MAXMASK] = {
   PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_BSIZE_LOWER,
   PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_BSIZE_UPPER,
   PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_BSIZE_ALL64,
   PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_BSIZE_ALL64
};

static const uint32_t cache_control_const[2][2] = {
   { PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_CMODE_BYPASS,
     PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_CMODE_CACHED },
   { 0, 0 }
};

static ALWAYS_INLINE uint32_t pvr_pds_inst_encode_dout(uint32_t cc,
                                                       uint32_t end,
                                                       uint32_t src1,
                                                       uint32_t src0,
                                                       uint32_t dst)
{
   uint32_t encoded = 0;

   encoded |= PVR_ROGUE_PDSINST_OPCODEC_DOUT
              << PVR_ROGUE_PDSINST_DOUT_OPCODE_SHIFT;
   encoded |= ((dst & PVR_ROGUE_PDSINST_DSTDOUT_MASK)
               << PVR_ROGUE_PDSINST_DOUT_DST_SHIFT);
   encoded |= ((src0 & PVR_ROGUE_PDSINST_REGS64_MASK)
               << PVR_ROGUE_PDSINST_DOUT_SRC0_SHIFT);
   encoded |= ((src1 & PVR_ROGUE_PDSINST_REGS32_MASK)
               << PVR_ROGUE_PDSINST_DOUT_SRC1_SHIFT);
   encoded |= ((end & 1U) << PVR_ROGUE_PDSINST_DOUT_END_SHIFT);
   encoded |= ((cc & 1U) << PVR_ROGUE_PDSINST_DOUT_CC_SHIFT);

   PVR_PDS_PRINT_INST(encoded);

   return encoded;
}

static ALWAYS_INLINE uint32_t
pvr_pds_encode_doutw_src1(uint32_t dest,
                          uint32_t dword_mask,
                          uint32_t flags,
                          bool cached,
                          const struct pvr_device_info *dev_info)
{
   assert(((dword_mask > PVR_PDS_DOUTW_LOWER64) && ((dest & 3) == 0)) ||
          ((dword_mask == PVR_PDS_DOUTW_LOWER64) && ((dest & 1) == 0)) ||
          (dword_mask < PVR_PDS_DOUTW_LOWER64));

   uint32_t encoded =
      (dest << PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_AO_SHIFT);

   encoded |= dword_mask_const[dword_mask];

   encoded |= flags;

   encoded |=
      cache_control_const[PVR_HAS_FEATURE(dev_info, slc_mcu_cache_controls) ? 0
                                                                            : 1]
                         [cached ? 1 : 0];
   return encoded;
}

static ALWAYS_INLINE uint32_t pvr_pds_encode_doutw64(uint32_t cc,
                                                     uint32_t end,
                                                     uint32_t src1,
                                                     uint32_t src0)
{
   return pvr_pds_inst_encode_dout(cc,
                                   end,
                                   src1,
                                   src0,
                                   PVR_ROGUE_PDSINST_DSTDOUT_DOUTW);
}

static ALWAYS_INLINE uint32_t pvr_pds_encode_doutu(uint32_t cc,
                                                   uint32_t end,
                                                   uint32_t src0)
{
   return pvr_pds_inst_encode_dout(cc,
                                   end,
                                   0,
                                   src0,
                                   PVR_ROGUE_PDSINST_DSTDOUT_DOUTU);
}

/**
 * Gets the next constant address and moves the next constant pointer along.
 *
 * \param next_constant Pointer to the next constant address.
 * \param num_constants The number of constants required.
 * \param count The number of constants allocated.
 * \return The address of the next constant.
 */
static uint32_t pvr_pds_get_constants(uint32_t *next_constant,
                                      uint32_t num_constants,
                                      uint32_t *count)
{
   uint32_t constant;

   /* Work out starting constant number. For even number of constants, start on
    * a 64-bit boundary.
    */
   if (num_constants & 1)
      constant = *next_constant;
   else
      constant = (*next_constant + 1) & ~1;

   /* Update the count with the number of constants actually allocated. */
   *count += constant + num_constants - *next_constant;

   /* Move the next constant pointer. */
   *next_constant = constant + num_constants;

   assert((constant + num_constants) <= PVR_PDS_CONSTANTS_BLOCK_SIZE);

   return constant;
}

/**
 * Write a 32-bit constant indexed by the long range.
 *
 * \param data_block Pointer to data block to write to.
 * \param index Index within the data to write to.
 * \param dword The 32-bit constant to write.
 */
static void
pvr_pds_write_constant32(uint32_t *data_block, uint32_t index, uint32_t dword0)
{
   /* Check range. */
   assert(index <= (PVR_ROGUE_PDSINST_REGS32_CONST32_UPPER -
                    PVR_ROGUE_PDSINST_REGS32_CONST32_LOWER));

   data_block[index + 0] = dword0;

   PVR_PDS_PRINT_DATA("WriteConstant32", (uint64_t)dword0, index);
}

/**
 * Write a 64-bit constant indexed by the long range.
 *
 * \param data_block Pointer to data block to write to.
 * \param index Index within the data to write to.
 * \param dword0 Lower half of the 64 bit constant.
 * \param dword1 Upper half of the 64 bit constant.
 */
static void pvr_pds_write_constant64(uint32_t *data_block,
                                     uint32_t index,
                                     uint32_t dword0,
                                     uint32_t dword1)
{
   /* Has to be on 64 bit boundary. */
   assert((index & 1) == 0);

   /* Check range. */
   assert((index >> 1) <= (PVR_ROGUE_PDSINST_REGS64_CONST64_UPPER -
                           PVR_ROGUE_PDSINST_REGS64_CONST64_LOWER));

   data_block[index + 0] = dword0;
   data_block[index + 1] = dword1;

   PVR_PDS_PRINT_DATA("WriteConstant64",
                      ((uint64_t)dword0 << 32) | (uint64_t)dword1,
                      index);
}

static void
pvr_pds_write_wide_constant(uint32_t *data_block, uint32_t index, uint64_t word)
{
   /* Has to be on 64 bit boundary. */
   assert((index & 1) == 0);

   /* Check range. */
   assert((index >> 1) <= (PVR_ROGUE_PDSINST_REGS64_CONST64_UPPER -
                           PVR_ROGUE_PDSINST_REGS64_CONST64_LOWER));

   data_block[index + 0] = L32(word);
   data_block[index + 1] = H32(word);

   PVR_PDS_PRINT_DATA("WriteWideConstant", word, index);
}

/**
 * Generate USC address.
 *
 * \param doutu Location to write the generated address.
 * \param execution_address Address to generate from.
 */
static ALWAYS_INLINE void
pvr_set_usc_execution_address64(uint64_t *doutu, uint64_t execution_address)
{
   doutu[0] |= (((execution_address >>
                  PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_EXE_OFF_ALIGNSHIFT)
                 << PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_EXE_OFF_SHIFT) &
                ~PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_EXE_OFF_CLRMSK);
}

/* FIXME: use the csbgen interface and pvr_csb_pack.
 * FIXME: use bool for phase_rate_change.
 */
/**
 * Sets up the USC control words for a DOUTU.
 *
 * \param usc_task_control USC task control structure to be setup.
 * \param execution_address USC execution virtual address.
 * \param usc_temps Number of USC temps.
 * \param sample_rate Sample rate for the DOUTU.
 * \param phase_rate_change Phase rate change for the DOUTU.
 */
void pvr_pds_setup_doutu(struct pvr_pds_usc_task_control *usc_task_control,
                         uint64_t execution_address,
                         uint32_t usc_temps,
                         uint32_t sample_rate,
                         bool phase_rate_change)
{
   usc_task_control->src0 = UINT64_C(0);

   /* Set the execution address. */
   pvr_set_usc_execution_address64(&(usc_task_control->src0),
                                   execution_address);

   if (usc_temps > 0) {
      /* Temps are allocated in blocks of 4 dwords. */
      usc_temps =
         DIV_ROUND_UP(usc_temps,
                      PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_TEMPS_ALIGNSIZE);

      /* Check for losing temps due to too many requested. */
      assert((usc_temps & PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_TEMPS_MASK) ==
             usc_temps);

      usc_task_control->src0 |=
         ((uint64_t)(usc_temps &
                     PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_TEMPS_MASK))
         << PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_TEMPS_SHIFT;
   }

   if (sample_rate > 0) {
      usc_task_control->src0 |=
         ((uint64_t)sample_rate)
         << PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_SAMPLE_RATE_SHIFT;
   }

   if (phase_rate_change) {
      usc_task_control->src0 |=
         PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTU_SRC0_DUAL_PHASE_EN;
   }
}

/**
 * Generates the PDS pixel event program.
 *
 * \param program Pointer to the PDS pixel event program.
 * \param buffer Pointer to the buffer for the program.
 * \param gen_mode Generate either a data segment or code segment.
 * \param dev_info PVR device info structure.
 * \returns Pointer to just beyond the buffer for the program.
 */
uint32_t *
pvr_pds_generate_pixel_event(struct pvr_pds_event_program *restrict program,
                             uint32_t *restrict buffer,
                             enum pvr_pds_generate_mode gen_mode,
                             const struct pvr_device_info *dev_info)
{
   uint32_t next_constant = PVR_PDS_CONSTANTS_BLOCK_BASE;
   uint32_t *constants = buffer;

   uint32_t data_size = 0;

   /* Copy the DMA control words and USC task control words to constants, then
    * arrange them so that the 64-bit words are together followed by the 32-bit
    * words.
    */
   uint32_t control_constant =
      pvr_pds_get_constants(&next_constant, 2, &data_size);
   uint32_t emit_constant =
      pvr_pds_get_constants(&next_constant,
                            (2 * program->num_emit_word_pairs),
                            &data_size);

   uint32_t control_word_constant =
      pvr_pds_get_constants(&next_constant,
                            program->num_emit_word_pairs,
                            &data_size);

   if (gen_mode == PDS_GENERATE_DATA_SEGMENT) {
      /* Src0 for DOUTU. */
      pvr_pds_write_wide_constant(buffer,
                                  control_constant,
                                  program->task_control.src0); /* DOUTU */
      /* 64-bit Src0. */

      /* Emit words for end of tile program. */
      for (uint32_t i = 0; i < program->num_emit_word_pairs; i++) {
         pvr_pds_write_constant64(constants,
                                  emit_constant + (2 * i),
                                  program->emit_words[(2 * i) + 0],
                                  program->emit_words[(2 * i) + 1]);
      }

      /* Control words. */
      for (uint32_t i = 0; i < program->num_emit_word_pairs; i++) {
         uint32_t doutw = pvr_pds_encode_doutw_src1(
            (2 * i),
            PVR_PDS_DOUTW_LOWER64,
            PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_DEST_COMMON_STORE,
            false,
            dev_info);

         if (i == (program->num_emit_word_pairs - 1))
            doutw |= PVR_ROGUE_PDSINST_DOUT_FIELDS_DOUTW_SRC1_LAST_EN;

         pvr_pds_write_constant32(constants, control_word_constant + i, doutw);
      }
   }

   else if (gen_mode == PDS_GENERATE_CODE_SEGMENT) {
      /* DOUTW the state into the shared register. */
      for (uint32_t i = 0; i < program->num_emit_word_pairs; i++) {
         *buffer++ = pvr_pds_encode_doutw64(
            /* cc */ 0,
            /* END */ 0,
            /* SRC1 */ (control_word_constant + i), /* DOUTW 32-bit Src1 */
            /* SRC0 */ (emit_constant + (2 * i)) >> 1); /* DOUTW 64-bit Src0
                                                         */
      }

      /* Kick the USC. */
      *buffer++ = pvr_pds_encode_doutu(
         /* cc */ 0,
         /* END */ 1,
         /* SRC0 */ control_constant >> 1);
   }

   uint32_t code_size = 1 + program->num_emit_word_pairs;

   /* Save the data segment Pointer and size. */
   program->data_segment = constants;
   program->data_size = data_size;
   program->code_size = code_size;

   if (gen_mode == PDS_GENERATE_DATA_SEGMENT)
      return (constants + next_constant);

   if (gen_mode == PDS_GENERATE_CODE_SEGMENT)
      return buffer;

   return NULL;
}
