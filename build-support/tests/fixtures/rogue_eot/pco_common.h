/*
 * Copyright © 2024 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PCO_COMMON_H
#define PCO_COMMON_H

/**
 * \file pco_common.h
 *
 * \brief PCO common definitions.
 */

#include "util/macros.h"

#include <stdbool.h>

#ifndef __OPENCL_VERSION__
#define __constant
#endif

/** Enums. */
#define _PCO_OP_TYPE_COUNT 3U
enum pco_op_type {
   PCO_OP_TYPE_PSEUDO = 0,
   PCO_OP_TYPE_HW = 1,
   PCO_OP_TYPE_HW_DIRECT = 2,
};

static inline
__constant const char *pco_op_type_str(uint64_t val) {
   switch (val) {
   case PCO_OP_TYPE_PSEUDO:
      return "pseudo";
   case PCO_OP_TYPE_HW:
      return "hw";
   case PCO_OP_TYPE_HW_DIRECT:
      return "hw_direct";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MOD_TYPE_COUNT 3U
enum pco_mod_type {
   PCO_MOD_TYPE_BOOL = 0,
   PCO_MOD_TYPE_UINT = 1,
   PCO_MOD_TYPE_ENUM = 2,
};

static inline
__constant const char *pco_mod_type_str(uint64_t val) {
   switch (val) {
   case PCO_MOD_TYPE_BOOL:
      return "bool";
   case PCO_MOD_TYPE_UINT:
      return "uint";
   case PCO_MOD_TYPE_ENUM:
      return "enum";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_REF_TYPE_COUNT 8U
enum pco_ref_type {
   PCO_REF_TYPE_NULL = 0,
   PCO_REF_TYPE_SSA = 1,
   PCO_REF_TYPE_REG = 2,
   PCO_REF_TYPE_IDX_REG = 3,
   PCO_REF_TYPE_IMM = 4,
   PCO_REF_TYPE_IO = 5,
   PCO_REF_TYPE_PRED = 6,
   PCO_REF_TYPE_DRC = 7,
};

static inline
__constant const char *pco_ref_type_str(uint64_t val) {
   switch (val) {
   case PCO_REF_TYPE_NULL:
      return "_";
   case PCO_REF_TYPE_SSA:
      return "%";
   case PCO_REF_TYPE_REG:
      return "";
   case PCO_REF_TYPE_IDX_REG:
      return "";
   case PCO_REF_TYPE_IMM:
      return "";
   case PCO_REF_TYPE_IO:
      return "";
   case PCO_REF_TYPE_PRED:
      return "";
   case PCO_REF_TYPE_DRC:
      return "drc";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_FUNC_TYPE_COUNT 4U
enum pco_func_type {
   PCO_FUNC_TYPE_CALLABLE = 0,
   PCO_FUNC_TYPE_PREAMBLE = 1,
   PCO_FUNC_TYPE_ENTRYPOINT = 2,
   PCO_FUNC_TYPE_PHASE_CHANGE = 3,
};

static inline
__constant const char *pco_func_type_str(uint64_t val) {
   switch (val) {
   case PCO_FUNC_TYPE_CALLABLE:
      return "callable";
   case PCO_FUNC_TYPE_PREAMBLE:
      return "preamble";
   case PCO_FUNC_TYPE_ENTRYPOINT:
      return "entrypoint";
   case PCO_FUNC_TYPE_PHASE_CHANGE:
      return "phase_change";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_DTYPE_COUNT 4U
enum pco_dtype {
   PCO_DTYPE_ANY = 0,
   PCO_DTYPE_UNSIGNED = 1,
   PCO_DTYPE_SIGNED = 2,
   PCO_DTYPE_FLOAT = 3,
};

static inline
__constant const char *pco_dtype_str(uint64_t val) {
   switch (val) {
   case PCO_DTYPE_ANY:
      return "";
   case PCO_DTYPE_UNSIGNED:
      return "u";
   case PCO_DTYPE_SIGNED:
      return "i";
   case PCO_DTYPE_FLOAT:
      return "f";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_BITS_COUNT 5U
enum pco_bits {
   PCO_BITS_1 = 0,
   PCO_BITS_8 = 1,
   PCO_BITS_16 = 2,
   PCO_BITS_32 = 3,
   PCO_BITS_64 = 4,
};

static inline
__constant const char *pco_bits_str(uint64_t val) {
   switch (val) {
   case PCO_BITS_1:
      return "1";
   case PCO_BITS_8:
      return "8";
   case PCO_BITS_16:
      return "16";
   case PCO_BITS_32:
      return "32";
   case PCO_BITS_64:
      return "64";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_REG_CLASS_COUNT 12U
enum pco_reg_class {
   PCO_REG_CLASS_VIRT = 0,
   PCO_REG_CLASS_TEMP = 1,
   PCO_REG_CLASS_VTXIN = 2,
   PCO_REG_CLASS_COEFF = 3,
   PCO_REG_CLASS_SHARED = 4,
   PCO_REG_CLASS_INDEX = 5,
   PCO_REG_CLASS_SPEC = 6,
   PCO_REG_CLASS_INTERN = 7,
   PCO_REG_CLASS_CONST = 8,
   PCO_REG_CLASS_PIXOUT = 9,
   PCO_REG_CLASS_GLOBAL = 10,
   PCO_REG_CLASS_SLOT = 11,
};

static inline
__constant const char *pco_reg_class_str(uint64_t val) {
   switch (val) {
   case PCO_REG_CLASS_VIRT:
      return "$";
   case PCO_REG_CLASS_TEMP:
      return "r";
   case PCO_REG_CLASS_VTXIN:
      return "vi";
   case PCO_REG_CLASS_COEFF:
      return "cf";
   case PCO_REG_CLASS_SHARED:
      return "sh";
   case PCO_REG_CLASS_INDEX:
      return "idx";
   case PCO_REG_CLASS_SPEC:
      return "sr";
   case PCO_REG_CLASS_INTERN:
      return "i";
   case PCO_REG_CLASS_CONST:
      return "sc";
   case PCO_REG_CLASS_PIXOUT:
      return "po";
   case PCO_REG_CLASS_GLOBAL:
      return "g";
   case PCO_REG_CLASS_SLOT:
      return "sl";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IO_COUNT 24U
enum pco_io {
   PCO_IO_S0 = 0,
   PCO_IO_S1 = 1,
   PCO_IO_S2 = 2,
   PCO_IO_S3 = 3,
   PCO_IO_S4 = 4,
   PCO_IO_S5 = 5,
   PCO_IO_W0 = 6,
   PCO_IO_W1 = 7,
   PCO_IO_IS0 = 8,
   PCO_IO_IS1 = 9,
   PCO_IO_IS2 = 10,
   PCO_IO_IS3 = 11,
   PCO_IO_IS4 = 12,
   PCO_IO_IS5 = 13,
   PCO_IO_FT0 = 14,
   PCO_IO_FT1 = 15,
   PCO_IO_FT2 = 16,
   PCO_IO_FTE = 17,
   PCO_IO_FT1_INVERT = 18,
   PCO_IO_FT3 = 19,
   PCO_IO_FT4 = 20,
   PCO_IO_FT5 = 21,
   PCO_IO_FTT = 22,
   PCO_IO_COUT = 23,
};

static inline
__constant const char *pco_io_str(uint64_t val) {
   switch (val) {
   case PCO_IO_S0:
      return "s0";
   case PCO_IO_S1:
      return "s1";
   case PCO_IO_S2:
      return "s2";
   case PCO_IO_S3:
      return "s3";
   case PCO_IO_S4:
      return "s4";
   case PCO_IO_S5:
      return "s5";
   case PCO_IO_W0:
      return "w0";
   case PCO_IO_W1:
      return "w1";
   case PCO_IO_IS0:
      return "is0";
   case PCO_IO_IS1:
      return "is1";
   case PCO_IO_IS2:
      return "is2";
   case PCO_IO_IS3:
      return "is3";
   case PCO_IO_IS4:
      return "is4";
   case PCO_IO_IS5:
      return "is5";
   case PCO_IO_FT0:
      return "ft0";
   case PCO_IO_FT1:
      return "ft1";
   case PCO_IO_FT2:
      return "ft2";
   case PCO_IO_FTE:
      return "fte";
   case PCO_IO_FT1_INVERT:
      return "~ft1";
   case PCO_IO_FT3:
      return "ft3";
   case PCO_IO_FT4:
      return "ft4";
   case PCO_IO_FT5:
      return "ft5";
   case PCO_IO_FTT:
      return "ftt";
   case PCO_IO_COUT:
      return "cout";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_PRED_COUNT 6U
enum pco_pred {
   PCO_PRED_PE = 0,
   PCO_PRED_P0 = 1,
   PCO_PRED_ALWAYS = 2,
   PCO_PRED_P0_TRUE = 3,
   PCO_PRED_NEVER = 4,
   PCO_PRED_P0_FALSE = 5,
};

static inline
__constant const char *pco_pred_str(uint64_t val) {
   switch (val) {
   case PCO_PRED_PE:
      return "pe";
   case PCO_PRED_P0:
      return "p0";
   case PCO_PRED_ALWAYS:
      return "if(1)";
   case PCO_PRED_P0_TRUE:
      return "if(p0)";
   case PCO_PRED_NEVER:
      return "if(0)";
   case PCO_PRED_P0_FALSE:
      return "if(!p0)";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_DRC_COUNT 3U
enum pco_drc {
   PCO_DRC_0 = 0,
   PCO_DRC_1 = 1,
   PCO_DRC_PENDING = 2,
};

static inline
__constant const char *pco_drc_str(uint64_t val) {
   switch (val) {
   case PCO_DRC_0:
      return "0";
   case PCO_DRC_1:
      return "1";
   case PCO_DRC_PENDING:
      return "?";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MUTEX_ID_COUNT 16U
enum pco_mutex_id {
   PCO_MUTEX_ID_ATOMIC_EMU = 0,
   PCO_MUTEX_ID_BARRIER = 1,
   PCO_MUTEX_ID_RSVD_2 = 2,
   PCO_MUTEX_ID_RSVD_3 = 3,
   PCO_MUTEX_ID_RSVD_4 = 4,
   PCO_MUTEX_ID_RSVD_5 = 5,
   PCO_MUTEX_ID_RSVD_6 = 6,
   PCO_MUTEX_ID_RSVD_7 = 7,
   PCO_MUTEX_ID_RSVD_8 = 8,
   PCO_MUTEX_ID_RSVD_9 = 9,
   PCO_MUTEX_ID_RSVD_A = 10,
   PCO_MUTEX_ID_RSVD_B = 11,
   PCO_MUTEX_ID_RSVD_C = 12,
   PCO_MUTEX_ID_RSVD_D = 13,
   PCO_MUTEX_ID_RSVD_E = 14,
   PCO_MUTEX_ID_RSVD_F = 15,
};

static inline
__constant const char *pco_mutex_id_str(uint64_t val) {
   switch (val) {
   case PCO_MUTEX_ID_ATOMIC_EMU:
      return "atomic_emu";
   case PCO_MUTEX_ID_BARRIER:
      return "barrier";
   case PCO_MUTEX_ID_RSVD_2:
      return "rsvd_2";
   case PCO_MUTEX_ID_RSVD_3:
      return "rsvd_3";
   case PCO_MUTEX_ID_RSVD_4:
      return "rsvd_4";
   case PCO_MUTEX_ID_RSVD_5:
      return "rsvd_5";
   case PCO_MUTEX_ID_RSVD_6:
      return "rsvd_6";
   case PCO_MUTEX_ID_RSVD_7:
      return "rsvd_7";
   case PCO_MUTEX_ID_RSVD_8:
      return "rsvd_8";
   case PCO_MUTEX_ID_RSVD_9:
      return "rsvd_9";
   case PCO_MUTEX_ID_RSVD_A:
      return "rsvd_a";
   case PCO_MUTEX_ID_RSVD_B:
      return "rsvd_b";
   case PCO_MUTEX_ID_RSVD_C:
      return "rsvd_c";
   case PCO_MUTEX_ID_RSVD_D:
      return "rsvd_d";
   case PCO_MUTEX_ID_RSVD_E:
      return "rsvd_e";
   case PCO_MUTEX_ID_RSVD_F:
      return "rsvd_f";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_ELEM_COUNT 4U
enum pco_elem {
   PCO_ELEM_E0 = 0x1,
   PCO_ELEM_E1 = 0x2,
   PCO_ELEM_E2 = 0x4,
   PCO_ELEM_E3 = 0x8,
};

static inline
__constant const char *pco_elem_str(uint64_t val) {
   switch (val) {
   case PCO_ELEM_E0:
      return "e0";
   case PCO_ELEM_E1:
      return "e1";
   case PCO_ELEM_E2:
      return "e2";
   case PCO_ELEM_E3:
      return "e3";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_EXEC_CND_COUNT 4U
enum pco_exec_cnd {
   PCO_EXEC_CND_E1_ZX = 0,
   PCO_EXEC_CND_E1_Z1 = 1,
   PCO_EXEC_CND_EX_ZX = 2,
   PCO_EXEC_CND_E1_Z0 = 3,
};

static inline
__constant const char *pco_exec_cnd_str(uint64_t val) {
   switch (val) {
   case PCO_EXEC_CND_E1_ZX:
      return "";
   case PCO_EXEC_CND_E1_Z1:
      return "if(p0)";
   case PCO_EXEC_CND_EX_ZX:
      return "(ignorepe)";
   case PCO_EXEC_CND_E1_Z0:
      return "if(!p0)";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_TST_TYPE_MAIN_COUNT 7U
enum pco_tst_type_main {
   PCO_TST_TYPE_MAIN_F32 = 0,
   PCO_TST_TYPE_MAIN_U16 = 1,
   PCO_TST_TYPE_MAIN_S16 = 2,
   PCO_TST_TYPE_MAIN_U8 = 3,
   PCO_TST_TYPE_MAIN_S8 = 4,
   PCO_TST_TYPE_MAIN_U32 = 5,
   PCO_TST_TYPE_MAIN_S32 = 6,
};

static inline
__constant const char *pco_tst_type_main_str(uint64_t val) {
   switch (val) {
   case PCO_TST_TYPE_MAIN_F32:
      return "f32";
   case PCO_TST_TYPE_MAIN_U16:
      return "u16";
   case PCO_TST_TYPE_MAIN_S16:
      return "s16";
   case PCO_TST_TYPE_MAIN_U8:
      return "u8";
   case PCO_TST_TYPE_MAIN_S8:
      return "s8";
   case PCO_TST_TYPE_MAIN_U32:
      return "u32";
   case PCO_TST_TYPE_MAIN_S32:
      return "s32";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_TST_OP_MAIN_COUNT 10U
enum pco_tst_op_main {
   PCO_TST_OP_MAIN_ZERO = 0,
   PCO_TST_OP_MAIN_GZERO = 1,
   PCO_TST_OP_MAIN_GEZERO = 2,
   PCO_TST_OP_MAIN_CARRY = 3,
   PCO_TST_OP_MAIN_EQUAL = 4,
   PCO_TST_OP_MAIN_GREATER = 5,
   PCO_TST_OP_MAIN_GEQUAL = 6,
   PCO_TST_OP_MAIN_NOTEQUAL = 7,
   PCO_TST_OP_MAIN_LESS = 8,
   PCO_TST_OP_MAIN_LEQUAL = 9,
};

static inline
__constant const char *pco_tst_op_main_str(uint64_t val) {
   switch (val) {
   case PCO_TST_OP_MAIN_ZERO:
      return "z";
   case PCO_TST_OP_MAIN_GZERO:
      return "gz";
   case PCO_TST_OP_MAIN_GEZERO:
      return "gez";
   case PCO_TST_OP_MAIN_CARRY:
      return "c";
   case PCO_TST_OP_MAIN_EQUAL:
      return "e";
   case PCO_TST_OP_MAIN_GREATER:
      return "g";
   case PCO_TST_OP_MAIN_GEQUAL:
      return "ge";
   case PCO_TST_OP_MAIN_NOTEQUAL:
      return "ne";
   case PCO_TST_OP_MAIN_LESS:
      return "l";
   case PCO_TST_OP_MAIN_LEQUAL:
      return "le";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_TST_OP_BITWISE_COUNT 2U
enum pco_tst_op_bitwise {
   PCO_TST_OP_BITWISE_ZERO = 0,
   PCO_TST_OP_BITWISE_NONZERO = 1,
};

static inline
__constant const char *pco_tst_op_bitwise_str(uint64_t val) {
   switch (val) {
   case PCO_TST_OP_BITWISE_ZERO:
      return "z";
   case PCO_TST_OP_BITWISE_NONZERO:
      return "nz";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SIGNPOS_COUNT 4U
enum pco_signpos {
   PCO_SIGNPOS_TWB = 0,
   PCO_SIGNPOS_PWB = 1,
   PCO_SIGNPOS_MTB = 2,
   PCO_SIGNPOS_FTB = 3,
};

static inline
__constant const char *pco_signpos_str(uint64_t val) {
   switch (val) {
   case PCO_SIGNPOS_TWB:
      return "twb";
   case PCO_SIGNPOS_PWB:
      return "pwb";
   case PCO_SIGNPOS_MTB:
      return "mtb";
   case PCO_SIGNPOS_FTB:
      return "ftb";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_DIM_COUNT 3U
enum pco_dim {
   PCO_DIM_1D = 0,
   PCO_DIM_2D = 1,
   PCO_DIM_3D = 2,
};

static inline
__constant const char *pco_dim_str(uint64_t val) {
   switch (val) {
   case PCO_DIM_1D:
      return "1d";
   case PCO_DIM_2D:
      return "2d";
   case PCO_DIM_3D:
      return "3d";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_LOD_MODE_COUNT 4U
enum pco_lod_mode {
   PCO_LOD_MODE_NORMAL = 0,
   PCO_LOD_MODE_BIAS = 1,
   PCO_LOD_MODE_REPLACE = 2,
   PCO_LOD_MODE_GRADIENTS = 3,
};

static inline
__constant const char *pco_lod_mode_str(uint64_t val) {
   switch (val) {
   case PCO_LOD_MODE_NORMAL:
      return "";
   case PCO_LOD_MODE_BIAS:
      return "bias";
   case PCO_LOD_MODE_REPLACE:
      return "replace";
   case PCO_LOD_MODE_GRADIENTS:
      return "gradients";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SB_MODE_COUNT 4U
enum pco_sb_mode {
   PCO_SB_MODE_NONE = 0,
   PCO_SB_MODE_RAWDATA = 1,
   PCO_SB_MODE_COEFFS = 2,
   PCO_SB_MODE_BOTH = 3,
};

static inline
__constant const char *pco_sb_mode_str(uint64_t val) {
   switch (val) {
   case PCO_SB_MODE_NONE:
      return "";
   case PCO_SB_MODE_RAWDATA:
      return "rawdata";
   case PCO_SB_MODE_COEFFS:
      return "coeffs";
   case PCO_SB_MODE_BOTH:
      return "both";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SCHEDSWAP_COUNT 2U
enum pco_schedswap {
   PCO_SCHEDSWAP_NONE = 0,
   PCO_SCHEDSWAP_SWAP = 1,
};

static inline
__constant const char *pco_schedswap_str(uint64_t val) {
   switch (val) {
   case PCO_SCHEDSWAP_NONE:
      return "";
   case PCO_SCHEDSWAP_SWAP:
      return "schedswap";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SAVMSK_MODE_COUNT 5U
enum pco_savmsk_mode {
   PCO_SAVMSK_MODE_VM = 0,
   PCO_SAVMSK_MODE_ICM = 1,
   PCO_SAVMSK_MODE_ICMOC = 2,
   PCO_SAVMSK_MODE_ICMI = 3,
   PCO_SAVMSK_MODE_CAXY = 4,
};

static inline
__constant const char *pco_savmsk_mode_str(uint64_t val) {
   switch (val) {
   case PCO_SAVMSK_MODE_VM:
      return "vm";
   case PCO_SAVMSK_MODE_ICM:
      return "icm";
   case PCO_SAVMSK_MODE_ICMOC:
      return "icmoc";
   case PCO_SAVMSK_MODE_ICMI:
      return "icmi";
   case PCO_SAVMSK_MODE_CAXY:
      return "caxy";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_ATOM_OP_COUNT 11U
enum pco_atom_op {
   PCO_ATOM_OP_ADD = 0,
   PCO_ATOM_OP_SUB = 1,
   PCO_ATOM_OP_XCHG = 2,
   PCO_ATOM_OP_CMPXCHG = 3,
   PCO_ATOM_OP_UMIN = 4,
   PCO_ATOM_OP_IMIN = 5,
   PCO_ATOM_OP_UMAX = 6,
   PCO_ATOM_OP_IMAX = 7,
   PCO_ATOM_OP_AND = 8,
   PCO_ATOM_OP_OR = 9,
   PCO_ATOM_OP_XOR = 10,
};

static inline
__constant const char *pco_atom_op_str(uint64_t val) {
   switch (val) {
   case PCO_ATOM_OP_ADD:
      return "add";
   case PCO_ATOM_OP_SUB:
      return "sub";
   case PCO_ATOM_OP_XCHG:
      return "xchg";
   case PCO_ATOM_OP_CMPXCHG:
      return "cmpxchg";
   case PCO_ATOM_OP_UMIN:
      return "umin";
   case PCO_ATOM_OP_IMIN:
      return "imin";
   case PCO_ATOM_OP_UMAX:
      return "umax";
   case PCO_ATOM_OP_IMAX:
      return "imax";
   case PCO_ATOM_OP_AND:
      return "and";
   case PCO_ATOM_OP_OR:
      return "or";
   case PCO_ATOM_OP_XOR:
      return "xor";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MCU_CACHE_MODE_LD_COUNT 3U
enum pco_mcu_cache_mode_ld {
   PCO_MCU_CACHE_MODE_LD_NORMAL = 0,
   PCO_MCU_CACHE_MODE_LD_BYPASS = 1,
   PCO_MCU_CACHE_MODE_LD_FORCE_LINE_FILL = 2,
};

static inline
__constant const char *pco_mcu_cache_mode_ld_str(uint64_t val) {
   switch (val) {
   case PCO_MCU_CACHE_MODE_LD_NORMAL:
      return "";
   case PCO_MCU_CACHE_MODE_LD_BYPASS:
      return "bypass";
   case PCO_MCU_CACHE_MODE_LD_FORCE_LINE_FILL:
      return "forcelinefill";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MCU_CACHE_MODE_ST_COUNT 3U
enum pco_mcu_cache_mode_st {
   PCO_MCU_CACHE_MODE_ST_WRITE_THROUGH = 0,
   PCO_MCU_CACHE_MODE_ST_WRITE_BACK = 1,
   PCO_MCU_CACHE_MODE_ST_LAZY_WRITE_BACK = 2,
};

static inline
__constant const char *pco_mcu_cache_mode_st_str(uint64_t val) {
   switch (val) {
   case PCO_MCU_CACHE_MODE_ST_WRITE_THROUGH:
      return "writethrough";
   case PCO_MCU_CACHE_MODE_ST_WRITE_BACK:
      return "writeback";
   case PCO_MCU_CACHE_MODE_ST_LAZY_WRITE_BACK:
      return "lazywriteback";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_BRANCH_CND_COUNT 3U
enum pco_branch_cnd {
   PCO_BRANCH_CND_EXEC_COND = 0,
   PCO_BRANCH_CND_ALLINST = 1,
   PCO_BRANCH_CND_ANYINST = 2,
};

static inline
__constant const char *pco_branch_cnd_str(uint64_t val) {
   switch (val) {
   case PCO_BRANCH_CND_EXEC_COND:
      return "";
   case PCO_BRANCH_CND_ALLINST:
      return "allinst";
   case PCO_BRANCH_CND_ANYINST:
      return "anyinst";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_PCK_FMT_COUNT 25U
enum pco_pck_fmt {
   PCO_PCK_FMT_U8888 = 0,
   PCO_PCK_FMT_S8888 = 1,
   PCO_PCK_FMT_O8888 = 2,
   PCO_PCK_FMT_U1616 = 3,
   PCO_PCK_FMT_S1616 = 4,
   PCO_PCK_FMT_O1616 = 5,
   PCO_PCK_FMT_U32 = 6,
   PCO_PCK_FMT_S32 = 7,
   PCO_PCK_FMT_U1010102 = 8,
   PCO_PCK_FMT_S1010102 = 9,
   PCO_PCK_FMT_U111110 = 10,
   PCO_PCK_FMT_S111110 = 11,
   PCO_PCK_FMT_F111110 = 12,
   PCO_PCK_FMT_F16F16 = 13,
   PCO_PCK_FMT_F32 = 14,
   PCO_PCK_FMT_COV = 15,
   PCO_PCK_FMT_U565U565 = 16,
   PCO_PCK_FMT_D24S8 = 17,
   PCO_PCK_FMT_S8D24 = 18,
   PCO_PCK_FMT_F32_MASK = 19,
   PCO_PCK_FMT_2F10F10F10 = 20,
   PCO_PCK_FMT_S8888OGL = 21,
   PCO_PCK_FMT_S1616OGL = 22,
   PCO_PCK_FMT_ZERO = 23,
   PCO_PCK_FMT_ONE = 24,
};

static inline
__constant const char *pco_pck_fmt_str(uint64_t val) {
   switch (val) {
   case PCO_PCK_FMT_U8888:
      return "u8888";
   case PCO_PCK_FMT_S8888:
      return "s8888";
   case PCO_PCK_FMT_O8888:
      return "o8888";
   case PCO_PCK_FMT_U1616:
      return "u1616";
   case PCO_PCK_FMT_S1616:
      return "s1616";
   case PCO_PCK_FMT_O1616:
      return "o1616";
   case PCO_PCK_FMT_U32:
      return "u32";
   case PCO_PCK_FMT_S32:
      return "s32";
   case PCO_PCK_FMT_U1010102:
      return "u1010102";
   case PCO_PCK_FMT_S1010102:
      return "s1010102";
   case PCO_PCK_FMT_U111110:
      return "u111110";
   case PCO_PCK_FMT_S111110:
      return "s111110";
   case PCO_PCK_FMT_F111110:
      return "f111110";
   case PCO_PCK_FMT_F16F16:
      return "f16f16";
   case PCO_PCK_FMT_F32:
      return "f32";
   case PCO_PCK_FMT_COV:
      return "cov";
   case PCO_PCK_FMT_U565U565:
      return "u565u565";
   case PCO_PCK_FMT_D24S8:
      return "d24s8";
   case PCO_PCK_FMT_S8D24:
      return "s8d24";
   case PCO_PCK_FMT_F32_MASK:
      return "f32_mask";
   case PCO_PCK_FMT_2F10F10F10:
      return "2f10f10f10";
   case PCO_PCK_FMT_S8888OGL:
      return "s8888ogl";
   case PCO_PCK_FMT_S1616OGL:
      return "s1616ogl";
   case PCO_PCK_FMT_ZERO:
      return "zero";
   case PCO_PCK_FMT_ONE:
      return "one";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_ITR_MODE_COUNT 3U
enum pco_itr_mode {
   PCO_ITR_MODE_PIXEL = 0,
   PCO_ITR_MODE_SAMPLE = 1,
   PCO_ITR_MODE_CENTROID = 2,
};

static inline
__constant const char *pco_itr_mode_str(uint64_t val) {
   switch (val) {
   case PCO_ITR_MODE_PIXEL:
      return "pixel";
   case PCO_ITR_MODE_SAMPLE:
      return "sample";
   case PCO_ITR_MODE_CENTROID:
      return "centroid";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SCHED_COUNT 3U
enum pco_sched {
   PCO_SCHED_NONE = 0,
   PCO_SCHED_SWAP = 1,
   PCO_SCHED_WDF = 2,
};

static inline
__constant const char *pco_sched_str(uint64_t val) {
   switch (val) {
   case PCO_SCHED_NONE:
      return "";
   case PCO_SCHED_SWAP:
      return "schedswap";
   case PCO_SCHED_WDF:
      return "schedwdf";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_LOGIOP_COUNT 6U
enum pco_logiop {
   PCO_LOGIOP_OR = 0,
   PCO_LOGIOP_AND = 1,
   PCO_LOGIOP_XOR = 2,
   PCO_LOGIOP_NOR = 3,
   PCO_LOGIOP_NAND = 4,
   PCO_LOGIOP_XNOR = 5,
};

static inline
__constant const char *pco_logiop_str(uint64_t val) {
   switch (val) {
   case PCO_LOGIOP_OR:
      return "or";
   case PCO_LOGIOP_AND:
      return "and";
   case PCO_LOGIOP_XOR:
      return "xor";
   case PCO_LOGIOP_NOR:
      return "nor";
   case PCO_LOGIOP_NAND:
      return "nand";
   case PCO_LOGIOP_XNOR:
      return "xnor";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SHIFTOP_COUNT 8U
enum pco_shiftop {
   PCO_SHIFTOP_LSL = 0,
   PCO_SHIFTOP_SHR = 1,
   PCO_SHIFTOP_ROL = 2,
   PCO_SHIFTOP_CPS = 3,
   PCO_SHIFTOP_ASR_TWB = 4,
   PCO_SHIFTOP_ASR_PWB = 5,
   PCO_SHIFTOP_ASR_MTB = 6,
   PCO_SHIFTOP_ASR_FTB = 7,
};

static inline
__constant const char *pco_shiftop_str(uint64_t val) {
   switch (val) {
   case PCO_SHIFTOP_LSL:
      return "lsl";
   case PCO_SHIFTOP_SHR:
      return "shr";
   case PCO_SHIFTOP_ROL:
      return "rol";
   case PCO_SHIFTOP_CPS:
      return "cps";
   case PCO_SHIFTOP_ASR_TWB:
      return "asr_twb";
   case PCO_SHIFTOP_ASR_PWB:
      return "asr_pwb";
   case PCO_SHIFTOP_ASR_MTB:
      return "asr_mtb";
   case PCO_SHIFTOP_ASR_FTB:
      return "asr_ftb";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_CND_COUNT 4U
enum pco_cnd {
   PCO_CND_ALWAYS = 0,
   PCO_CND_P0_TRUE = 1,
   PCO_CND_NEVER = 2,
   PCO_CND_P0_FALSE = 3,
};

static inline
__constant const char *pco_cnd_str(uint64_t val) {
   switch (val) {
   case PCO_CND_ALWAYS:
      return "if(1)";
   case PCO_CND_P0_TRUE:
      return "if(p0)";
   case PCO_CND_NEVER:
      return "if(0)";
   case PCO_CND_P0_FALSE:
      return "if(!p0)";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_FRED_TYPE_COUNT 2U
enum pco_fred_type {
   PCO_FRED_TYPE_SIN = 0,
   PCO_FRED_TYPE_COS = 1,
};

static inline
__constant const char *pco_fred_type_str(uint64_t val) {
   switch (val) {
   case PCO_FRED_TYPE_SIN:
      return "sin";
   case PCO_FRED_TYPE_COS:
      return "cos";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_FRED_PART_COUNT 2U
enum pco_fred_part {
   PCO_FRED_PART_A = 0,
   PCO_FRED_PART_B = 1,
};

static inline
__constant const char *pco_fred_part_str(uint64_t val) {
   switch (val) {
   case PCO_FRED_PART_A:
      return "a";
   case PCO_FRED_PART_B:
      return "b";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MUTEX_OP_COUNT 4U
enum pco_mutex_op {
   PCO_MUTEX_OP_RELEASE = 0,
   PCO_MUTEX_OP_RELEASE_SLEEP = 1,
   PCO_MUTEX_OP_RELEASE_WAKEUP = 2,
   PCO_MUTEX_OP_LOCK = 3,
};

static inline
__constant const char *pco_mutex_op_str(uint64_t val) {
   switch (val) {
   case PCO_MUTEX_OP_RELEASE:
      return "release";
   case PCO_MUTEX_OP_RELEASE_SLEEP:
      return "release.sleep";
   case PCO_MUTEX_OP_RELEASE_WAKEUP:
      return "release.wakeup";
   case PCO_MUTEX_OP_LOCK:
      return "lock";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_OFFSET_SD_COUNT 2U
enum pco_offset_sd {
   PCO_OFFSET_SD_SRC = 0,
   PCO_OFFSET_SD_DEST = 1,
};

static inline
__constant const char *pco_offset_sd_str(uint64_t val) {
   switch (val) {
   case PCO_OFFSET_SD_SRC:
      return "src";
   case PCO_OFFSET_SD_DEST:
      return "dest";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_VOTE_OP_COUNT 2U
enum pco_vote_op {
   PCO_VOTE_OP_ALL = 0,
   PCO_VOTE_OP_ANY = 1,
};

static inline
__constant const char *pco_vote_op_str(uint64_t val) {
   switch (val) {
   case PCO_VOTE_OP_ALL:
      return "all";
   case PCO_VOTE_OP_ANY:
      return "any";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_OP_PHASE_COUNT 6U
enum pco_op_phase {
   PCO_OP_PHASE_0 = 0,
   PCO_OP_PHASE_CTRL = 0,
   PCO_OP_PHASE_1 = 1,
   PCO_OP_PHASE_2_PCK = 2,
   PCO_OP_PHASE_2 = 2,
   PCO_OP_PHASE_2_TST = 3,
   PCO_OP_PHASE_2_MOV = 4,
   PCO_OP_PHASE_BACKEND = 5,
};

static inline
__constant const char *pco_op_phase_str(uint64_t val) {
   switch (val) {
   case PCO_OP_PHASE_0:
      return "p0";
   case PCO_OP_PHASE_1:
      return "p1";
   case PCO_OP_PHASE_2_PCK:
      return "p2_pck";
   case PCO_OP_PHASE_2_TST:
      return "p2_tst";
   case PCO_OP_PHASE_2_MOV:
      return "p2_mov";
   case PCO_OP_PHASE_BACKEND:
      return "backend";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SR_COUNT 67U
enum pco_sr {
   PCO_SR_PIXOUT0 = 32,
   PCO_SR_PIXOUT1 = 33,
   PCO_SR_PIXOUT2 = 34,
   PCO_SR_PIXOUT3 = 35,
   PCO_SR_INTL0 = 36,
   PCO_SR_INTL1 = 37,
   PCO_SR_INTL2 = 38,
   PCO_SR_INTL3 = 39,
   PCO_SR_INTL4 = 40,
   PCO_SR_INTL5 = 41,
   PCO_SR_INTL6 = 42,
   PCO_SR_INTL7 = 43,
   PCO_SR_SLOT7 = 36,
   PCO_SR_SLOT6 = 37,
   PCO_SR_SLOT5 = 38,
   PCO_SR_SLOT4 = 39,
   PCO_SR_SLOT3 = 40,
   PCO_SR_SLOT2 = 41,
   PCO_SR_SLOT1 = 42,
   PCO_SR_SLOT0 = 43,
   PCO_SR_FACE_ORIENT = 44,
   PCO_SR_BACK_FACE = 44,
   PCO_SR_CLUSTER_NUM = 45,
   PCO_SR_OUTPUT_PART = 46,
   PCO_SR_TASK_ID = 47,
   PCO_SR_SLOT_NUM = 48,
   PCO_SR_TILE_X_PIX = 49,
   PCO_SR_TILE_Y_PIX = 50,
   PCO_SR_INST_NUM = 51,
   PCO_SR_DM_TASK_TYPE = 52,
   PCO_SR_SAMP_NUM = 53,
   PCO_SR_TILED_LD_COMP0 = 54,
   PCO_SR_TILED_LD_COMP1 = 55,
   PCO_SR_TILED_LD_COMP2 = 56,
   PCO_SR_TILED_LD_COMP3 = 57,
   PCO_SR_TILED_ST_COMP0 = 58,
   PCO_SR_TILED_ST_COMP1 = 59,
   PCO_SR_TILED_ST_COMP2 = 60,
   PCO_SR_TILED_ST_COMP3 = 61,
   PCO_SR_BATCH_NUM = 62,
   PCO_SR_INST_VALID = 63,
   PCO_SR_TILE_XY = 96,
   PCO_SR_X_P = 97,
   PCO_SR_X_S = 98,
   PCO_SR_Y_P = 100,
   PCO_SR_Y_S = 101,
   PCO_SR_WG_ID = 102,
   PCO_SR_SH_ALLOC_SIZE = 103,
   PCO_SR_GLOBAL0 = 104,
   PCO_SR_GLOBAL1 = 105,
   PCO_SR_GLOBAL2 = 106,
   PCO_SR_GLOBAL3 = 107,
   PCO_SR_GLOBAL4 = 108,
   PCO_SR_GLOBAL5 = 109,
   PCO_SR_GLOBAL6 = 110,
   PCO_SR_GLOBAL7 = 111,
   PCO_SR_LOCAL_ADDR_INST_NUM = 112,
   PCO_SR_TILE_X_P = 114,
   PCO_SR_TILE_X_S = 115,
   PCO_SR_TILE_Y_P = 116,
   PCO_SR_TILE_Y_S = 117,
   PCO_SR_RENDER_TGT_ID = 118,
   PCO_SR_TILED_LD_COMP4 = 119,
   PCO_SR_TILED_LD_COMP5 = 120,
   PCO_SR_TILED_LD_COMP6 = 121,
   PCO_SR_TILED_LD_COMP7 = 122,
   PCO_SR_TILED_ST_COMP4 = 123,
   PCO_SR_TILED_ST_COMP5 = 124,
   PCO_SR_TILED_ST_COMP6 = 125,
   PCO_SR_TILED_ST_COMP7 = 126,
   PCO_SR_GPU_OFFSET = 127,
   PCO_SR_TIMER_80NS = 160,
   PCO_SR_PIXOUT4 = 164,
   PCO_SR_PIXOUT5 = 165,
   PCO_SR_PIXOUT6 = 166,
   PCO_SR_PIXOUT7 = 167,
};

static inline
__constant const char *pco_sr_str(uint64_t val) {
   switch (val) {
   case PCO_SR_PIXOUT0:
      return "pixout0";
   case PCO_SR_PIXOUT1:
      return "pixout1";
   case PCO_SR_PIXOUT2:
      return "pixout2";
   case PCO_SR_PIXOUT3:
      return "pixout3";
   case PCO_SR_INTL0:
      return "intl0";
   case PCO_SR_INTL1:
      return "intl1";
   case PCO_SR_INTL2:
      return "intl2";
   case PCO_SR_INTL3:
      return "intl3";
   case PCO_SR_INTL4:
      return "intl4";
   case PCO_SR_INTL5:
      return "intl5";
   case PCO_SR_INTL6:
      return "intl6";
   case PCO_SR_INTL7:
      return "intl7";
   case PCO_SR_FACE_ORIENT:
      return "face_orient";
   case PCO_SR_CLUSTER_NUM:
      return "cluster_num";
   case PCO_SR_OUTPUT_PART:
      return "output_part";
   case PCO_SR_TASK_ID:
      return "task_id";
   case PCO_SR_SLOT_NUM:
      return "slot_num";
   case PCO_SR_TILE_X_PIX:
      return "tile_x_pix";
   case PCO_SR_TILE_Y_PIX:
      return "tile_y_pix";
   case PCO_SR_INST_NUM:
      return "inst_num";
   case PCO_SR_DM_TASK_TYPE:
      return "dm_task_type";
   case PCO_SR_SAMP_NUM:
      return "samp_num";
   case PCO_SR_TILED_LD_COMP0:
      return "tiled_ld_comp0";
   case PCO_SR_TILED_LD_COMP1:
      return "tiled_ld_comp1";
   case PCO_SR_TILED_LD_COMP2:
      return "tiled_ld_comp2";
   case PCO_SR_TILED_LD_COMP3:
      return "tiled_ld_comp3";
   case PCO_SR_TILED_ST_COMP0:
      return "tiled_st_comp0";
   case PCO_SR_TILED_ST_COMP1:
      return "tiled_st_comp1";
   case PCO_SR_TILED_ST_COMP2:
      return "tiled_st_comp2";
   case PCO_SR_TILED_ST_COMP3:
      return "tiled_st_comp3";
   case PCO_SR_BATCH_NUM:
      return "batch_num";
   case PCO_SR_INST_VALID:
      return "inst_valid";
   case PCO_SR_TILE_XY:
      return "tile_xy";
   case PCO_SR_X_P:
      return "x_p";
   case PCO_SR_X_S:
      return "x_s";
   case PCO_SR_Y_P:
      return "y_p";
   case PCO_SR_Y_S:
      return "y_s";
   case PCO_SR_WG_ID:
      return "wg_id";
   case PCO_SR_SH_ALLOC_SIZE:
      return "sh_alloc_size";
   case PCO_SR_GLOBAL0:
      return "global0";
   case PCO_SR_GLOBAL1:
      return "global1";
   case PCO_SR_GLOBAL2:
      return "global2";
   case PCO_SR_GLOBAL3:
      return "global3";
   case PCO_SR_GLOBAL4:
      return "global4";
   case PCO_SR_GLOBAL5:
      return "global5";
   case PCO_SR_GLOBAL6:
      return "global6";
   case PCO_SR_GLOBAL7:
      return "global7";
   case PCO_SR_LOCAL_ADDR_INST_NUM:
      return "local_addr_inst_num";
   case PCO_SR_TILE_X_P:
      return "tile_x_p";
   case PCO_SR_TILE_X_S:
      return "tile_x_s";
   case PCO_SR_TILE_Y_P:
      return "tile_y_p";
   case PCO_SR_TILE_Y_S:
      return "tile_y_s";
   case PCO_SR_RENDER_TGT_ID:
      return "render_tgt_id";
   case PCO_SR_TILED_LD_COMP4:
      return "tiled_ld_comp4";
   case PCO_SR_TILED_LD_COMP5:
      return "tiled_ld_comp5";
   case PCO_SR_TILED_LD_COMP6:
      return "tiled_ld_comp6";
   case PCO_SR_TILED_LD_COMP7:
      return "tiled_ld_comp7";
   case PCO_SR_TILED_ST_COMP4:
      return "tiled_st_comp4";
   case PCO_SR_TILED_ST_COMP5:
      return "tiled_st_comp5";
   case PCO_SR_TILED_ST_COMP6:
      return "tiled_st_comp6";
   case PCO_SR_TILED_ST_COMP7:
      return "tiled_st_comp7";
   case PCO_SR_GPU_OFFSET:
      return "gpu_offset";
   case PCO_SR_TIMER_80NS:
      return "timer_80ns";
   case PCO_SR_PIXOUT4:
      return "pixout4";
   case PCO_SR_PIXOUT5:
      return "pixout5";
   case PCO_SR_PIXOUT6:
      return "pixout6";
   case PCO_SR_PIXOUT7:
      return "pixout7";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_OPORG_COUNT 8U
enum pco_oporg {
   PCO_OPORG_P0 = 0,
   PCO_OPORG_P2 = 1,
   PCO_OPORG_BE = 2,
   PCO_OPORG_P0_P1 = 3,
   PCO_OPORG_P0_P2 = 4,
   PCO_OPORG_P0_P1_P2 = 5,
   PCO_OPORG_P0_P2_BE = 6,
   PCO_OPORG_P0_P1_P2_BE = 7,
};

static inline
__constant const char *pco_oporg_str(uint64_t val) {
   switch (val) {
   case PCO_OPORG_P0:
      return "p0";
   case PCO_OPORG_P2:
      return "p2";
   case PCO_OPORG_BE:
      return "be";
   case PCO_OPORG_P0_P1:
      return "p0_p1";
   case PCO_OPORG_P0_P2:
      return "p0_p2";
   case PCO_OPORG_P0_P1_P2:
      return "p0_p1_p2";
   case PCO_OPORG_P0_P2_BE:
      return "p0_p2_be";
   case PCO_OPORG_P0_P1_P2_BE:
      return "p0_p1_p2_be";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_OPCNT_COUNT 3U
enum pco_opcnt {
   PCO_OPCNT_P0 = 0x1,
   PCO_OPCNT_P1 = 0x2,
   PCO_OPCNT_P2 = 0x4,
};

static inline
__constant const char *pco_opcnt_str(uint64_t val) {
   switch (val) {
   case PCO_OPCNT_P0:
      return "p0";
   case PCO_OPCNT_P1:
      return "p1";
   case PCO_OPCNT_P2:
      return "p2";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_CC_COUNT 4U
enum pco_cc {
   PCO_CC_E1_ZX = 0,
   PCO_CC_E1_Z1 = 1,
   PCO_CC_EX_ZX = 2,
   PCO_CC_E1_Z0 = 3,
};

static inline
__constant const char *pco_cc_str(uint64_t val) {
   switch (val) {
   case PCO_CC_E1_ZX:
      return "";
   case PCO_CC_E1_Z1:
      return "if(p0)";
   case PCO_CC_EX_ZX:
      return "(ignorepe)";
   case PCO_CC_E1_Z0:
      return "if(!p0)";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_ALUTYPE_COUNT 3U
enum pco_alutype {
   PCO_ALUTYPE_MAIN = 0,
   PCO_ALUTYPE_BITWISE = 2,
   PCO_ALUTYPE_CONTROL = 3,
};

static inline
__constant const char *pco_alutype_str(uint64_t val) {
   switch (val) {
   case PCO_ALUTYPE_MAIN:
      return "main";
   case PCO_ALUTYPE_BITWISE:
      return "bitwise";
   case PCO_ALUTYPE_CONTROL:
      return "control";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_CTRLOP_COUNT 11U
enum pco_ctrlop {
   PCO_CTRLOP_B = 0,
   PCO_CTRLOP_LAPC = 1,
   PCO_CTRLOP_SAVL = 2,
   PCO_CTRLOP_CND = 3,
   PCO_CTRLOP_WOP = 4,
   PCO_CTRLOP_WDF = 5,
   PCO_CTRLOP_MUTEX = 6,
   PCO_CTRLOP_NOP = 7,
   PCO_CTRLOP_ITRSMP = 8,
   PCO_CTRLOP_SBO = 11,
   PCO_CTRLOP_DITR = 12,
};

static inline
__constant const char *pco_ctrlop_str(uint64_t val) {
   switch (val) {
   case PCO_CTRLOP_B:
      return "b";
   case PCO_CTRLOP_LAPC:
      return "lapc";
   case PCO_CTRLOP_SAVL:
      return "savl";
   case PCO_CTRLOP_CND:
      return "cnd";
   case PCO_CTRLOP_WOP:
      return "wop";
   case PCO_CTRLOP_WDF:
      return "wdf";
   case PCO_CTRLOP_MUTEX:
      return "mutex";
   case PCO_CTRLOP_NOP:
      return "nop";
   case PCO_CTRLOP_ITRSMP:
      return "itrsmp";
   case PCO_CTRLOP_SBO:
      return "sbo";
   case PCO_CTRLOP_DITR:
      return "ditr";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IS0_SEL_COUNT 6U
enum pco_is0_sel {
   PCO_IS0_SEL_S0 = 0,
   PCO_IS0_SEL_S3 = 1,
   PCO_IS0_SEL_S4 = 2,
   PCO_IS0_SEL_S5 = 3,
   PCO_IS0_SEL_S1 = 4,
   PCO_IS0_SEL_S2 = 5,
};

static inline
__constant const char *pco_is0_sel_str(uint64_t val) {
   switch (val) {
   case PCO_IS0_SEL_S0:
      return "s0";
   case PCO_IS0_SEL_S3:
      return "s3";
   case PCO_IS0_SEL_S4:
      return "s4";
   case PCO_IS0_SEL_S5:
      return "s5";
   case PCO_IS0_SEL_S1:
      return "s1";
   case PCO_IS0_SEL_S2:
      return "s2";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_REGBANK_COUNT 8U
enum pco_regbank {
   PCO_REGBANK_SPECIAL = 0,
   PCO_REGBANK_TEMP = 1,
   PCO_REGBANK_VTXIN = 2,
   PCO_REGBANK_COEFF = 3,
   PCO_REGBANK_SHARED = 4,
   PCO_REGBANK_COEFF_ALT = 5,
   PCO_REGBANK_IDX0 = 6,
   PCO_REGBANK_IDX1 = 7,
};

static inline
__constant const char *pco_regbank_str(uint64_t val) {
   switch (val) {
   case PCO_REGBANK_SPECIAL:
      return "special";
   case PCO_REGBANK_TEMP:
      return "temp";
   case PCO_REGBANK_VTXIN:
      return "vtxin";
   case PCO_REGBANK_COEFF:
      return "coeff";
   case PCO_REGBANK_SHARED:
      return "shared";
   case PCO_REGBANK_COEFF_ALT:
      return "coeff_alt";
   case PCO_REGBANK_IDX0:
      return "idx0";
   case PCO_REGBANK_IDX1:
      return "idx1";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IDXBANK_COUNT 7U
enum pco_idxbank {
   PCO_IDXBANK_TEMP = 0,
   PCO_IDXBANK_VTXIN = 1,
   PCO_IDXBANK_COEFF = 2,
   PCO_IDXBANK_SHARED = 3,
   PCO_IDXBANK_IDX = 5,
   PCO_IDXBANK_COEFF_ALT = 6,
   PCO_IDXBANK_PIXOUT = 7,
};

static inline
__constant const char *pco_idxbank_str(uint64_t val) {
   switch (val) {
   case PCO_IDXBANK_TEMP:
      return "temp";
   case PCO_IDXBANK_VTXIN:
      return "vtxin";
   case PCO_IDXBANK_COEFF:
      return "coeff";
   case PCO_IDXBANK_SHARED:
      return "shared";
   case PCO_IDXBANK_IDX:
      return "idx";
   case PCO_IDXBANK_COEFF_ALT:
      return "coeff_alt";
   case PCO_IDXBANK_PIXOUT:
      return "pixout";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IS5_SEL_COUNT 4U
enum pco_is5_sel {
   PCO_IS5_SEL_FT0 = 0,
   PCO_IS5_SEL_FT1 = 1,
   PCO_IS5_SEL_FT2 = 2,
   PCO_IS5_SEL_FTE = 3,
};

static inline
__constant const char *pco_is5_sel_str(uint64_t val) {
   switch (val) {
   case PCO_IS5_SEL_FT0:
      return "ft0";
   case PCO_IS5_SEL_FT1:
      return "ft1";
   case PCO_IS5_SEL_FT2:
      return "ft2";
   case PCO_IS5_SEL_FTE:
      return "fte";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IS4_SEL_COUNT 4U
enum pco_is4_sel {
   PCO_IS4_SEL_FT0 = 0,
   PCO_IS4_SEL_FT1 = 1,
   PCO_IS4_SEL_FT2 = 2,
   PCO_IS4_SEL_FTE = 3,
};

static inline
__constant const char *pco_is4_sel_str(uint64_t val) {
   switch (val) {
   case PCO_IS4_SEL_FT0:
      return "ft0";
   case PCO_IS4_SEL_FT1:
      return "ft1";
   case PCO_IS4_SEL_FT2:
      return "ft2";
   case PCO_IS4_SEL_FTE:
      return "fte";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IS3_SEL_COUNT 3U
enum pco_is3_sel {
   PCO_IS3_SEL_FT0 = 0,
   PCO_IS3_SEL_FT1 = 1,
   PCO_IS3_SEL_FTE = 3,
};

static inline
__constant const char *pco_is3_sel_str(uint64_t val) {
   switch (val) {
   case PCO_IS3_SEL_FT0:
      return "ft0";
   case PCO_IS3_SEL_FT1:
      return "ft1";
   case PCO_IS3_SEL_FTE:
      return "fte";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IS2_SEL_COUNT 2U
enum pco_is2_sel {
   PCO_IS2_SEL_FT1 = 0,
   PCO_IS2_SEL_FTE = 1,
};

static inline
__constant const char *pco_is2_sel_str(uint64_t val) {
   switch (val) {
   case PCO_IS2_SEL_FT1:
      return "ft1";
   case PCO_IS2_SEL_FTE:
      return "fte";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IS1_SEL_COUNT 2U
enum pco_is1_sel {
   PCO_IS1_SEL_FT0 = 0,
   PCO_IS1_SEL_FTE = 1,
};

static inline
__constant const char *pco_is1_sel_str(uint64_t val) {
   switch (val) {
   case PCO_IS1_SEL_FT0:
      return "ft0";
   case PCO_IS1_SEL_FTE:
      return "fte";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MAIN_OP_COUNT 8U
enum pco_main_op {
   PCO_MAIN_OP_FADD = 0,
   PCO_MAIN_OP_FADD_LP = 1,
   PCO_MAIN_OP_FMUL = 2,
   PCO_MAIN_OP_FMUL_LP = 3,
   PCO_MAIN_OP_SNGL = 4,
   PCO_MAIN_OP_INT8_16 = 5,
   PCO_MAIN_OP_FMAD_MOVC = 6,
   PCO_MAIN_OP_INT32_64_TST = 7,
};

static inline
__constant const char *pco_main_op_str(uint64_t val) {
   switch (val) {
   case PCO_MAIN_OP_FADD:
      return "fadd";
   case PCO_MAIN_OP_FADD_LP:
      return "fadd_lp";
   case PCO_MAIN_OP_FMUL:
      return "fmul";
   case PCO_MAIN_OP_FMUL_LP:
      return "fmul_lp";
   case PCO_MAIN_OP_SNGL:
      return "sngl";
   case PCO_MAIN_OP_INT8_16:
      return "int8_16";
   case PCO_MAIN_OP_FMAD_MOVC:
      return "fmad_movc";
   case PCO_MAIN_OP_INT32_64_TST:
      return "int32_64_tst";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SNGL_OP_COUNT 16U
enum pco_sngl_op {
   PCO_SNGL_OP_RCP = 0,
   PCO_SNGL_OP_RSQ = 1,
   PCO_SNGL_OP_LOG = 2,
   PCO_SNGL_OP_EXP = 3,
   PCO_SNGL_OP_F16SOP = 4,
   PCO_SNGL_OP_LOGCN = 5,
   PCO_SNGL_OP_GAMMA = 6,
   PCO_SNGL_OP_BYP = 7,
   PCO_SNGL_OP_DSX = 8,
   PCO_SNGL_OP_DSY = 9,
   PCO_SNGL_OP_DSXF = 10,
   PCO_SNGL_OP_DSYF = 11,
   PCO_SNGL_OP_PCK = 12,
   PCO_SNGL_OP_RED = 13,
   PCO_SNGL_OP_SINC = 14,
   PCO_SNGL_OP_ARCTANC = 15,
};

static inline
__constant const char *pco_sngl_op_str(uint64_t val) {
   switch (val) {
   case PCO_SNGL_OP_RCP:
      return "rcp";
   case PCO_SNGL_OP_RSQ:
      return "rsq";
   case PCO_SNGL_OP_LOG:
      return "log";
   case PCO_SNGL_OP_EXP:
      return "exp";
   case PCO_SNGL_OP_F16SOP:
      return "f16sop";
   case PCO_SNGL_OP_LOGCN:
      return "logcn";
   case PCO_SNGL_OP_GAMMA:
      return "gamma";
   case PCO_SNGL_OP_BYP:
      return "byp";
   case PCO_SNGL_OP_DSX:
      return "dsx";
   case PCO_SNGL_OP_DSY:
      return "dsy";
   case PCO_SNGL_OP_DSXF:
      return "dsxf";
   case PCO_SNGL_OP_DSYF:
      return "dsyf";
   case PCO_SNGL_OP_PCK:
      return "pck";
   case PCO_SNGL_OP_RED:
      return "red";
   case PCO_SNGL_OP_SINC:
      return "sinc";
   case PCO_SNGL_OP_ARCTANC:
      return "arctanc";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_RED_PART_COUNT 2U
enum pco_red_part {
   PCO_RED_PART_A = 0,
   PCO_RED_PART_B = 1,
};

static inline
__constant const char *pco_red_part_str(uint64_t val) {
   switch (val) {
   case PCO_RED_PART_A:
      return "a";
   case PCO_RED_PART_B:
      return "b";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_RED_TYPE_COUNT 2U
enum pco_red_type {
   PCO_RED_TYPE_SIN = 0,
   PCO_RED_TYPE_COS = 1,
};

static inline
__constant const char *pco_red_type_str(uint64_t val) {
   switch (val) {
   case PCO_RED_TYPE_SIN:
      return "sin";
   case PCO_RED_TYPE_COS:
      return "cos";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_GAMMA_OP_COUNT 2U
enum pco_gamma_op {
   PCO_GAMMA_OP_CMP = 0,
   PCO_GAMMA_OP_EXP = 1,
};

static inline
__constant const char *pco_gamma_op_str(uint64_t val) {
   switch (val) {
   case PCO_GAMMA_OP_CMP:
      return "cmp";
   case PCO_GAMMA_OP_EXP:
      return "exp";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_UPCK_ELEM_COUNT 4U
enum pco_upck_elem {
   PCO_UPCK_ELEM_E0 = 0,
   PCO_UPCK_ELEM_E1 = 1,
   PCO_UPCK_ELEM_E2 = 2,
   PCO_UPCK_ELEM_E3 = 3,
};

static inline
__constant const char *pco_upck_elem_str(uint64_t val) {
   switch (val) {
   case PCO_UPCK_ELEM_E0:
      return "e0";
   case PCO_UPCK_ELEM_E1:
      return "e1";
   case PCO_UPCK_ELEM_E2:
      return "e2";
   case PCO_UPCK_ELEM_E3:
      return "e3";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_PCK_FORMAT_COUNT 25U
enum pco_pck_format {
   PCO_PCK_FORMAT_U8888 = 0,
   PCO_PCK_FORMAT_S8888 = 1,
   PCO_PCK_FORMAT_O8888 = 2,
   PCO_PCK_FORMAT_U1616 = 3,
   PCO_PCK_FORMAT_S1616 = 4,
   PCO_PCK_FORMAT_O1616 = 5,
   PCO_PCK_FORMAT_U32 = 6,
   PCO_PCK_FORMAT_S32 = 7,
   PCO_PCK_FORMAT_U1010102 = 8,
   PCO_PCK_FORMAT_S1010102 = 9,
   PCO_PCK_FORMAT_U111110 = 10,
   PCO_PCK_FORMAT_S111110 = 11,
   PCO_PCK_FORMAT_F111110 = 12,
   PCO_PCK_FORMAT_F16F16 = 14,
   PCO_PCK_FORMAT_F32 = 15,
   PCO_PCK_FORMAT_COV = 16,
   PCO_PCK_FORMAT_U565U565 = 17,
   PCO_PCK_FORMAT_D24S8 = 18,
   PCO_PCK_FORMAT_S8D24 = 19,
   PCO_PCK_FORMAT_F32_MASK = 20,
   PCO_PCK_FORMAT_2F10F10F10 = 21,
   PCO_PCK_FORMAT_S8888OGL = 22,
   PCO_PCK_FORMAT_S1616OGL = 23,
   PCO_PCK_FORMAT_ZERO = 30,
   PCO_PCK_FORMAT_ONE = 31,
};

static inline
__constant const char *pco_pck_format_str(uint64_t val) {
   switch (val) {
   case PCO_PCK_FORMAT_U8888:
      return "u8888";
   case PCO_PCK_FORMAT_S8888:
      return "s8888";
   case PCO_PCK_FORMAT_O8888:
      return "o8888";
   case PCO_PCK_FORMAT_U1616:
      return "u1616";
   case PCO_PCK_FORMAT_S1616:
      return "s1616";
   case PCO_PCK_FORMAT_O1616:
      return "o1616";
   case PCO_PCK_FORMAT_U32:
      return "u32";
   case PCO_PCK_FORMAT_S32:
      return "s32";
   case PCO_PCK_FORMAT_U1010102:
      return "u1010102";
   case PCO_PCK_FORMAT_S1010102:
      return "s1010102";
   case PCO_PCK_FORMAT_U111110:
      return "u111110";
   case PCO_PCK_FORMAT_S111110:
      return "s111110";
   case PCO_PCK_FORMAT_F111110:
      return "f111110";
   case PCO_PCK_FORMAT_F16F16:
      return "f16f16";
   case PCO_PCK_FORMAT_F32:
      return "f32";
   case PCO_PCK_FORMAT_COV:
      return "cov";
   case PCO_PCK_FORMAT_U565U565:
      return "u565u565";
   case PCO_PCK_FORMAT_D24S8:
      return "d24s8";
   case PCO_PCK_FORMAT_S8D24:
      return "s8d24";
   case PCO_PCK_FORMAT_F32_MASK:
      return "f32_mask";
   case PCO_PCK_FORMAT_2F10F10F10:
      return "2f10f10f10";
   case PCO_PCK_FORMAT_S8888OGL:
      return "s8888ogl";
   case PCO_PCK_FORMAT_S1616OGL:
      return "s1616ogl";
   case PCO_PCK_FORMAT_ZERO:
      return "zero";
   case PCO_PCK_FORMAT_ONE:
      return "one";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_INT8_16_OP_COUNT 4U
enum pco_int8_16_op {
   PCO_INT8_16_OP_ADD = 0,
   PCO_INT8_16_OP_MUL = 1,
   PCO_INT8_16_OP_MAD_0_1 = 2,
   PCO_INT8_16_OP_MAD_2_3 = 3,
};

static inline
__constant const char *pco_int8_16_op_str(uint64_t val) {
   switch (val) {
   case PCO_INT8_16_OP_ADD:
      return "add";
   case PCO_INT8_16_OP_MUL:
      return "mul";
   case PCO_INT8_16_OP_MAD_0_1:
      return "mad_0_1";
   case PCO_INT8_16_OP_MAD_2_3:
      return "mad_2_3";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_INT8_16_FMT_COUNT 2U
enum pco_int8_16_fmt {
   PCO_INT8_16_FMT_8BIT = 0,
   PCO_INT8_16_FMT_16BIT = 1,
};

static inline
__constant const char *pco_int8_16_fmt_str(uint64_t val) {
   switch (val) {
   case PCO_INT8_16_FMT_8BIT:
      return "8bit";
   case PCO_INT8_16_FMT_16BIT:
      return "16bit";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_S2CH_COUNT 2U
enum pco_s2ch {
   PCO_S2CH_ELO = 0,
   PCO_S2CH_EHI = 1,
};

static inline
__constant const char *pco_s2ch_str(uint64_t val) {
   switch (val) {
   case PCO_S2CH_ELO:
      return "elo";
   case PCO_S2CH_EHI:
      return "ehi";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_S01CH_COUNT 4U
enum pco_s01ch {
   PCO_S01CH_E0 = 0,
   PCO_S01CH_E1 = 1,
   PCO_S01CH_E2 = 2,
   PCO_S01CH_E3 = 3,
};

static inline
__constant const char *pco_s01ch_str(uint64_t val) {
   switch (val) {
   case PCO_S01CH_E0:
      return "e0";
   case PCO_S01CH_E1:
      return "e1";
   case PCO_S01CH_E2:
      return "e2";
   case PCO_S01CH_E3:
      return "e3";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MOVW01_COUNT 4U
enum pco_movw01 {
   PCO_MOVW01_FT0 = 0,
   PCO_MOVW01_FT1 = 1,
   PCO_MOVW01_FT2 = 2,
   PCO_MOVW01_FTE = 3,
};

static inline
__constant const char *pco_movw01_str(uint64_t val) {
   switch (val) {
   case PCO_MOVW01_FT0:
      return "ft0";
   case PCO_MOVW01_FT1:
      return "ft1";
   case PCO_MOVW01_FT2:
      return "ft2";
   case PCO_MOVW01_FTE:
      return "fte";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MASKW0_COUNT 4U
enum pco_maskw0 {
   PCO_MASKW0_E0 = 0x1,
   PCO_MASKW0_E1 = 0x2,
   PCO_MASKW0_E2 = 0x4,
   PCO_MASKW0_E3 = 0x8,
   PCO_MASKW0_EALL = 0xf,
};

static inline
__constant const char *pco_maskw0_str(uint64_t val) {
   switch (val) {
   case PCO_MASKW0_E0:
      return "e0";
   case PCO_MASKW0_E1:
      return "e1";
   case PCO_MASKW0_E2:
      return "e2";
   case PCO_MASKW0_E3:
      return "e3";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_INT32_64_OP_COUNT 4U
enum pco_int32_64_op {
   PCO_INT32_64_OP_ADD6432 = 0,
   PCO_INT32_64_OP_ADD64 = 1,
   PCO_INT32_64_OP_MADD32 = 2,
   PCO_INT32_64_OP_MADD64 = 3,
};

static inline
__constant const char *pco_int32_64_op_str(uint64_t val) {
   switch (val) {
   case PCO_INT32_64_OP_ADD6432:
      return "add6432";
   case PCO_INT32_64_OP_ADD64:
      return "add64";
   case PCO_INT32_64_OP_MADD32:
      return "madd32";
   case PCO_INT32_64_OP_MADD64:
      return "madd64";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_TST_OP_COUNT 10U
enum pco_tst_op {
   PCO_TST_OP_Z = 0,
   PCO_TST_OP_GZ = 1,
   PCO_TST_OP_GEZ = 2,
   PCO_TST_OP_C = 3,
   PCO_TST_OP_E = 4,
   PCO_TST_OP_G = 5,
   PCO_TST_OP_GE = 6,
   PCO_TST_OP_NE = 7,
   PCO_TST_OP_L = 8,
   PCO_TST_OP_LE = 9,
};

static inline
__constant const char *pco_tst_op_str(uint64_t val) {
   switch (val) {
   case PCO_TST_OP_Z:
      return "z";
   case PCO_TST_OP_GZ:
      return "gz";
   case PCO_TST_OP_GEZ:
      return "gez";
   case PCO_TST_OP_C:
      return "c";
   case PCO_TST_OP_E:
      return "e";
   case PCO_TST_OP_G:
      return "g";
   case PCO_TST_OP_GE:
      return "ge";
   case PCO_TST_OP_NE:
      return "ne";
   case PCO_TST_OP_L:
      return "l";
   case PCO_TST_OP_LE:
      return "le";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_TST_TYPE_COUNT 7U
enum pco_tst_type {
   PCO_TST_TYPE_F32 = 0,
   PCO_TST_TYPE_U16 = 1,
   PCO_TST_TYPE_S16 = 2,
   PCO_TST_TYPE_U8 = 3,
   PCO_TST_TYPE_S8 = 4,
   PCO_TST_TYPE_U32 = 5,
   PCO_TST_TYPE_S32 = 6,
};

static inline
__constant const char *pco_tst_type_str(uint64_t val) {
   switch (val) {
   case PCO_TST_TYPE_F32:
      return "f32";
   case PCO_TST_TYPE_U16:
      return "u16";
   case PCO_TST_TYPE_S16:
      return "s16";
   case PCO_TST_TYPE_U8:
      return "u8";
   case PCO_TST_TYPE_S8:
      return "s8";
   case PCO_TST_TYPE_U32:
      return "u32";
   case PCO_TST_TYPE_S32:
      return "s32";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_BACKEND_OP_COUNT 8U
enum pco_backend_op {
   PCO_BACKEND_OP_UVSW = 0,
   PCO_BACKEND_OP_MSK = 1,
   PCO_BACKEND_OP_PHAS = 2,
   PCO_BACKEND_OP_SETL = 3,
   PCO_BACKEND_OP_VISTEST = 4,
   PCO_BACKEND_OP_FITR = 5,
   PCO_BACKEND_OP_EMIT = 6,
   PCO_BACKEND_OP_DMA = 7,
};

static inline
__constant const char *pco_backend_op_str(uint64_t val) {
   switch (val) {
   case PCO_BACKEND_OP_UVSW:
      return "uvsw";
   case PCO_BACKEND_OP_MSK:
      return "msk";
   case PCO_BACKEND_OP_PHAS:
      return "phas";
   case PCO_BACKEND_OP_SETL:
      return "setl";
   case PCO_BACKEND_OP_VISTEST:
      return "vistest";
   case PCO_BACKEND_OP_FITR:
      return "fitr";
   case PCO_BACKEND_OP_EMIT:
      return "emit";
   case PCO_BACKEND_OP_DMA:
      return "dma";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_DSEL_COUNT 2U
enum pco_dsel {
   PCO_DSEL_W0 = 0,
   PCO_DSEL_W1 = 1,
};

static inline
__constant const char *pco_dsel_str(uint64_t val) {
   switch (val) {
   case PCO_DSEL_W0:
      return "w0";
   case PCO_DSEL_W1:
      return "w1";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_UVSW_OP_COUNT 7U
enum pco_uvsw_op {
   PCO_UVSW_OP_WRITE = 0,
   PCO_UVSW_OP_EMIT = 1,
   PCO_UVSW_OP_CUT = 2,
   PCO_UVSW_OP_EMIT_CUT = 3,
   PCO_UVSW_OP_ENDTASK = 4,
   PCO_UVSW_OP_EMIT_ENDTASK = 5,
   PCO_UVSW_OP_WRITE_EMIT_ENDTASK = 6,
};

static inline
__constant const char *pco_uvsw_op_str(uint64_t val) {
   switch (val) {
   case PCO_UVSW_OP_WRITE:
      return "write";
   case PCO_UVSW_OP_EMIT:
      return "emit";
   case PCO_UVSW_OP_CUT:
      return "cut";
   case PCO_UVSW_OP_EMIT_CUT:
      return "emit_cut";
   case PCO_UVSW_OP_ENDTASK:
      return "endtask";
   case PCO_UVSW_OP_EMIT_ENDTASK:
      return "emit_endtask";
   case PCO_UVSW_OP_WRITE_EMIT_ENDTASK:
      return "write_emit_endtask";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SRCSEL_COUNT 6U
enum pco_srcsel {
   PCO_SRCSEL_S0 = 0,
   PCO_SRCSEL_S1 = 1,
   PCO_SRCSEL_S2 = 2,
   PCO_SRCSEL_S3 = 3,
   PCO_SRCSEL_S4 = 4,
   PCO_SRCSEL_S5 = 5,
};

static inline
__constant const char *pco_srcsel_str(uint64_t val) {
   switch (val) {
   case PCO_SRCSEL_S0:
      return "s0";
   case PCO_SRCSEL_S1:
      return "s1";
   case PCO_SRCSEL_S2:
      return "s2";
   case PCO_SRCSEL_S3:
      return "s3";
   case PCO_SRCSEL_S4:
      return "s4";
   case PCO_SRCSEL_S5:
      return "s5";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MSK_MODE_COUNT 5U
enum pco_msk_mode {
   PCO_MSK_MODE_VM = 0,
   PCO_MSK_MODE_ICM = 1,
   PCO_MSK_MODE_ICMOC = 2,
   PCO_MSK_MODE_ICMI = 3,
   PCO_MSK_MODE_CAXY = 4,
};

static inline
__constant const char *pco_msk_mode_str(uint64_t val) {
   switch (val) {
   case PCO_MSK_MODE_VM:
      return "vm";
   case PCO_MSK_MODE_ICM:
      return "icm";
   case PCO_MSK_MODE_ICMOC:
      return "icmoc";
   case PCO_MSK_MODE_ICMI:
      return "icmi";
   case PCO_MSK_MODE_CAXY:
      return "caxy";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_MSK_OP_COUNT 2U
enum pco_msk_op {
   PCO_MSK_OP_SAV = 0,
   PCO_MSK_OP_MOV = 1,
};

static inline
__constant const char *pco_msk_op_str(uint64_t val) {
   switch (val) {
   case PCO_MSK_OP_SAV:
      return "sav";
   case PCO_MSK_OP_MOV:
      return "mov";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_PHAS_TYPE_COUNT 2U
enum pco_phas_type {
   PCO_PHAS_TYPE_REG = 0,
   PCO_PHAS_TYPE_IMM = 1,
};

static inline
__constant const char *pco_phas_type_str(uint64_t val) {
   switch (val) {
   case PCO_PHAS_TYPE_REG:
      return "reg";
   case PCO_PHAS_TYPE_IMM:
      return "imm";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_PHAS_RATE_COUNT 3U
enum pco_phas_rate {
   PCO_PHAS_RATE_INST = 0,
   PCO_PHAS_RATE_SMP_SEL = 1,
   PCO_PHAS_RATE_SMP_FULL = 2,
};

static inline
__constant const char *pco_phas_rate_str(uint64_t val) {
   switch (val) {
   case PCO_PHAS_RATE_INST:
      return "inst";
   case PCO_PHAS_RATE_SMP_SEL:
      return "smp_sel";
   case PCO_PHAS_RATE_SMP_FULL:
      return "smp_full";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_VISTEST_OP_COUNT 2U
enum pco_vistest_op {
   PCO_VISTEST_OP_DEPTHF = 0,
   PCO_VISTEST_OP_ATST = 1,
};

static inline
__constant const char *pco_vistest_op_str(uint64_t val) {
   switch (val) {
   case PCO_VISTEST_OP_DEPTHF:
      return "depthf";
   case PCO_VISTEST_OP_ATST:
      return "atst";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_ITER_MODE_COUNT 3U
enum pco_iter_mode {
   PCO_ITER_MODE_PIXEL = 0,
   PCO_ITER_MODE_SAMPLE = 1,
   PCO_ITER_MODE_CENTROID = 2,
};

static inline
__constant const char *pco_iter_mode_str(uint64_t val) {
   switch (val) {
   case PCO_ITER_MODE_PIXEL:
      return "pixel";
   case PCO_ITER_MODE_SAMPLE:
      return "sample";
   case PCO_ITER_MODE_CENTROID:
      return "centroid";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_PERSP_CTL_COUNT 4U
enum pco_persp_ctl {
   PCO_PERSP_CTL_NONE = 0,
   PCO_PERSP_CTL_ITER_MUL = 1,
   PCO_PERSP_CTL_ITER_MUL_STORE = 2,
   PCO_PERSP_CTL_MUL_STORED = 3,
};

static inline
__constant const char *pco_persp_ctl_str(uint64_t val) {
   switch (val) {
   case PCO_PERSP_CTL_NONE:
      return "none";
   case PCO_PERSP_CTL_ITER_MUL:
      return "iter_mul";
   case PCO_PERSP_CTL_ITER_MUL_STORE:
      return "iter_mul_store";
   case PCO_PERSP_CTL_MUL_STORED:
      return "mul_stored";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SCHED_CTRL_COUNT 3U
enum pco_sched_ctrl {
   PCO_SCHED_CTRL_NONE = 0,
   PCO_SCHED_CTRL_SWAP = 1,
   PCO_SCHED_CTRL_WDF = 2,
};

static inline
__constant const char *pco_sched_ctrl_str(uint64_t val) {
   switch (val) {
   case PCO_SCHED_CTRL_NONE:
      return "none";
   case PCO_SCHED_CTRL_SWAP:
      return "swap";
   case PCO_SCHED_CTRL_WDF:
      return "wdf";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_IDX_CTRL_COUNT 3U
enum pco_idx_ctrl {
   PCO_IDX_CTRL_NONE = 0,
   PCO_IDX_CTRL_IDX0 = 1,
   PCO_IDX_CTRL_IDX1 = 2,
};

static inline
__constant const char *pco_idx_ctrl_str(uint64_t val) {
   switch (val) {
   case PCO_IDX_CTRL_NONE:
      return "none";
   case PCO_IDX_CTRL_IDX0:
      return "idx0";
   case PCO_IDX_CTRL_IDX1:
      return "idx1";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_DMA_OP_COUNT 5U
enum pco_dma_op {
   PCO_DMA_OP_IDF = 0,
   PCO_DMA_OP_LD = 1,
   PCO_DMA_OP_ST = 2,
   PCO_DMA_OP_SMP = 4,
   PCO_DMA_OP_ATOMIC = 5,
};

static inline
__constant const char *pco_dma_op_str(uint64_t val) {
   switch (val) {
   case PCO_DMA_OP_IDF:
      return "idf";
   case PCO_DMA_OP_LD:
      return "ld";
   case PCO_DMA_OP_ST:
      return "st";
   case PCO_DMA_OP_SMP:
      return "smp";
   case PCO_DMA_OP_ATOMIC:
      return "atomic";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_CACHEMODE_LD_COUNT 3U
enum pco_cachemode_ld {
   PCO_CACHEMODE_LD_NORMAL = 0,
   PCO_CACHEMODE_LD_BYPASS = 1,
   PCO_CACHEMODE_LD_FORCE_LINE_FILL = 2,
};

static inline
__constant const char *pco_cachemode_ld_str(uint64_t val) {
   switch (val) {
   case PCO_CACHEMODE_LD_NORMAL:
      return "normal";
   case PCO_CACHEMODE_LD_BYPASS:
      return "bypass";
   case PCO_CACHEMODE_LD_FORCE_LINE_FILL:
      return "force_line_fill";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_CACHEMODE_ST_COUNT 3U
enum pco_cachemode_st {
   PCO_CACHEMODE_ST_WRITE_THROUGH = 0,
   PCO_CACHEMODE_ST_WRITE_BACK = 1,
   PCO_CACHEMODE_ST_WRITE_BACK_LAZY = 2,
};

static inline
__constant const char *pco_cachemode_st_str(uint64_t val) {
   switch (val) {
   case PCO_CACHEMODE_ST_WRITE_THROUGH:
      return "write_through";
   case PCO_CACHEMODE_ST_WRITE_BACK:
      return "write_back";
   case PCO_CACHEMODE_ST_WRITE_BACK_LAZY:
      return "write_back_lazy";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_CACHEMODE_LD_ST_COUNT 3U
enum pco_cachemode_ld_st {
   PCO_CACHEMODE_LD_ST_NORMAL = 0,
   PCO_CACHEMODE_LD_ST_BYPASS = 1,
   PCO_CACHEMODE_LD_ST_FORCE_LINE_FILL = 2,
   PCO_CACHEMODE_LD_ST_WRITE_THROUGH = 0,
   PCO_CACHEMODE_LD_ST_WRITE_BACK = 1,
   PCO_CACHEMODE_LD_ST_WRITE_BACK_LAZY = 2,
};

static inline
__constant const char *pco_cachemode_ld_st_str(uint64_t val) {
   switch (val) {
   case PCO_CACHEMODE_LD_ST_NORMAL:
      return "normal";
   case PCO_CACHEMODE_LD_ST_BYPASS:
      return "bypass";
   case PCO_CACHEMODE_LD_ST_FORCE_LINE_FILL:
      return "force_line_fill";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_DSIZE_COUNT 3U
enum pco_dsize {
   PCO_DSIZE_8BIT = 0,
   PCO_DSIZE_16BIT = 1,
   PCO_DSIZE_32BIT = 2,
};

static inline
__constant const char *pco_dsize_str(uint64_t val) {
   switch (val) {
   case PCO_DSIZE_8BIT:
      return "8bit";
   case PCO_DSIZE_16BIT:
      return "16bit";
   case PCO_DSIZE_32BIT:
      return "32bit";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_DMN_COUNT 3U
enum pco_dmn {
   PCO_DMN_1D = 1,
   PCO_DMN_2D = 2,
   PCO_DMN_3D = 3,
};

static inline
__constant const char *pco_dmn_str(uint64_t val) {
   switch (val) {
   case PCO_DMN_1D:
      return "1d";
   case PCO_DMN_2D:
      return "2d";
   case PCO_DMN_3D:
      return "3d";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_LODM_COUNT 4U
enum pco_lodm {
   PCO_LODM_NORMAL = 0,
   PCO_LODM_BIAS = 1,
   PCO_LODM_REPLACE = 2,
   PCO_LODM_GRADIENTS = 3,
};

static inline
__constant const char *pco_lodm_str(uint64_t val) {
   switch (val) {
   case PCO_LODM_NORMAL:
      return "normal";
   case PCO_LODM_BIAS:
      return "bias";
   case PCO_LODM_REPLACE:
      return "replace";
   case PCO_LODM_GRADIENTS:
      return "gradients";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SBMODE_COUNT 4U
enum pco_sbmode {
   PCO_SBMODE_NONE = 0,
   PCO_SBMODE_RAWDATA = 1,
   PCO_SBMODE_COEFFS = 2,
   PCO_SBMODE_BOTH = 3,
};

static inline
__constant const char *pco_sbmode_str(uint64_t val) {
   switch (val) {
   case PCO_SBMODE_NONE:
      return "none";
   case PCO_SBMODE_RAWDATA:
      return "rawdata";
   case PCO_SBMODE_COEFFS:
      return "coeffs";
   case PCO_SBMODE_BOTH:
      return "both";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_ATOMIC_OP_COUNT 10U
enum pco_atomic_op {
   PCO_ATOMIC_OP_ADD = 0,
   PCO_ATOMIC_OP_SUB = 1,
   PCO_ATOMIC_OP_XCHG = 2,
   PCO_ATOMIC_OP_UMIN = 4,
   PCO_ATOMIC_OP_IMIN = 5,
   PCO_ATOMIC_OP_UMAX = 6,
   PCO_ATOMIC_OP_IMAX = 7,
   PCO_ATOMIC_OP_AND = 8,
   PCO_ATOMIC_OP_OR = 9,
   PCO_ATOMIC_OP_XOR = 10,
};

static inline
__constant const char *pco_atomic_op_str(uint64_t val) {
   switch (val) {
   case PCO_ATOMIC_OP_ADD:
      return "add";
   case PCO_ATOMIC_OP_SUB:
      return "sub";
   case PCO_ATOMIC_OP_XCHG:
      return "xchg";
   case PCO_ATOMIC_OP_UMIN:
      return "umin";
   case PCO_ATOMIC_OP_IMIN:
      return "imin";
   case PCO_ATOMIC_OP_UMAX:
      return "umax";
   case PCO_ATOMIC_OP_IMAX:
      return "imax";
   case PCO_ATOMIC_OP_AND:
      return "and";
   case PCO_ATOMIC_OP_OR:
      return "or";
   case PCO_ATOMIC_OP_XOR:
      return "xor";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_COUNT_SRC_COUNT 2U
enum pco_count_src {
   PCO_COUNT_SRC_S2 = 0,
   PCO_COUNT_SRC_FT2 = 1,
};

static inline
__constant const char *pco_count_src_str(uint64_t val) {
   switch (val) {
   case PCO_COUNT_SRC_S2:
      return "s2";
   case PCO_COUNT_SRC_FT2:
      return "ft2";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_COUNT_OP_COUNT 3U
enum pco_count_op {
   PCO_COUNT_OP_CBS = 0,
   PCO_COUNT_OP_FTB = 1,
   PCO_COUNT_OP_BYP = 2,
};

static inline
__constant const char *pco_count_op_str(uint64_t val) {
   switch (val) {
   case PCO_COUNT_OP_CBS:
      return "cbs";
   case PCO_COUNT_OP_FTB:
      return "ftb";
   case PCO_COUNT_OP_BYP:
      return "byp";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_BITMASK_SRC_OP_COUNT 2U
enum pco_bitmask_src_op {
   PCO_BITMASK_SRC_OP_BYP = 0,
   PCO_BITMASK_SRC_OP_MSK = 1,
};

static inline
__constant const char *pco_bitmask_src_op_str(uint64_t val) {
   switch (val) {
   case PCO_BITMASK_SRC_OP_BYP:
      return "byp";
   case PCO_BITMASK_SRC_OP_MSK:
      return "msk";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_BITMASK_IMM_OP_COUNT 2U
enum pco_bitmask_imm_op {
   PCO_BITMASK_IMM_OP_BYP16 = 0,
   PCO_BITMASK_IMM_OP_BYP32 = 1,
};

static inline
__constant const char *pco_bitmask_imm_op_str(uint64_t val) {
   switch (val) {
   case PCO_BITMASK_IMM_OP_BYP16:
      return "byp16";
   case PCO_BITMASK_IMM_OP_BYP32:
      return "byp32";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SHIFT1_OP_COUNT 4U
enum pco_shift1_op {
   PCO_SHIFT1_OP_BYP = 0,
   PCO_SHIFT1_OP_SHFL = 1,
   PCO_SHIFT1_OP_REV = 2,
   PCO_SHIFT1_OP_LSL = 3,
};

static inline
__constant const char *pco_shift1_op_str(uint64_t val) {
   switch (val) {
   case PCO_SHIFT1_OP_BYP:
      return "byp";
   case PCO_SHIFT1_OP_SHFL:
      return "shfl";
   case PCO_SHIFT1_OP_REV:
      return "rev";
   case PCO_SHIFT1_OP_LSL:
      return "lsl";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_LOGICAL_OP_COUNT 7U
enum pco_logical_op {
   PCO_LOGICAL_OP_OR = 0,
   PCO_LOGICAL_OP_AND = 1,
   PCO_LOGICAL_OP_XOR = 2,
   PCO_LOGICAL_OP_NOR = 4,
   PCO_LOGICAL_OP_NAND = 5,
   PCO_LOGICAL_OP_XNOR = 6,
   PCO_LOGICAL_OP_BYP = 7,
};

static inline
__constant const char *pco_logical_op_str(uint64_t val) {
   switch (val) {
   case PCO_LOGICAL_OP_OR:
      return "or";
   case PCO_LOGICAL_OP_AND:
      return "and";
   case PCO_LOGICAL_OP_XOR:
      return "xor";
   case PCO_LOGICAL_OP_NOR:
      return "nor";
   case PCO_LOGICAL_OP_NAND:
      return "nand";
   case PCO_LOGICAL_OP_XNOR:
      return "xnor";
   case PCO_LOGICAL_OP_BYP:
      return "byp";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_BW_TST_SRC_COUNT 2U
enum pco_bw_tst_src {
   PCO_BW_TST_SRC_FT5 = 0,
   PCO_BW_TST_SRC_FT3 = 1,
};

static inline
__constant const char *pco_bw_tst_src_str(uint64_t val) {
   switch (val) {
   case PCO_BW_TST_SRC_FT5:
      return "ft5";
   case PCO_BW_TST_SRC_FT3:
      return "ft3";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_BW_TST_OP_COUNT 2U
enum pco_bw_tst_op {
   PCO_BW_TST_OP_Z = 0,
   PCO_BW_TST_OP_NZ = 1,
};

static inline
__constant const char *pco_bw_tst_op_str(uint64_t val) {
   switch (val) {
   case PCO_BW_TST_OP_Z:
      return "z";
   case PCO_BW_TST_OP_NZ:
      return "nz";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_SHIFT2_OP_COUNT 8U
enum pco_shift2_op {
   PCO_SHIFT2_OP_LSL = 0,
   PCO_SHIFT2_OP_SHR = 1,
   PCO_SHIFT2_OP_ROL = 2,
   PCO_SHIFT2_OP_CPS = 3,
   PCO_SHIFT2_OP_ASR_TWB = 4,
   PCO_SHIFT2_OP_ASR_PWB = 5,
   PCO_SHIFT2_OP_ASR_MTB = 6,
   PCO_SHIFT2_OP_ASR_FTB = 7,
};

static inline
__constant const char *pco_shift2_op_str(uint64_t val) {
   switch (val) {
   case PCO_SHIFT2_OP_LSL:
      return "lsl";
   case PCO_SHIFT2_OP_SHR:
      return "shr";
   case PCO_SHIFT2_OP_ROL:
      return "rol";
   case PCO_SHIFT2_OP_CPS:
      return "cps";
   case PCO_SHIFT2_OP_ASR_TWB:
      return "asr_twb";
   case PCO_SHIFT2_OP_ASR_PWB:
      return "asr_pwb";
   case PCO_SHIFT2_OP_ASR_MTB:
      return "asr_mtb";
   case PCO_SHIFT2_OP_ASR_FTB:
      return "asr_ftb";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_PCND_COUNT 4U
enum pco_pcnd {
   PCO_PCND_ALWAYS = 0,
   PCO_PCND_P0_TRUE = 1,
   PCO_PCND_NEVER = 2,
   PCO_PCND_P0_FALSE = 3,
};

static inline
__constant const char *pco_pcnd_str(uint64_t val) {
   switch (val) {
   case PCO_PCND_ALWAYS:
      return "always";
   case PCO_PCND_P0_TRUE:
      return "p0_true";
   case PCO_PCND_NEVER:
      return "never";
   case PCO_PCND_P0_FALSE:
      return "p0_false";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_CNDINST_COUNT 8U
enum pco_cndinst {
   PCO_CNDINST_ST = 0,
   PCO_CNDINST_EF = 1,
   PCO_CNDINST_SM = 2,
   PCO_CNDINST_LT = 3,
   PCO_CNDINST_END = 4,
   PCO_CNDINST_SETL_B = 5,
   PCO_CNDINST_LPC = 6,
   PCO_CNDINST_SETL_A = 7,
};

static inline
__constant const char *pco_cndinst_str(uint64_t val) {
   switch (val) {
   case PCO_CNDINST_ST:
      return "st";
   case PCO_CNDINST_EF:
      return "ef";
   case PCO_CNDINST_SM:
      return "sm";
   case PCO_CNDINST_LT:
      return "lt";
   case PCO_CNDINST_END:
      return "end";
   case PCO_CNDINST_SETL_B:
      return "setl_b";
   case PCO_CNDINST_LPC:
      return "lpc";
   case PCO_CNDINST_SETL_A:
      return "setl_a";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_LR_COUNT 4U
enum pco_lr {
   PCO_LR_RELEASE = 0,
   PCO_LR_RELEASE_SLEEP = 1,
   PCO_LR_RELEASE_WAKEUP = 2,
   PCO_LR_LOCK = 3,
};

static inline
__constant const char *pco_lr_str(uint64_t val) {
   switch (val) {
   case PCO_LR_RELEASE:
      return "release";
   case PCO_LR_RELEASE_SLEEP:
      return "release_sleep";
   case PCO_LR_RELEASE_WAKEUP:
      return "release_wakeup";
   case PCO_LR_LOCK:
      return "lock";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_TGT_COUNT 2U
enum pco_tgt {
   PCO_TGT_COEFF = 0,
   PCO_TGT_SHARED = 1,
};

static inline
__constant const char *pco_tgt_str(uint64_t val) {
   switch (val) {
   case PCO_TGT_COEFF:
      return "coeff";
   case PCO_TGT_SHARED:
      return "shared";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_BPRED_COUNT 3U
enum pco_bpred {
   PCO_BPRED_CC = 0,
   PCO_BPRED_ALLP = 1,
   PCO_BPRED_ANYP = 2,
};

static inline
__constant const char *pco_bpred_str(uint64_t val) {
   switch (val) {
   case PCO_BPRED_CC:
      return "cc";
   case PCO_BPRED_ALLP:
      return "allp";
   case PCO_BPRED_ANYP:
      return "anyp";

   default:
      break;
   }

   UNREACHABLE("");
}

#define _PCO_REF_MAP_COUNT 30U
enum pco_ref_map {
   PCO_REF_MAP__ = 0,
   PCO_REF_MAP_S0 = 1,
   PCO_REF_MAP_S1 = 2,
   PCO_REF_MAP_S2 = 3,
   PCO_REF_MAP_S3 = 4,
   PCO_REF_MAP_S4 = 5,
   PCO_REF_MAP_S5 = 6,
   PCO_REF_MAP_W0 = 7,
   PCO_REF_MAP_W1 = 8,
   PCO_REF_MAP_IS0 = 9,
   PCO_REF_MAP_IS1 = 10,
   PCO_REF_MAP_IS2 = 11,
   PCO_REF_MAP_IS3 = 12,
   PCO_REF_MAP_IS4 = 13,
   PCO_REF_MAP_IS5 = 14,
   PCO_REF_MAP_FT0 = 15,
   PCO_REF_MAP_FT1 = 16,
   PCO_REF_MAP_FT1_INVERT = 17,
   PCO_REF_MAP_FT2 = 18,
   PCO_REF_MAP_FTE = 19,
   PCO_REF_MAP_FT3 = 20,
   PCO_REF_MAP_FT4 = 21,
   PCO_REF_MAP_FT5 = 22,
   PCO_REF_MAP_FTT = 23,
   PCO_REF_MAP_P0 = 24,
   PCO_REF_MAP_PE = 25,
   PCO_REF_MAP_IMM = 26,
   PCO_REF_MAP_DRC = 27,
   PCO_REF_MAP_TEMP = 28,
   PCO_REF_MAP_COEFF = 29,
};

static inline
__constant const char *pco_ref_map_str(uint64_t val) {
   switch (val) {
   case PCO_REF_MAP__:
      return "_";
   case PCO_REF_MAP_S0:
      return "s0";
   case PCO_REF_MAP_S1:
      return "s1";
   case PCO_REF_MAP_S2:
      return "s2";
   case PCO_REF_MAP_S3:
      return "s3";
   case PCO_REF_MAP_S4:
      return "s4";
   case PCO_REF_MAP_S5:
      return "s5";
   case PCO_REF_MAP_W0:
      return "w0";
   case PCO_REF_MAP_W1:
      return "w1";
   case PCO_REF_MAP_IS0:
      return "is0";
   case PCO_REF_MAP_IS1:
      return "is1";
   case PCO_REF_MAP_IS2:
      return "is2";
   case PCO_REF_MAP_IS3:
      return "is3";
   case PCO_REF_MAP_IS4:
      return "is4";
   case PCO_REF_MAP_IS5:
      return "is5";
   case PCO_REF_MAP_FT0:
      return "ft0";
   case PCO_REF_MAP_FT1:
      return "ft1";
   case PCO_REF_MAP_FT1_INVERT:
      return "~ft1";
   case PCO_REF_MAP_FT2:
      return "ft2";
   case PCO_REF_MAP_FTE:
      return "fte";
   case PCO_REF_MAP_FT3:
      return "ft3";
   case PCO_REF_MAP_FT4:
      return "ft4";
   case PCO_REF_MAP_FT5:
      return "ft5";
   case PCO_REF_MAP_FTT:
      return "ftt";
   case PCO_REF_MAP_P0:
      return "p0";
   case PCO_REF_MAP_PE:
      return "pe";
   case PCO_REF_MAP_IMM:
      return "imm";
   case PCO_REF_MAP_DRC:
      return "drc";
   case PCO_REF_MAP_TEMP:
      return "temp";
   case PCO_REF_MAP_COEFF:
      return "coeff";

   default:
      break;
   }

   UNREACHABLE("");
}

/** Enum validation. */
static bool pco_op_type_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_mod_type_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_ref_type_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_func_type_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_dtype_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_bits_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4;
}

static bool pco_reg_class_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10 || val == 11;
}

static bool pco_io_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10 || val == 11 || val == 12 || val == 13 || val == 14 || val == 15 || val == 16 || val == 17 || val == 18 || val == 19 || val == 20 || val == 21 || val == 22 || val == 23;
}

static bool pco_pred_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5;
}

static bool pco_drc_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_mutex_id_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10 || val == 11 || val == 12 || val == 13 || val == 14 || val == 15;
}

static bool pco_elem_valid(uint64_t val)
{
   return !(val & ~(15ULL));
}

static bool pco_exec_cnd_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_tst_type_main_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6;
}

static bool pco_tst_op_main_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9;
}

static bool pco_tst_op_bitwise_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_signpos_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_dim_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_lod_mode_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_sb_mode_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_schedswap_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_savmsk_mode_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4;
}

static bool pco_atom_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10;
}

static bool pco_mcu_cache_mode_ld_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_mcu_cache_mode_st_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_branch_cnd_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_pck_fmt_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10 || val == 11 || val == 12 || val == 13 || val == 14 || val == 15 || val == 16 || val == 17 || val == 18 || val == 19 || val == 20 || val == 21 || val == 22 || val == 23 || val == 24;
}

static bool pco_itr_mode_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_sched_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_logiop_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5;
}

static bool pco_shiftop_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_cnd_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_fred_type_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_fred_part_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_mutex_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_offset_sd_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_vote_op_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_op_phase_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5;
}

static bool pco_sr_valid(uint64_t val)
{
   return val == 32 || val == 33 || val == 34 || val == 35 || val == 36 || val == 37 || val == 38 || val == 39 || val == 40 || val == 41 || val == 42 || val == 43 || val == 44 || val == 45 || val == 46 || val == 47 || val == 48 || val == 49 || val == 50 || val == 51 || val == 52 || val == 53 || val == 54 || val == 55 || val == 56 || val == 57 || val == 58 || val == 59 || val == 60 || val == 61 || val == 62 || val == 63 || val == 160 || val == 164 || val == 165 || val == 166 || val == 167 || val == 96 || val == 97 || val == 98 || val == 100 || val == 101 || val == 102 || val == 103 || val == 104 || val == 105 || val == 106 || val == 107 || val == 108 || val == 109 || val == 110 || val == 111 || val == 112 || val == 114 || val == 115 || val == 116 || val == 117 || val == 118 || val == 119 || val == 120 || val == 121 || val == 122 || val == 123 || val == 124 || val == 125 || val == 126 || val == 127;
}

static bool pco_oporg_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_opcnt_valid(uint64_t val)
{
   return !(val & ~(7ULL));
}

static bool pco_cc_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_cc1_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_alutype_valid(uint64_t val)
{
   return val == 0 || val == 2 || val == 3;
}

static bool pco_ctrlop_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 11 || val == 12;
}

static bool pco_is0_sel_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5;
}

static bool pco_is0_sel2_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_regbank_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_idxbank_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 5 || val == 6 || val == 7;
}

static bool pco_regbank2_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_regbank1_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_is5_sel_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_is4_sel_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_is3_sel_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 3;
}

static bool pco_is2_sel_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_is1_sel_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_main_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_sngl_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10 || val == 11 || val == 12 || val == 13 || val == 14 || val == 15;
}

static bool pco_red_part_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_red_type_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_gamma_op_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_upck_elem_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_pck_format_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10 || val == 11 || val == 12 || val == 14 || val == 15 || val == 16 || val == 17 || val == 18 || val == 19 || val == 20 || val == 21 || val == 22 || val == 23 || val == 30 || val == 31;
}

static bool pco_int8_16_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_int8_16_fmt_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_s2ch_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_s01ch_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_movw01_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_maskw0_valid(uint64_t val)
{
   return !(val & ~(15ULL));
}

static bool pco_int32_64_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_tst_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9;
}

static bool pco_tst_op3_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_tst_type_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6;
}

static bool pco_backend_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_dsel_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_uvsw_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6;
}

static bool pco_srcsel_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5;
}

static bool pco_msk_mode_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4;
}

static bool pco_msk_op_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_phas_type_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_phas_rate_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_vistest_op_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_iter_mode_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_iter_mode1_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_persp_ctl_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_persp_ctl1_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_sched_ctrl_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_sched_ctrl1_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_idx_ctrl_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_dma_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 4 || val == 5;
}

static bool pco_cachemode_ld_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_cachemode_st_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_cachemode_ld_st_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_dsize_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_dmn_valid(uint64_t val)
{
   return val == 1 || val == 2 || val == 3;
}

static bool pco_lodm_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_sbmode_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_atomic_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10;
}

static bool pco_count_src_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_count_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_bitmask_src_op_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_bitmask_imm_op_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_shift1_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_logical_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_bw_tst_src_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_bw_tst_op_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_shift2_op_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_pcnd_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_cndinst_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7;
}

static bool pco_lr_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3;
}

static bool pco_tgt_valid(uint64_t val)
{
   return val == 0 || val == 1;
}

static bool pco_bpred_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2;
}

static bool pco_ref_map_valid(uint64_t val)
{
   return val == 0 || val == 1 || val == 2 || val == 3 || val == 4 || val == 5 || val == 6 || val == 7 || val == 8 || val == 9 || val == 10 || val == 11 || val == 12 || val == 13 || val == 14 || val == 15 || val == 16 || val == 17 || val == 18 || val == 19 || val == 20 || val == 21 || val == 22 || val == 23 || val == 24 || val == 25 || val == 26 || val == 27 || val == 28 || val == 29;
}

/** Bit set variants. */
#define _PCO_IGRP_HDR_VARIANT_COUNT 5U
enum pco_igrp_hdr_variant {
   PCO_IGRP_HDR_NONE,
   PCO_IGRP_HDR_MAIN_BRIEF,
   PCO_IGRP_HDR_MAIN,
   PCO_IGRP_HDR_BITWISE,
   PCO_IGRP_HDR_CONTROL,
};

#define _PCO_SRC_VARIANT_COUNT 17U
enum pco_src_variant {
   PCO_SRC_NONE,
   PCO_SRC_1LO_1B6I,
   PCO_SRC_1LO_3B11I_2M,
   PCO_SRC_2LO_1B6I_1B5I,
   PCO_SRC_2LO_2B7I_2B7I_2M,
   PCO_SRC_2LO_3B11I_2B8I_3M,
   PCO_SRC_3LO_2B7I_2B7I_2B6I_2M,
   PCO_SRC_3LO_3B8I_2B8I_3B8I_3M,
   PCO_SRC_3LO_3B11I_2B8I_3B11I_3M,
   PCO_SRC_1UP_1B6I,
   PCO_SRC_1UP_3B11I,
   PCO_SRC_2UP_1B6I_1B5I,
   PCO_SRC_2UP_2B7I_2B7I,
   PCO_SRC_2UP_3B11I_2B8I,
   PCO_SRC_3UP_2B7I_2B7I_2B6I,
   PCO_SRC_3UP_3B8I_2B8I_2B8I,
   PCO_SRC_3UP_3B11I_2B8I_2B8I,
};

#define _PCO_ISS_VARIANT_COUNT 2U
enum pco_iss_variant {
   PCO_ISS_NONE,
   PCO_ISS_ISS,
};

#define _PCO_DST_VARIANT_COUNT 6U
enum pco_dst_variant {
   PCO_DST_NONE,
   PCO_DST_1_1B6I,
   PCO_DST_1_3B11I,
   PCO_DST_2_1B7I_1B6I,
   PCO_DST_2_3B8I_3B8I,
   PCO_DST_2_3B11I_3B11I,
};

#define _PCO_MAIN_VARIANT_COUNT 24U
enum pco_main_variant {
   PCO_MAIN_NONE,
   PCO_MAIN_FADD,
   PCO_MAIN_FADD_LP,
   PCO_MAIN_FMUL,
   PCO_MAIN_FMUL_LP,
   PCO_MAIN_SNGL,
   PCO_MAIN_SNGL_EXT,
   PCO_MAIN_GCMP,
   PCO_MAIN_GCMP_EXT,
   PCO_MAIN_GEXP,
   PCO_MAIN_PCK,
   PCO_MAIN_UPCK,
   PCO_MAIN_FRED,
   PCO_MAIN_INT8_16,
   PCO_MAIN_INT8_16_EXT,
   PCO_MAIN_INT8_16_EXT_SEL,
   PCO_MAIN_FMAD,
   PCO_MAIN_FMAD_EXT,
   PCO_MAIN_INT32_64,
   PCO_MAIN_INT32_64_EXT,
   PCO_MAIN_MOVC,
   PCO_MAIN_MOVC_EXT,
   PCO_MAIN_TST,
   PCO_MAIN_TST_EXT,
};

#define _PCO_BACKEND_VARIANT_COUNT 36U
enum pco_backend_variant {
   PCO_BACKEND_NONE,
   PCO_BACKEND_UVSW_WRITE_REG,
   PCO_BACKEND_UVSW_WRITE_IMM,
   PCO_BACKEND_UVSW_EMIT,
   PCO_BACKEND_UVSW_EMIT_IMM,
   PCO_BACKEND_UVSW_CUT,
   PCO_BACKEND_UVSW_CUT_IMM,
   PCO_BACKEND_UVSW_EMIT_CUT,
   PCO_BACKEND_UVSW_EMIT_CUT_IMM,
   PCO_BACKEND_UVSW_ENDTASK,
   PCO_BACKEND_UVSW_EMIT_ENDTASK,
   PCO_BACKEND_UVSW_WRITE_EMIT_ENDTASK_REG,
   PCO_BACKEND_UVSW_WRITE_EMIT_ENDTASK_IMM,
   PCO_BACKEND_MOVMSK,
   PCO_BACKEND_MOVMSK_SM,
   PCO_BACKEND_SAVMSK,
   PCO_BACKEND_PHAS_REG,
   PCO_BACKEND_PHAS_IMM,
   PCO_BACKEND_SETL,
   PCO_BACKEND_VISTEST_DEPTHF,
   PCO_BACKEND_VISTEST_ATST,
   PCO_BACKEND_FITR,
   PCO_BACKEND_EMITPIX,
   PCO_BACKEND_IDF,
   PCO_BACKEND_LD_IMMBL,
   PCO_BACKEND_LD_REGBL,
   PCO_BACKEND_ST_IMMBL,
   PCO_BACKEND_ST_REGBL,
   PCO_BACKEND_ST_IMMBL_TILED,
   PCO_BACKEND_ST_REGBL_TILED,
   PCO_BACKEND_SMP_BRIEF,
   PCO_BACKEND_SMP_EXTA,
   PCO_BACKEND_SMP_EXTB,
   PCO_BACKEND_SMP_EXTAB,
   PCO_BACKEND_SMP_EXTABC,
   PCO_BACKEND_ATOMIC,
};

#define _PCO_BITWISE_VARIANT_COUNT 6U
enum pco_bitwise_variant {
   PCO_BITWISE_NONE,
   PCO_BITWISE_PHASE0_SRC,
   PCO_BITWISE_PHASE0_IMM16,
   PCO_BITWISE_PHASE0_IMM32,
   PCO_BITWISE_PHASE1,
   PCO_BITWISE_PHASE2,
};

#define _PCO_CTRL_VARIANT_COUNT 14U
enum pco_ctrl_variant {
   PCO_CTRL_NONE,
   PCO_CTRL_BRANCH,
   PCO_CTRL_LAPC,
   PCO_CTRL_SAVL,
   PCO_CTRL_CND,
   PCO_CTRL_WOP,
   PCO_CTRL_WDF,
   PCO_CTRL_MUTEX,
   PCO_CTRL_NOP,
   PCO_CTRL_SBO,
   PCO_CTRL_DITR,
   PCO_CTRL_ITRSMP,
   PCO_CTRL_ITRSMP_EXT,
   PCO_CTRL_ITRSMP_EXT2,
};

#endif /* PCO_COMMON_H */
