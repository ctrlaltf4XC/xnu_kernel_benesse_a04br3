/*
 * Copyright © 2024 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PCO_ISA_H
#define PCO_ISA_H

/**
 * \file pco_isa.h
 *
 * \brief PCO ISA definitions.
 */

#include "pco_common.h"
#include "util/macros.h"

#include <stdbool.h>

/** igrp_hdr */
enum pco_igrp_hdr_field {
   PCO_IGRP_HDR_DA,
   PCO_IGRP_HDR_LENGTH,
   PCO_IGRP_HDR_EXT,
   PCO_IGRP_HDR_OPORG,
   PCO_IGRP_HDR_OPCNT,
   PCO_IGRP_HDR_OLCHK,
   PCO_IGRP_HDR_W1P,
   PCO_IGRP_HDR_W0P,
   PCO_IGRP_HDR_CC1,
   PCO_IGRP_HDR_END,
   PCO_IGRP_HDR_ALUTYPE,
   PCO_IGRP_HDR_RSVD,
   PCO_IGRP_HDR_ATOM,
   PCO_IGRP_HDR_RPT,
   PCO_IGRP_HDR_CC,
   PCO_IGRP_HDR_MISCCTL,
   PCO_IGRP_HDR_CTRLOP,
};

static unsigned pco_igrp_hdr_bytes(enum pco_igrp_hdr_variant variant)
{
   switch (variant) {
   case PCO_IGRP_HDR_NONE: return 0;
   case PCO_IGRP_HDR_MAIN_BRIEF: return 2;
   case PCO_IGRP_HDR_MAIN: return 3;
   case PCO_IGRP_HDR_BITWISE: return 3;
   case PCO_IGRP_HDR_CONTROL: return 3;
   default: break;
   }

   UNREACHABLE("");
}

static unsigned pco_igrp_hdr_encode_field(uint8_t *bin, enum pco_igrp_hdr_field field, uint64_t val)
{
   uint64_t enc_val;

   switch (field) {
   case PCO_IGRP_HDR_DA:
      enc_val = val;
      assert(enc_val < (1ULL << 4));
      bin[0] &= 0xf;
      bin[0] |= (enc_val & 0xf) << 4;
      return 4;

   case PCO_IGRP_HDR_LENGTH:
      assert(val >= 1 && val <= 16);
      enc_val = val % 16;
      assert(enc_val < (1ULL << 4));
      bin[0] &= 0xf0;
      bin[0] |= (enc_val & 0xf);
      return 4;

   case PCO_IGRP_HDR_EXT:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_IGRP_HDR_OPORG:
      enc_val = val;
      assert(pco_oporg_valid(enc_val));
      bin[1] &= 0x8f;
      bin[1] |= (enc_val & 0x7) << 4;
      return 3;

   case PCO_IGRP_HDR_OPCNT:
      enc_val = val;
      assert(pco_opcnt_valid(enc_val));
      bin[1] &= 0x8f;
      bin[1] |= (enc_val & 0x7) << 4;
      return 3;

   case PCO_IGRP_HDR_OLCHK:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xf7;
      bin[1] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_IGRP_HDR_W1P:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfb;
      bin[1] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_IGRP_HDR_W0P:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfd;
      bin[1] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_IGRP_HDR_CC1:
      enc_val = val;
      assert(pco_cc1_valid(enc_val));
      bin[1] &= 0xfe;
      bin[1] |= (enc_val & 0x1);
      return 1;

   case PCO_IGRP_HDR_END:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0x7f;
      bin[2] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_IGRP_HDR_ALUTYPE:
      enc_val = val;
      assert(pco_alutype_valid(enc_val));
      bin[2] &= 0x9f;
      bin[2] |= (enc_val & 0x3) << 5;
      return 2;

   case PCO_IGRP_HDR_RSVD:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xef;
      bin[2] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_IGRP_HDR_ATOM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xf7;
      bin[2] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_IGRP_HDR_RPT:
      assert(val >= 1 && val <= 4);
      enc_val = val - 1;
      assert(enc_val < (1ULL << 2));
      bin[2] &= 0xf9;
      bin[2] |= (enc_val & 0x3) << 1;
      return 2;

   case PCO_IGRP_HDR_CC:
      enc_val = val;
      assert(pco_cc_valid(enc_val));
      bin[1] &= 0xfe;
      bin[1] |= (enc_val & 0x1);
      bin[2] &= 0xfe;
      bin[2] |= ((enc_val >> 1) & 0x1);
      return 2;

   case PCO_IGRP_HDR_MISCCTL:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0x7f;
      bin[2] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_IGRP_HDR_CTRLOP:
      enc_val = val;
      assert(pco_ctrlop_valid(enc_val));
      bin[2] &= 0xe1;
      bin[2] |= (enc_val & 0xf) << 1;
      return 4;

   default:
      break;
   }

   UNREACHABLE("");
}

struct pco_igrp_hdr_main_brief {
   unsigned da : 4;
   unsigned length : 5;
   enum pco_oporg oporg : 3;
   bool olchk : 1;
   bool w1p : 1;
   bool w0p : 1;
   enum pco_cc cc : 2;
};

#define pco_igrp_hdr_main_brief_encode(bin, ...) _pco_igrp_hdr_main_brief_encode(bin, (const struct pco_igrp_hdr_main_brief){0, ##__VA_ARGS__})
static unsigned _pco_igrp_hdr_main_brief_encode(uint8_t *bin, const struct pco_igrp_hdr_main_brief s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_DA, s.da);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_LENGTH, s.length);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_EXT, 0);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_OPORG, s.oporg);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_OLCHK, s.olchk);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_W1P, s.w1p);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_W0P, s.w0p);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_CC1, s.cc);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_igrp_hdr_main {
   unsigned da : 4;
   unsigned length : 5;
   enum pco_oporg oporg : 3;
   bool olchk : 1;
   bool w1p : 1;
   bool w0p : 1;
   enum pco_cc cc : 2;
   bool end : 1;
   bool atom : 1;
   unsigned rpt : 3;
};

#define pco_igrp_hdr_main_encode(bin, ...) _pco_igrp_hdr_main_encode(bin, (const struct pco_igrp_hdr_main){0, ##__VA_ARGS__})
static unsigned _pco_igrp_hdr_main_encode(uint8_t *bin, const struct pco_igrp_hdr_main s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_DA, s.da);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_LENGTH, s.length);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_EXT, 1);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_OPORG, s.oporg);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_OLCHK, s.olchk);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_W1P, s.w1p);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_W0P, s.w0p);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_CC, s.cc);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_END, s.end);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_ALUTYPE, PCO_ALUTYPE_MAIN);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_RSVD, 0);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_ATOM, s.atom);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_RPT, s.rpt);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_igrp_hdr_bitwise {
   unsigned da : 4;
   unsigned length : 5;
   enum pco_opcnt opcnt : 3;
   bool olchk : 1;
   bool w1p : 1;
   bool w0p : 1;
   enum pco_cc cc : 2;
   bool end : 1;
   bool atom : 1;
   unsigned rpt : 3;
};

#define pco_igrp_hdr_bitwise_encode(bin, ...) _pco_igrp_hdr_bitwise_encode(bin, (const struct pco_igrp_hdr_bitwise){0, ##__VA_ARGS__})
static unsigned _pco_igrp_hdr_bitwise_encode(uint8_t *bin, const struct pco_igrp_hdr_bitwise s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_DA, s.da);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_LENGTH, s.length);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_EXT, 1);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_OPCNT, s.opcnt);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_OLCHK, s.olchk);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_W1P, s.w1p);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_W0P, s.w0p);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_CC, s.cc);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_END, s.end);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_ALUTYPE, PCO_ALUTYPE_BITWISE);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_RSVD, 0);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_ATOM, s.atom);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_RPT, s.rpt);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_igrp_hdr_control {
   unsigned da : 4;
   unsigned length : 5;
   bool olchk : 1;
   bool w1p : 1;
   bool w0p : 1;
   enum pco_cc cc : 2;
   unsigned miscctl : 1;
   enum pco_ctrlop ctrlop : 4;
};

#define pco_igrp_hdr_control_encode(bin, ...) _pco_igrp_hdr_control_encode(bin, (const struct pco_igrp_hdr_control){0, ##__VA_ARGS__})
static unsigned _pco_igrp_hdr_control_encode(uint8_t *bin, const struct pco_igrp_hdr_control s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_DA, s.da);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_LENGTH, s.length);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_EXT, 1);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_OPCNT, 0);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_OLCHK, s.olchk);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_W1P, s.w1p);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_W0P, s.w0p);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_CC, s.cc);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_MISCCTL, s.miscctl);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_ALUTYPE, PCO_ALUTYPE_CONTROL);
   bits_encoded += pco_igrp_hdr_encode_field(bin, PCO_IGRP_HDR_CTRLOP, s.ctrlop);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

/** src */
enum pco_src_field {
   PCO_SRC_EXT0,
   PCO_SRC_SBA_1BIT_B0,
   PCO_SRC_SA_6BIT_B0,
   PCO_SRC_SEL,
   PCO_SRC_EXT1,
   PCO_SRC_MUX_2BIT_B1,
   PCO_SRC_SBA_3BIT_B1,
   PCO_SRC_SA_11BIT_B2,
   PCO_SRC_RSVD2,
   PCO_SRC_SBB_1BIT_B1,
   PCO_SRC_SB_5BIT_B1,
   PCO_SRC_EXT2,
   PCO_SRC_MUX_2BIT_B2,
   PCO_SRC_SBA_2BIT_B2,
   PCO_SRC_SBB_2BIT_B2,
   PCO_SRC_SA_7BIT_B2,
   PCO_SRC_SB_7BIT_B2,
   PCO_SRC_SA_11BIT_B3,
   PCO_SRC_MUX_3BIT_B3,
   PCO_SRC_SBA_3BIT_B3,
   PCO_SRC_RSVD3,
   PCO_SRC_SB_8BIT_B3,
   PCO_SRC_SBC_2BIT_B3,
   PCO_SRC_SC_6BIT_B3,
   PCO_SRC_RSVD4,
   PCO_SRC_SBC_3BIT_B4,
   PCO_SRC_SC_8BIT_B4,
   PCO_SRC_MUX_3BIT_B4,
   PCO_SRC_SBA_3BIT_B4,
   PCO_SRC_EXT4,
   PCO_SRC_SA_8BIT_B4,
   PCO_SRC_SB_8BIT_B4,
   PCO_SRC_RSVD5,
   PCO_SRC_RSVD5_,
   PCO_SRC_SC_11BIT_B5,
   PCO_SRC_SA_11BIT_B5,
};

static unsigned pco_src_bytes(enum pco_src_variant variant)
{
   switch (variant) {
   case PCO_SRC_NONE: return 0;
   case PCO_SRC_1LO_1B6I: return 1;
   case PCO_SRC_1LO_3B11I_2M: return 3;
   case PCO_SRC_2LO_1B6I_1B5I: return 2;
   case PCO_SRC_2LO_2B7I_2B7I_2M: return 3;
   case PCO_SRC_2LO_3B11I_2B8I_3M: return 4;
   case PCO_SRC_3LO_2B7I_2B7I_2B6I_2M: return 4;
   case PCO_SRC_3LO_3B8I_2B8I_3B8I_3M: return 5;
   case PCO_SRC_3LO_3B11I_2B8I_3B11I_3M: return 6;
   case PCO_SRC_1UP_1B6I: return 1;
   case PCO_SRC_1UP_3B11I: return 3;
   case PCO_SRC_2UP_1B6I_1B5I: return 2;
   case PCO_SRC_2UP_2B7I_2B7I: return 3;
   case PCO_SRC_2UP_3B11I_2B8I: return 4;
   case PCO_SRC_3UP_2B7I_2B7I_2B6I: return 4;
   case PCO_SRC_3UP_3B8I_2B8I_2B8I: return 5;
   case PCO_SRC_3UP_3B11I_2B8I_2B8I: return 6;
   default: break;
   }

   UNREACHABLE("");
}

static unsigned pco_src_encode_field(uint8_t *bin, enum pco_src_field field, uint64_t val)
{
   uint64_t enc_val;

   switch (field) {
   case PCO_SRC_EXT0:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0x7f;
      bin[0] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_SRC_SBA_1BIT_B0:
      enc_val = val;
      assert(pco_regbank1_valid(enc_val));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_SRC_SA_6BIT_B0:
      enc_val = val;
      assert(enc_val < (1ULL << 6));
      bin[0] &= 0xc0;
      bin[0] |= (enc_val & 0x3f);
      return 6;

   case PCO_SRC_SEL:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_SRC_EXT1:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xbf;
      bin[1] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_SRC_MUX_2BIT_B1:
      enc_val = val;
      assert(pco_is0_sel2_valid(enc_val));
      bin[1] &= 0xcf;
      bin[1] |= (enc_val & 0x3) << 4;
      return 2;

   case PCO_SRC_SBA_3BIT_B1:
      enc_val = val;
      assert(pco_regbank_valid(enc_val));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      bin[1] &= 0xf3;
      bin[1] |= ((enc_val >> 1) & 0x3) << 2;
      return 3;

   case PCO_SRC_SA_11BIT_B2:
      enc_val = val;
      assert(enc_val < (1ULL << 11));
      bin[0] &= 0xc0;
      bin[0] |= (enc_val & 0x3f);
      bin[1] &= 0xfc;
      bin[1] |= ((enc_val >> 6) & 0x3);
      bin[2] &= 0xf8;
      bin[2] |= ((enc_val >> 8) & 0x7);
      return 11;

   case PCO_SRC_RSVD2:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 5));
      bin[2] &= 0x7;
      bin[2] |= (enc_val & 0x1f) << 3;
      return 5;

   case PCO_SRC_SBB_1BIT_B1:
      enc_val = val;
      assert(pco_regbank1_valid(enc_val));
      bin[1] &= 0xdf;
      bin[1] |= (enc_val & 0x1) << 5;
      return 1;

   case PCO_SRC_SB_5BIT_B1:
      enc_val = val;
      assert(enc_val < (1ULL << 5));
      bin[1] &= 0xe0;
      bin[1] |= (enc_val & 0x1f);
      return 5;

   case PCO_SRC_EXT2:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0x7f;
      bin[2] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_SRC_MUX_2BIT_B2:
      enc_val = val;
      assert(pco_is0_sel2_valid(enc_val));
      bin[2] &= 0x9f;
      bin[2] |= (enc_val & 0x3) << 5;
      return 2;

   case PCO_SRC_SBA_2BIT_B2:
      enc_val = val;
      assert(pco_regbank2_valid(enc_val));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      bin[2] &= 0xef;
      bin[2] |= ((enc_val >> 1) & 0x1) << 4;
      return 2;

   case PCO_SRC_SBB_2BIT_B2:
      enc_val = val;
      assert(pco_regbank2_valid(enc_val));
      bin[1] &= 0xdf;
      bin[1] |= (enc_val & 0x1) << 5;
      bin[2] &= 0xf7;
      bin[2] |= ((enc_val >> 1) & 0x1) << 3;
      return 2;

   case PCO_SRC_SA_7BIT_B2:
      enc_val = val;
      assert(enc_val < (1ULL << 7));
      bin[0] &= 0xc0;
      bin[0] |= (enc_val & 0x3f);
      bin[2] &= 0xfb;
      bin[2] |= ((enc_val >> 6) & 0x1) << 2;
      return 7;

   case PCO_SRC_SB_7BIT_B2:
      enc_val = val;
      assert(enc_val < (1ULL << 7));
      bin[1] &= 0xe0;
      bin[1] |= (enc_val & 0x1f);
      bin[2] &= 0xfc;
      bin[2] |= ((enc_val >> 5) & 0x3);
      return 7;

   case PCO_SRC_SA_11BIT_B3:
      enc_val = val;
      assert(enc_val < (1ULL << 11));
      bin[0] &= 0xc0;
      bin[0] |= (enc_val & 0x3f);
      bin[2] &= 0xfb;
      bin[2] |= ((enc_val >> 6) & 0x1) << 2;
      bin[3] &= 0xfd;
      bin[3] |= ((enc_val >> 7) & 0x1) << 1;
      bin[3] &= 0x1f;
      bin[3] |= ((enc_val >> 8) & 0x7) << 5;
      return 11;

   case PCO_SRC_MUX_3BIT_B3:
      enc_val = val;
      assert(pco_is0_sel_valid(enc_val));
      bin[2] &= 0x9f;
      bin[2] |= (enc_val & 0x3) << 5;
      bin[3] &= 0xef;
      bin[3] |= ((enc_val >> 2) & 0x1) << 4;
      return 3;

   case PCO_SRC_SBA_3BIT_B3:
      enc_val = val;
      assert(pco_regbank_valid(enc_val));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      bin[2] &= 0xef;
      bin[2] |= ((enc_val >> 1) & 0x1) << 4;
      bin[3] &= 0xf7;
      bin[3] |= ((enc_val >> 2) & 0x1) << 3;
      return 3;

   case PCO_SRC_RSVD3:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[3] &= 0xfb;
      bin[3] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_SRC_SB_8BIT_B3:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[1] &= 0xe0;
      bin[1] |= (enc_val & 0x1f);
      bin[2] &= 0xfc;
      bin[2] |= ((enc_val >> 5) & 0x3);
      bin[3] &= 0xfe;
      bin[3] |= ((enc_val >> 7) & 0x1);
      return 8;

   case PCO_SRC_SBC_2BIT_B3:
      enc_val = val;
      assert(pco_regbank2_valid(enc_val));
      bin[3] &= 0x3f;
      bin[3] |= (enc_val & 0x3) << 6;
      return 2;

   case PCO_SRC_SC_6BIT_B3:
      enc_val = val;
      assert(enc_val < (1ULL << 6));
      bin[3] &= 0xc0;
      bin[3] |= (enc_val & 0x3f);
      return 6;

   case PCO_SRC_RSVD4:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0x7f;
      bin[4] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_SRC_SBC_3BIT_B4:
      enc_val = val;
      assert(pco_regbank_valid(enc_val));
      bin[3] &= 0x3f;
      bin[3] |= (enc_val & 0x3) << 6;
      bin[4] &= 0x7f;
      bin[4] |= ((enc_val >> 2) & 0x1) << 7;
      return 3;

   case PCO_SRC_SC_8BIT_B4:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[3] &= 0xc0;
      bin[3] |= (enc_val & 0x3f);
      bin[4] &= 0x9f;
      bin[4] |= ((enc_val >> 6) & 0x3) << 5;
      return 8;

   case PCO_SRC_MUX_3BIT_B4:
      enc_val = val;
      assert(pco_is0_sel_valid(enc_val));
      bin[2] &= 0x9f;
      bin[2] |= (enc_val & 0x3) << 5;
      bin[4] &= 0xef;
      bin[4] |= ((enc_val >> 2) & 0x1) << 4;
      return 3;

   case PCO_SRC_SBA_3BIT_B4:
      enc_val = val;
      assert(pco_regbank_valid(enc_val));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      bin[2] &= 0xef;
      bin[2] |= ((enc_val >> 1) & 0x1) << 4;
      bin[4] &= 0xf7;
      bin[4] |= ((enc_val >> 2) & 0x1) << 3;
      return 3;

   case PCO_SRC_EXT4:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xfb;
      bin[4] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_SRC_SA_8BIT_B4:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[0] &= 0xc0;
      bin[0] |= (enc_val & 0x3f);
      bin[2] &= 0xfb;
      bin[2] |= ((enc_val >> 6) & 0x1) << 2;
      bin[4] &= 0xfd;
      bin[4] |= ((enc_val >> 7) & 0x1) << 1;
      return 8;

   case PCO_SRC_SB_8BIT_B4:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[1] &= 0xe0;
      bin[1] |= (enc_val & 0x1f);
      bin[2] &= 0xfc;
      bin[2] |= ((enc_val >> 5) & 0x3);
      bin[4] &= 0xfe;
      bin[4] |= ((enc_val >> 7) & 0x1);
      return 8;

   case PCO_SRC_RSVD5:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[5] &= 0x3f;
      bin[5] |= (enc_val & 0x3) << 6;
      return 2;

   case PCO_SRC_RSVD5_:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 3));
      bin[5] &= 0xc7;
      bin[5] |= (enc_val & 0x7) << 3;
      return 3;

   case PCO_SRC_SC_11BIT_B5:
      enc_val = val;
      assert(enc_val < (1ULL << 11));
      bin[3] &= 0xc0;
      bin[3] |= (enc_val & 0x3f);
      bin[4] &= 0x9f;
      bin[4] |= ((enc_val >> 6) & 0x3) << 5;
      bin[5] &= 0xc7;
      bin[5] |= ((enc_val >> 8) & 0x7) << 3;
      return 11;

   case PCO_SRC_SA_11BIT_B5:
      enc_val = val;
      assert(enc_val < (1ULL << 11));
      bin[0] &= 0xc0;
      bin[0] |= (enc_val & 0x3f);
      bin[2] &= 0xfb;
      bin[2] |= ((enc_val >> 6) & 0x1) << 2;
      bin[4] &= 0xfd;
      bin[4] |= ((enc_val >> 7) & 0x1) << 1;
      bin[5] &= 0xf8;
      bin[5] |= ((enc_val >> 8) & 0x7);
      return 11;

   default:
      break;
   }

   UNREACHABLE("");
}

struct pco_src_1lo_1b6i {
   enum pco_regbank sb0 : 3;
   unsigned s0 : 6;
};

#define pco_src_1lo_1b6i_encode(bin, ...) _pco_src_1lo_1b6i_encode(bin, (const struct pco_src_1lo_1b6i){0, ##__VA_ARGS__})
static unsigned _pco_src_1lo_1b6i_encode(uint8_t *bin, const struct pco_src_1lo_1b6i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_1BIT_B0, s.sb0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_6BIT_B0, s.s0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_1lo_3b11i_2m {
   enum pco_regbank sb0 : 3;
   unsigned s0 : 11;
   enum pco_is0_sel is0 : 3;
};

#define pco_src_1lo_3b11i_2m_encode(bin, ...) _pco_src_1lo_3b11i_2m_encode(bin, (const struct pco_src_1lo_3b11i_2m){0, ##__VA_ARGS__})
static unsigned _pco_src_1lo_3b11i_2m_encode(uint8_t *bin, const struct pco_src_1lo_3b11i_2m s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD2, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_3BIT_B1, s.sb0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_11BIT_B2, s.s0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_2BIT_B1, s.is0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_2lo_1b6i_1b5i {
   enum pco_regbank sb0 : 3;
   unsigned s0 : 6;
   enum pco_regbank sb1 : 3;
   unsigned s1 : 5;
};

#define pco_src_2lo_1b6i_1b5i_encode(bin, ...) _pco_src_2lo_1b6i_1b5i_encode(bin, (const struct pco_src_2lo_1b6i_1b5i){0, ##__VA_ARGS__})
static unsigned _pco_src_2lo_1b6i_1b5i_encode(uint8_t *bin, const struct pco_src_2lo_1b6i_1b5i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_1BIT_B0, s.sb0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_6BIT_B0, s.s0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_1BIT_B1, s.sb1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_5BIT_B1, s.s1);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_2lo_2b7i_2b7i_2m {
   enum pco_regbank sb0 : 3;
   unsigned s0 : 7;
   enum pco_regbank sb1 : 3;
   unsigned s1 : 7;
   enum pco_is0_sel is0 : 3;
};

#define pco_src_2lo_2b7i_2b7i_2m_encode(bin, ...) _pco_src_2lo_2b7i_2b7i_2m_encode(bin, (const struct pco_src_2lo_2b7i_2b7i_2m){0, ##__VA_ARGS__})
static unsigned _pco_src_2lo_2b7i_2b7i_2m_encode(uint8_t *bin, const struct pco_src_2lo_2b7i_2b7i_2m s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_2BIT_B2, s.sb0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_7BIT_B2, s.s0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_7BIT_B2, s.s1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_2BIT_B2, s.is0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_2lo_3b11i_2b8i_3m {
   enum pco_regbank sb0 : 3;
   unsigned s0 : 11;
   enum pco_regbank sb1 : 3;
   unsigned s1 : 8;
   enum pco_is0_sel is0 : 3;
};

#define pco_src_2lo_3b11i_2b8i_3m_encode(bin, ...) _pco_src_2lo_3b11i_2b8i_3m_encode(bin, (const struct pco_src_2lo_3b11i_2b8i_3m){0, ##__VA_ARGS__})
static unsigned _pco_src_2lo_3b11i_2b8i_3m_encode(uint8_t *bin, const struct pco_src_2lo_3b11i_2b8i_3m s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD3, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_3BIT_B3, s.sb0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_11BIT_B3, s.s0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_8BIT_B3, s.s1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_3BIT_B3, s.is0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_3lo_2b7i_2b7i_2b6i_2m {
   enum pco_regbank sb0 : 3;
   unsigned s0 : 7;
   enum pco_regbank sb1 : 3;
   unsigned s1 : 7;
   enum pco_regbank sb2 : 3;
   unsigned s2 : 6;
   enum pco_is0_sel is0 : 3;
};

#define pco_src_3lo_2b7i_2b7i_2b6i_2m_encode(bin, ...) _pco_src_3lo_2b7i_2b7i_2b6i_2m_encode(bin, (const struct pco_src_3lo_2b7i_2b7i_2b6i_2m){0, ##__VA_ARGS__})
static unsigned _pco_src_3lo_2b7i_2b7i_2b6i_2m_encode(uint8_t *bin, const struct pco_src_3lo_2b7i_2b7i_2b6i_2m s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_2BIT_B2, s.sb0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_7BIT_B2, s.s0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_7BIT_B2, s.s1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBC_2BIT_B3, s.sb2);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SC_6BIT_B3, s.s2);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_2BIT_B2, s.is0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_3lo_3b8i_2b8i_3b8i_3m {
   enum pco_regbank sb0 : 3;
   unsigned s0 : 8;
   enum pco_regbank sb1 : 3;
   unsigned s1 : 8;
   enum pco_regbank sb2 : 3;
   unsigned s2 : 8;
   enum pco_is0_sel is0 : 3;
};

#define pco_src_3lo_3b8i_2b8i_3b8i_3m_encode(bin, ...) _pco_src_3lo_3b8i_2b8i_3b8i_3m_encode(bin, (const struct pco_src_3lo_3b8i_2b8i_3b8i_3m){0, ##__VA_ARGS__})
static unsigned _pco_src_3lo_3b8i_2b8i_3b8i_3m_encode(uint8_t *bin, const struct pco_src_3lo_3b8i_2b8i_3b8i_3m s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT4, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_3BIT_B4, s.sb0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_8BIT_B4, s.s0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_8BIT_B4, s.s1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBC_3BIT_B4, s.sb2);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SC_8BIT_B4, s.s2);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_3BIT_B4, s.is0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_3lo_3b11i_2b8i_3b11i_3m {
   enum pco_regbank sb0 : 3;
   unsigned s0 : 11;
   enum pco_regbank sb1 : 3;
   unsigned s1 : 8;
   enum pco_regbank sb2 : 3;
   unsigned s2 : 11;
   enum pco_is0_sel is0 : 3;
};

#define pco_src_3lo_3b11i_2b8i_3b11i_3m_encode(bin, ...) _pco_src_3lo_3b11i_2b8i_3b11i_3m_encode(bin, (const struct pco_src_3lo_3b11i_2b8i_3b11i_3m){0, ##__VA_ARGS__})
static unsigned _pco_src_3lo_3b11i_2b8i_3b11i_3m_encode(uint8_t *bin, const struct pco_src_3lo_3b11i_2b8i_3b11i_3m s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT4, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD5, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_3BIT_B4, s.sb0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_11BIT_B5, s.s0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_8BIT_B4, s.s1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBC_3BIT_B4, s.sb2);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SC_11BIT_B5, s.s2);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_3BIT_B4, s.is0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_1up_1b6i {
   enum pco_regbank sb3 : 3;
   unsigned s3 : 6;
};

#define pco_src_1up_1b6i_encode(bin, ...) _pco_src_1up_1b6i_encode(bin, (const struct pco_src_1up_1b6i){0, ##__VA_ARGS__})
static unsigned _pco_src_1up_1b6i_encode(uint8_t *bin, const struct pco_src_1up_1b6i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_1BIT_B0, s.sb3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_6BIT_B0, s.s3);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_1up_3b11i {
   enum pco_regbank sb3 : 3;
   unsigned s3 : 11;
};

#define pco_src_1up_3b11i_encode(bin, ...) _pco_src_1up_3b11i_encode(bin, (const struct pco_src_1up_3b11i){0, ##__VA_ARGS__})
static unsigned _pco_src_1up_3b11i_encode(uint8_t *bin, const struct pco_src_1up_3b11i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_2BIT_B1, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD2, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_3BIT_B1, s.sb3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_11BIT_B2, s.s3);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_2up_1b6i_1b5i {
   enum pco_regbank sb3 : 3;
   unsigned s3 : 6;
   enum pco_regbank sb4 : 3;
   unsigned s4 : 5;
};

#define pco_src_2up_1b6i_1b5i_encode(bin, ...) _pco_src_2up_1b6i_1b5i_encode(bin, (const struct pco_src_2up_1b6i_1b5i){0, ##__VA_ARGS__})
static unsigned _pco_src_2up_1b6i_1b5i_encode(uint8_t *bin, const struct pco_src_2up_1b6i_1b5i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_1BIT_B0, s.sb3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_6BIT_B0, s.s3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_1BIT_B1, s.sb4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_5BIT_B1, s.s4);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_2up_2b7i_2b7i {
   enum pco_regbank sb3 : 3;
   unsigned s3 : 7;
   enum pco_regbank sb4 : 3;
   unsigned s4 : 7;
};

#define pco_src_2up_2b7i_2b7i_encode(bin, ...) _pco_src_2up_2b7i_2b7i_encode(bin, (const struct pco_src_2up_2b7i_2b7i){0, ##__VA_ARGS__})
static unsigned _pco_src_2up_2b7i_2b7i_encode(uint8_t *bin, const struct pco_src_2up_2b7i_2b7i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_2BIT_B2, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_2BIT_B2, s.sb3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_7BIT_B2, s.s3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_7BIT_B2, s.s4);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_2up_3b11i_2b8i {
   enum pco_regbank sb3 : 3;
   unsigned s3 : 11;
   enum pco_regbank sb4 : 3;
   unsigned s4 : 8;
};

#define pco_src_2up_3b11i_2b8i_encode(bin, ...) _pco_src_2up_3b11i_2b8i_encode(bin, (const struct pco_src_2up_3b11i_2b8i){0, ##__VA_ARGS__})
static unsigned _pco_src_2up_3b11i_2b8i_encode(uint8_t *bin, const struct pco_src_2up_3b11i_2b8i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD3, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_3BIT_B3, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_3BIT_B3, s.sb3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_11BIT_B3, s.s3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_8BIT_B3, s.s4);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_3up_2b7i_2b7i_2b6i {
   enum pco_regbank sb3 : 3;
   unsigned s3 : 7;
   enum pco_regbank sb4 : 3;
   unsigned s4 : 7;
   enum pco_regbank sb5 : 3;
   unsigned s5 : 6;
};

#define pco_src_3up_2b7i_2b7i_2b6i_encode(bin, ...) _pco_src_3up_2b7i_2b7i_2b6i_encode(bin, (const struct pco_src_3up_2b7i_2b7i_2b6i){0, ##__VA_ARGS__})
static unsigned _pco_src_3up_2b7i_2b7i_2b6i_encode(uint8_t *bin, const struct pco_src_3up_2b7i_2b7i_2b6i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_2BIT_B2, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_2BIT_B2, s.sb3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_7BIT_B2, s.s3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_7BIT_B2, s.s4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBC_2BIT_B3, s.sb5);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SC_6BIT_B3, s.s5);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_3up_3b8i_2b8i_2b8i {
   enum pco_regbank sb3 : 3;
   unsigned s3 : 8;
   enum pco_regbank sb4 : 3;
   unsigned s4 : 8;
   enum pco_regbank sb5 : 3;
   unsigned s5 : 8;
};

#define pco_src_3up_3b8i_2b8i_2b8i_encode(bin, ...) _pco_src_3up_3b8i_2b8i_2b8i_encode(bin, (const struct pco_src_3up_3b8i_2b8i_2b8i){0, ##__VA_ARGS__})
static unsigned _pco_src_3up_3b8i_2b8i_2b8i_encode(uint8_t *bin, const struct pco_src_3up_3b8i_2b8i_2b8i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT4, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_3BIT_B4, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD4, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_3BIT_B4, s.sb3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_8BIT_B4, s.s3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_8BIT_B4, s.s4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBC_2BIT_B3, s.sb5);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SC_8BIT_B4, s.s5);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_src_3up_3b11i_2b8i_2b8i {
   enum pco_regbank sb3 : 3;
   unsigned s3 : 11;
   enum pco_regbank sb4 : 3;
   unsigned s4 : 8;
   enum pco_regbank sb5 : 3;
   unsigned s5 : 8;
};

#define pco_src_3up_3b11i_2b8i_2b8i_encode(bin, ...) _pco_src_3up_3b11i_2b8i_2b8i_encode(bin, (const struct pco_src_3up_3b11i_2b8i_2b8i){0, ##__VA_ARGS__})
static unsigned _pco_src_3up_3b11i_2b8i_2b8i_encode(uint8_t *bin, const struct pco_src_3up_3b11i_2b8i_2b8i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT0, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SEL, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT1, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT2, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_EXT4, 1);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_MUX_3BIT_B4, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD4, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD5, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_RSVD5_, 0);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBA_3BIT_B4, s.sb3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SA_11BIT_B5, s.s3);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBB_2BIT_B2, s.sb4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SB_8BIT_B4, s.s4);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SBC_2BIT_B3, s.sb5);
   bits_encoded += pco_src_encode_field(bin, PCO_SRC_SC_8BIT_B4, s.s5);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

/** iss */
enum pco_iss_field {
   PCO_ISS_IS5,
   PCO_ISS_IS4,
   PCO_ISS_IS3,
   PCO_ISS_IS2,
   PCO_ISS_IS1,
};

static unsigned pco_iss_bytes(enum pco_iss_variant variant)
{
   switch (variant) {
   case PCO_ISS_NONE: return 0;
   case PCO_ISS_ISS: return 1;
   default: break;
   }

   UNREACHABLE("");
}

static unsigned pco_iss_encode_field(uint8_t *bin, enum pco_iss_field field, uint64_t val)
{
   uint64_t enc_val;

   switch (field) {
   case PCO_ISS_IS5:
      enc_val = val;
      assert(pco_is5_sel_valid(enc_val));
      bin[0] &= 0x3f;
      bin[0] |= (enc_val & 0x3) << 6;
      return 2;

   case PCO_ISS_IS4:
      enc_val = val;
      assert(pco_is4_sel_valid(enc_val));
      bin[0] &= 0xcf;
      bin[0] |= (enc_val & 0x3) << 4;
      return 2;

   case PCO_ISS_IS3:
      enc_val = val;
      assert(pco_is3_sel_valid(enc_val));
      bin[0] &= 0xf3;
      bin[0] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_ISS_IS2:
      enc_val = val;
      assert(pco_is2_sel_valid(enc_val));
      bin[0] &= 0xfd;
      bin[0] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_ISS_IS1:
      enc_val = val;
      assert(pco_is1_sel_valid(enc_val));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   default:
      break;
   }

   UNREACHABLE("");
}

struct pco_iss_iss {
   enum pco_is5_sel is5 : 2;
   enum pco_is4_sel is4 : 2;
   enum pco_is3_sel is3 : 2;
   enum pco_is2_sel is2 : 1;
   enum pco_is1_sel is1 : 1;
};

#define pco_iss_iss_encode(bin, ...) _pco_iss_iss_encode(bin, (const struct pco_iss_iss){0, ##__VA_ARGS__})
static unsigned _pco_iss_iss_encode(uint8_t *bin, const struct pco_iss_iss s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_iss_encode_field(bin, PCO_ISS_IS5, s.is5);
   bits_encoded += pco_iss_encode_field(bin, PCO_ISS_IS4, s.is4);
   bits_encoded += pco_iss_encode_field(bin, PCO_ISS_IS3, s.is3);
   bits_encoded += pco_iss_encode_field(bin, PCO_ISS_IS2, s.is2);
   bits_encoded += pco_iss_encode_field(bin, PCO_ISS_IS1, s.is1);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

/** dst */
enum pco_dst_field {
   PCO_DST_EXT0,
   PCO_DST_DBN_1BIT_B0,
   PCO_DST_DN_6BIT_B0,
   PCO_DST_RSVD1,
   PCO_DST_DBN_3BIT_B1,
   PCO_DST_DN_11BIT_B1,
   PCO_DST_DB0_1BIT_B0,
   PCO_DST_D0_7BIT_B0,
   PCO_DST_EXT1,
   PCO_DST_DB1_1BIT_B1,
   PCO_DST_D1_6BIT_B1,
   PCO_DST_EXT2,
   PCO_DST_DB0_3BIT_B2,
   PCO_DST_D0_8BIT_B2,
   PCO_DST_DB1_3BIT_B2,
   PCO_DST_D1_8BIT_B2,
   PCO_DST_RSVD3,
   PCO_DST_D0_11BIT_B3,
   PCO_DST_D1_11BIT_B3,
};

static unsigned pco_dst_bytes(enum pco_dst_variant variant)
{
   switch (variant) {
   case PCO_DST_NONE: return 0;
   case PCO_DST_1_1B6I: return 1;
   case PCO_DST_1_3B11I: return 2;
   case PCO_DST_2_1B7I_1B6I: return 2;
   case PCO_DST_2_3B8I_3B8I: return 3;
   case PCO_DST_2_3B11I_3B11I: return 4;
   default: break;
   }

   UNREACHABLE("");
}

static unsigned pco_dst_encode_field(uint8_t *bin, enum pco_dst_field field, uint64_t val)
{
   uint64_t enc_val;

   switch (field) {
   case PCO_DST_EXT0:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0x7f;
      bin[0] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_DST_DBN_1BIT_B0:
      enc_val = val;
      assert(pco_regbank1_valid(enc_val));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_DST_DN_6BIT_B0:
      enc_val = val;
      assert(enc_val < (1ULL << 6));
      bin[0] &= 0xc0;
      bin[0] |= (enc_val & 0x3f);
      return 6;

   case PCO_DST_RSVD1:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_DST_DBN_3BIT_B1:
      enc_val = val;
      assert(pco_regbank_valid(enc_val));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      bin[1] &= 0xf3;
      bin[1] |= ((enc_val >> 1) & 0x3) << 2;
      return 3;

   case PCO_DST_DN_11BIT_B1:
      enc_val = val;
      assert(enc_val < (1ULL << 11));
      bin[0] &= 0xc0;
      bin[0] |= (enc_val & 0x3f);
      bin[1] &= 0xfc;
      bin[1] |= ((enc_val >> 6) & 0x3);
      bin[1] &= 0x8f;
      bin[1] |= ((enc_val >> 8) & 0x7) << 4;
      return 11;

   case PCO_DST_DB0_1BIT_B0:
      enc_val = val;
      assert(pco_regbank1_valid(enc_val));
      bin[0] &= 0x7f;
      bin[0] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_DST_D0_7BIT_B0:
      enc_val = val;
      assert(enc_val < (1ULL << 7));
      bin[0] &= 0x80;
      bin[0] |= (enc_val & 0x7f);
      return 7;

   case PCO_DST_EXT1:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_DST_DB1_1BIT_B1:
      enc_val = val;
      assert(pco_regbank1_valid(enc_val));
      bin[1] &= 0xbf;
      bin[1] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_DST_D1_6BIT_B1:
      enc_val = val;
      assert(enc_val < (1ULL << 6));
      bin[1] &= 0xc0;
      bin[1] |= (enc_val & 0x3f);
      return 6;

   case PCO_DST_EXT2:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0x7f;
      bin[2] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_DST_DB0_3BIT_B2:
      enc_val = val;
      assert(pco_regbank_valid(enc_val));
      bin[0] &= 0x7f;
      bin[0] |= (enc_val & 0x1) << 7;
      bin[2] &= 0xf9;
      bin[2] |= ((enc_val >> 1) & 0x3) << 1;
      return 3;

   case PCO_DST_D0_8BIT_B2:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[0] &= 0x80;
      bin[0] |= (enc_val & 0x7f);
      bin[2] &= 0xfe;
      bin[2] |= ((enc_val >> 7) & 0x1);
      return 8;

   case PCO_DST_DB1_3BIT_B2:
      enc_val = val;
      assert(pco_regbank_valid(enc_val));
      bin[1] &= 0xbf;
      bin[1] |= (enc_val & 0x1) << 6;
      bin[2] &= 0x9f;
      bin[2] |= ((enc_val >> 1) & 0x3) << 5;
      return 3;

   case PCO_DST_D1_8BIT_B2:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[1] &= 0xc0;
      bin[1] |= (enc_val & 0x3f);
      bin[2] &= 0xe7;
      bin[2] |= ((enc_val >> 6) & 0x3) << 3;
      return 8;

   case PCO_DST_RSVD3:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[3] &= 0x3f;
      bin[3] |= (enc_val & 0x3) << 6;
      return 2;

   case PCO_DST_D0_11BIT_B3:
      enc_val = val;
      assert(enc_val < (1ULL << 11));
      bin[0] &= 0x80;
      bin[0] |= (enc_val & 0x7f);
      bin[2] &= 0xfe;
      bin[2] |= ((enc_val >> 7) & 0x1);
      bin[3] &= 0xf8;
      bin[3] |= ((enc_val >> 8) & 0x7);
      return 11;

   case PCO_DST_D1_11BIT_B3:
      enc_val = val;
      assert(enc_val < (1ULL << 11));
      bin[1] &= 0xc0;
      bin[1] |= (enc_val & 0x3f);
      bin[2] &= 0xe7;
      bin[2] |= ((enc_val >> 6) & 0x3) << 3;
      bin[3] &= 0xc7;
      bin[3] |= ((enc_val >> 8) & 0x7) << 3;
      return 11;

   default:
      break;
   }

   UNREACHABLE("");
}

struct pco_dst_1_1b6i {
   enum pco_regbank dbN : 3;
   unsigned dN : 6;
};

#define pco_dst_1_1b6i_encode(bin, ...) _pco_dst_1_1b6i_encode(bin, (const struct pco_dst_1_1b6i){0, ##__VA_ARGS__})
static unsigned _pco_dst_1_1b6i_encode(uint8_t *bin, const struct pco_dst_1_1b6i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_dst_encode_field(bin, PCO_DST_EXT0, 0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DBN_1BIT_B0, s.dbN);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DN_6BIT_B0, s.dN);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_dst_1_3b11i {
   enum pco_regbank dbN : 3;
   unsigned dN : 11;
};

#define pco_dst_1_3b11i_encode(bin, ...) _pco_dst_1_3b11i_encode(bin, (const struct pco_dst_1_3b11i){0, ##__VA_ARGS__})
static unsigned _pco_dst_1_3b11i_encode(uint8_t *bin, const struct pco_dst_1_3b11i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_dst_encode_field(bin, PCO_DST_EXT0, 1);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_RSVD1, 0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DBN_3BIT_B1, s.dbN);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DN_11BIT_B1, s.dN);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_dst_2_1b7i_1b6i {
   enum pco_regbank db0 : 3;
   unsigned d0 : 7;
   enum pco_regbank db1 : 3;
   unsigned d1 : 6;
};

#define pco_dst_2_1b7i_1b6i_encode(bin, ...) _pco_dst_2_1b7i_1b6i_encode(bin, (const struct pco_dst_2_1b7i_1b6i){0, ##__VA_ARGS__})
static unsigned _pco_dst_2_1b7i_1b6i_encode(uint8_t *bin, const struct pco_dst_2_1b7i_1b6i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_dst_encode_field(bin, PCO_DST_EXT1, 0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DB0_1BIT_B0, s.db0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_D0_7BIT_B0, s.d0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DB1_1BIT_B1, s.db1);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_D1_6BIT_B1, s.d1);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_dst_2_3b8i_3b8i {
   enum pco_regbank db0 : 3;
   unsigned d0 : 8;
   enum pco_regbank db1 : 3;
   unsigned d1 : 8;
};

#define pco_dst_2_3b8i_3b8i_encode(bin, ...) _pco_dst_2_3b8i_3b8i_encode(bin, (const struct pco_dst_2_3b8i_3b8i){0, ##__VA_ARGS__})
static unsigned _pco_dst_2_3b8i_3b8i_encode(uint8_t *bin, const struct pco_dst_2_3b8i_3b8i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_dst_encode_field(bin, PCO_DST_EXT1, 1);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_EXT2, 0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DB0_3BIT_B2, s.db0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_D0_8BIT_B2, s.d0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DB1_3BIT_B2, s.db1);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_D1_8BIT_B2, s.d1);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_dst_2_3b11i_3b11i {
   enum pco_regbank db0 : 3;
   unsigned d0 : 11;
   enum pco_regbank db1 : 3;
   unsigned d1 : 11;
};

#define pco_dst_2_3b11i_3b11i_encode(bin, ...) _pco_dst_2_3b11i_3b11i_encode(bin, (const struct pco_dst_2_3b11i_3b11i){0, ##__VA_ARGS__})
static unsigned _pco_dst_2_3b11i_3b11i_encode(uint8_t *bin, const struct pco_dst_2_3b11i_3b11i s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_dst_encode_field(bin, PCO_DST_EXT1, 1);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_EXT2, 1);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_RSVD3, 0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DB0_3BIT_B2, s.db0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_D0_11BIT_B3, s.d0);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_DB1_3BIT_B2, s.db1);
   bits_encoded += pco_dst_encode_field(bin, PCO_DST_D1_11BIT_B3, s.d1);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

/** main */
enum pco_main_field {
   PCO_MAIN_MAIN_OP,
   PCO_MAIN_EXT0,
   PCO_MAIN_SAT_FAM,
   PCO_MAIN_S0NEG_FAM,
   PCO_MAIN_S0ABS_FAM,
   PCO_MAIN_S1ABS_FAM,
   PCO_MAIN_S0FLR_FAM,
   PCO_MAIN_SNGL_OP,
   PCO_MAIN_RSVD1_SNGL,
   PCO_MAIN_RSVD1_SNGL_,
   PCO_MAIN_RED_PART,
   PCO_MAIN_ITER,
   PCO_MAIN_RED_TYPE,
   PCO_MAIN_PWEN_RED,
   PCO_MAIN_GAMMAOP,
   PCO_MAIN_S0NEG_SNGL,
   PCO_MAIN_S0ABS_SNGL,
   PCO_MAIN_UPCK_ELEM,
   PCO_MAIN_SCALE_RTZ,
   PCO_MAIN_PROG,
   PCO_MAIN_RTZ,
   PCO_MAIN_SCALE,
   PCO_MAIN_PCK_FORMAT,
   PCO_MAIN_INT8_16_OP,
   PCO_MAIN_S_I816,
   PCO_MAIN_F_I816,
   PCO_MAIN_S2CH,
   PCO_MAIN_RSVD1_I816,
   PCO_MAIN_S2NEG_I816,
   PCO_MAIN_S2ABS_I816,
   PCO_MAIN_S1ABS_I816,
   PCO_MAIN_S0NEG_I816,
   PCO_MAIN_S0ABS_I816,
   PCO_MAIN_SAT_I816,
   PCO_MAIN_RSVD2_I816,
   PCO_MAIN_S1CH,
   PCO_MAIN_S0CH,
   PCO_MAIN_S0NEG_FMA,
   PCO_MAIN_S0ABS_FMA,
   PCO_MAIN_S2NEG_FMA,
   PCO_MAIN_SAT_FMA,
   PCO_MAIN_RSVD1_FMA,
   PCO_MAIN_LP_FMA,
   PCO_MAIN_S1ABS_FMA,
   PCO_MAIN_S1NEG_FMA,
   PCO_MAIN_S2FLR_FMA,
   PCO_MAIN_S2ABS_FMA,
   PCO_MAIN_S_I3264,
   PCO_MAIN_S2NEG_I3264,
   PCO_MAIN_INT32_64_OP,
   PCO_MAIN_RSVD1_I3264,
   PCO_MAIN_CIN_I3264,
   PCO_MAIN_S1NEG_I3264,
   PCO_MAIN_S0NEG_I3264,
   PCO_MAIN_S0ABS_I3264,
   PCO_MAIN_S1ABS_I3264,
   PCO_MAIN_S2ABS_I3264,
   PCO_MAIN_MOVW1,
   PCO_MAIN_MOVW0,
   PCO_MAIN_RSVD1_MOVC,
   PCO_MAIN_MASKW0,
   PCO_MAIN_AW,
   PCO_MAIN_P2END_MOVC,
   PCO_MAIN_TST_OP_3BIT,
   PCO_MAIN_PWEN_TST,
   PCO_MAIN_TST_TYPE,
   PCO_MAIN_P2END_TST,
   PCO_MAIN_TST_ELEM,
   PCO_MAIN_RSVD1_TST,
   PCO_MAIN_TST_OP_4BIT,
};

static unsigned pco_main_bytes(enum pco_main_variant variant)
{
   switch (variant) {
   case PCO_MAIN_NONE: return 0;
   case PCO_MAIN_FADD: return 1;
   case PCO_MAIN_FADD_LP: return 1;
   case PCO_MAIN_FMUL: return 1;
   case PCO_MAIN_FMUL_LP: return 1;
   case PCO_MAIN_SNGL: return 1;
   case PCO_MAIN_SNGL_EXT: return 2;
   case PCO_MAIN_GCMP: return 1;
   case PCO_MAIN_GCMP_EXT: return 2;
   case PCO_MAIN_GEXP: return 2;
   case PCO_MAIN_PCK: return 2;
   case PCO_MAIN_UPCK: return 2;
   case PCO_MAIN_FRED: return 2;
   case PCO_MAIN_INT8_16: return 1;
   case PCO_MAIN_INT8_16_EXT: return 2;
   case PCO_MAIN_INT8_16_EXT_SEL: return 3;
   case PCO_MAIN_FMAD: return 1;
   case PCO_MAIN_FMAD_EXT: return 2;
   case PCO_MAIN_INT32_64: return 1;
   case PCO_MAIN_INT32_64_EXT: return 2;
   case PCO_MAIN_MOVC: return 1;
   case PCO_MAIN_MOVC_EXT: return 2;
   case PCO_MAIN_TST: return 1;
   case PCO_MAIN_TST_EXT: return 2;
   default: break;
   }

   UNREACHABLE("");
}

static unsigned pco_main_encode_field(uint8_t *bin, enum pco_main_field field, uint64_t val)
{
   uint64_t enc_val;

   switch (field) {
   case PCO_MAIN_MAIN_OP:
      enc_val = val;
      assert(pco_main_op_valid(enc_val));
      bin[0] &= 0x1f;
      bin[0] |= (enc_val & 0x7) << 5;
      return 3;

   case PCO_MAIN_EXT0:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_MAIN_SAT_FAM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_MAIN_S0NEG_FAM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xf7;
      bin[0] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_MAIN_S0ABS_FAM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfb;
      bin[0] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_S1ABS_FAM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfd;
      bin[0] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_S0FLR_FAM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_SNGL_OP:
      enc_val = val;
      assert(pco_sngl_op_valid(enc_val));
      bin[0] &= 0xf0;
      bin[0] |= (enc_val & 0xf);
      return 4;

   case PCO_MAIN_RSVD1_SNGL:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 5));
      bin[1] &= 0xf7;
      bin[1] |= (enc_val & 0x1) << 3;
      bin[1] &= 0x8f;
      bin[1] |= ((enc_val >> 1) & 0x7) << 4;
      bin[1] &= 0x7f;
      bin[1] |= ((enc_val >> 4) & 0x1) << 7;
      return 5;

   case PCO_MAIN_RSVD1_SNGL_:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfb;
      bin[1] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_RED_PART:
      enc_val = val;
      assert(pco_red_part_valid(enc_val));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_MAIN_ITER:
      enc_val = val;
      assert(enc_val < (1ULL << 3));
      bin[1] &= 0x8f;
      bin[1] |= (enc_val & 0x7) << 4;
      return 3;

   case PCO_MAIN_RED_TYPE:
      enc_val = val;
      assert(pco_red_type_valid(enc_val));
      bin[1] &= 0xf7;
      bin[1] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_MAIN_PWEN_RED:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfb;
      bin[1] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_GAMMAOP:
      enc_val = val;
      assert(pco_gamma_op_valid(enc_val));
      bin[1] &= 0xfb;
      bin[1] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_S0NEG_SNGL:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfd;
      bin[1] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_S0ABS_SNGL:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfe;
      bin[1] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_UPCK_ELEM:
      enc_val = val;
      assert(pco_upck_elem_valid(enc_val));
      bin[1] &= 0x3f;
      bin[1] |= (enc_val & 0x3) << 6;
      return 2;

   case PCO_MAIN_SCALE_RTZ:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xdf;
      bin[1] |= (enc_val & 0x1) << 5;
      return 1;

   case PCO_MAIN_PROG:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_MAIN_RTZ:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xbf;
      bin[1] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_MAIN_SCALE:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xdf;
      bin[1] |= (enc_val & 0x1) << 5;
      return 1;

   case PCO_MAIN_PCK_FORMAT:
      enc_val = val;
      assert(pco_pck_format_valid(enc_val));
      bin[1] &= 0xe0;
      bin[1] |= (enc_val & 0x1f);
      return 5;

   case PCO_MAIN_INT8_16_OP:
      enc_val = val;
      assert(pco_int8_16_op_valid(enc_val));
      bin[0] &= 0xf3;
      bin[0] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_MAIN_S_I816:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfd;
      bin[0] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_F_I816:
      enc_val = val;
      assert(pco_int8_16_fmt_valid(enc_val));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_S2CH:
      enc_val = val;
      assert(pco_s2ch_valid(enc_val));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_MAIN_RSVD1_I816:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xbf;
      bin[1] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_MAIN_S2NEG_I816:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xdf;
      bin[1] |= (enc_val & 0x1) << 5;
      return 1;

   case PCO_MAIN_S2ABS_I816:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xef;
      bin[1] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_MAIN_S1ABS_I816:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xf7;
      bin[1] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_MAIN_S0NEG_I816:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfb;
      bin[1] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_S0ABS_I816:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfd;
      bin[1] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_SAT_I816:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfe;
      bin[1] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_RSVD2_I816:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 4));
      bin[2] &= 0xf;
      bin[2] |= (enc_val & 0xf) << 4;
      return 4;

   case PCO_MAIN_S1CH:
      enc_val = val;
      assert(pco_s01ch_valid(enc_val));
      bin[2] &= 0xf3;
      bin[2] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_MAIN_S0CH:
      enc_val = val;
      assert(pco_s01ch_valid(enc_val));
      bin[2] &= 0xfc;
      bin[2] |= (enc_val & 0x3);
      return 2;

   case PCO_MAIN_S0NEG_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xf7;
      bin[0] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_MAIN_S0ABS_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfb;
      bin[0] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_S2NEG_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfd;
      bin[0] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_SAT_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_RSVD1_FMA:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 3));
      bin[1] &= 0x1f;
      bin[1] |= (enc_val & 0x7) << 5;
      return 3;

   case PCO_MAIN_LP_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xef;
      bin[1] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_MAIN_S1ABS_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xf7;
      bin[1] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_MAIN_S1NEG_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfb;
      bin[1] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_S2FLR_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfd;
      bin[1] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_S2ABS_FMA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfe;
      bin[1] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_S_I3264:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xf7;
      bin[0] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_MAIN_S2NEG_I3264:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfb;
      bin[0] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_INT32_64_OP:
      enc_val = val;
      assert(pco_int32_64_op_valid(enc_val));
      bin[0] &= 0xfc;
      bin[0] |= (enc_val & 0x3);
      return 2;

   case PCO_MAIN_RSVD1_I3264:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      bin[1] &= 0xf7;
      bin[1] |= ((enc_val >> 1) & 0x1) << 3;
      return 2;

   case PCO_MAIN_CIN_I3264:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xbf;
      bin[1] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_MAIN_S1NEG_I3264:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xdf;
      bin[1] |= (enc_val & 0x1) << 5;
      return 1;

   case PCO_MAIN_S0NEG_I3264:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xef;
      bin[1] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_MAIN_S0ABS_I3264:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfb;
      bin[1] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_MAIN_S1ABS_I3264:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfd;
      bin[1] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_S2ABS_I3264:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfe;
      bin[1] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_MOVW1:
      enc_val = val;
      assert(pco_movw01_valid(enc_val));
      bin[0] &= 0xf3;
      bin[0] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_MAIN_MOVW0:
      enc_val = val;
      assert(pco_movw01_valid(enc_val));
      bin[0] &= 0xfc;
      bin[0] |= (enc_val & 0x3);
      return 2;

   case PCO_MAIN_RSVD1_MOVC:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[1] &= 0x3f;
      bin[1] |= (enc_val & 0x3) << 6;
      return 2;

   case PCO_MAIN_MASKW0:
      enc_val = val;
      assert(pco_maskw0_valid(enc_val));
      bin[1] &= 0xc3;
      bin[1] |= (enc_val & 0xf) << 2;
      return 4;

   case PCO_MAIN_AW:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfd;
      bin[1] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_P2END_MOVC:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfe;
      bin[1] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_TST_OP_3BIT:
      enc_val = val;
      assert(pco_tst_op3_valid(enc_val));
      bin[0] &= 0xf1;
      bin[0] |= (enc_val & 0x7) << 1;
      return 3;

   case PCO_MAIN_PWEN_TST:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_MAIN_TST_TYPE:
      enc_val = val;
      assert(pco_tst_type_valid(enc_val));
      bin[1] &= 0x1f;
      bin[1] |= (enc_val & 0x7) << 5;
      return 3;

   case PCO_MAIN_P2END_TST:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xef;
      bin[1] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_MAIN_TST_ELEM:
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[1] &= 0xf3;
      bin[1] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_MAIN_RSVD1_TST:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xfd;
      bin[1] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_MAIN_TST_OP_4BIT:
      enc_val = val;
      assert(pco_tst_op_valid(enc_val));
      bin[0] &= 0xf1;
      bin[0] |= (enc_val & 0x7) << 1;
      bin[1] &= 0xfe;
      bin[1] |= ((enc_val >> 3) & 0x1);
      return 4;

   default:
      break;
   }

   UNREACHABLE("");
}

struct pco_main_fadd {
   bool sat : 1;
   bool s0neg : 1;
   bool s0abs : 1;
   bool s1abs : 1;
   bool s0flr : 1;
};

#define pco_main_fadd_encode(bin, ...) _pco_main_fadd_encode(bin, (const struct pco_main_fadd){0, ##__VA_ARGS__})
static unsigned _pco_main_fadd_encode(uint8_t *bin, const struct pco_main_fadd s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_FADD);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SAT_FAM, s.sat);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_FAM, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_FAM, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1ABS_FAM, s.s1abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0FLR_FAM, s.s0flr);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_fadd_lp {
   bool sat : 1;
   bool s0neg : 1;
   bool s0abs : 1;
   bool s1abs : 1;
   bool s0flr : 1;
};

#define pco_main_fadd_lp_encode(bin, ...) _pco_main_fadd_lp_encode(bin, (const struct pco_main_fadd_lp){0, ##__VA_ARGS__})
static unsigned _pco_main_fadd_lp_encode(uint8_t *bin, const struct pco_main_fadd_lp s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_FADD_LP);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SAT_FAM, s.sat);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_FAM, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_FAM, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1ABS_FAM, s.s1abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0FLR_FAM, s.s0flr);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_fmul {
   bool sat : 1;
   bool s0neg : 1;
   bool s0abs : 1;
   bool s1abs : 1;
   bool s0flr : 1;
};

#define pco_main_fmul_encode(bin, ...) _pco_main_fmul_encode(bin, (const struct pco_main_fmul){0, ##__VA_ARGS__})
static unsigned _pco_main_fmul_encode(uint8_t *bin, const struct pco_main_fmul s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_FMUL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SAT_FAM, s.sat);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_FAM, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_FAM, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1ABS_FAM, s.s1abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0FLR_FAM, s.s0flr);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_fmul_lp {
   bool sat : 1;
   bool s0neg : 1;
   bool s0abs : 1;
   bool s1abs : 1;
   bool s0flr : 1;
};

#define pco_main_fmul_lp_encode(bin, ...) _pco_main_fmul_lp_encode(bin, (const struct pco_main_fmul_lp){0, ##__VA_ARGS__})
static unsigned _pco_main_fmul_lp_encode(uint8_t *bin, const struct pco_main_fmul_lp s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_FMUL_LP);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SAT_FAM, s.sat);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_FAM, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_FAM, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1ABS_FAM, s.s1abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0FLR_FAM, s.s0flr);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_sngl {
   enum pco_sngl_op sngl_op : 4;
};

#define pco_main_sngl_encode(bin, ...) _pco_main_sngl_encode(bin, (const struct pco_main_sngl){0, ##__VA_ARGS__})
static unsigned _pco_main_sngl_encode(uint8_t *bin, const struct pco_main_sngl s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_SNGL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SNGL_OP, s.sngl_op);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_sngl_ext {
   enum pco_sngl_op sngl_op : 4;
   bool s0neg : 1;
   bool s0abs : 1;
};

#define pco_main_sngl_ext_encode(bin, ...) _pco_main_sngl_ext_encode(bin, (const struct pco_main_sngl_ext){0, ##__VA_ARGS__})
static unsigned _pco_main_sngl_ext_encode(uint8_t *bin, const struct pco_main_sngl_ext s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_SNGL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SNGL_OP, s.sngl_op);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_SNGL, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_SNGL_, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_SNGL, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_SNGL, s.s0abs);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_main_gcmp_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_SNGL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SNGL_OP, PCO_SNGL_OP_GAMMA);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_gcmp_ext {
   bool s0neg : 1;
   bool s0abs : 1;
};

#define pco_main_gcmp_ext_encode(bin, ...) _pco_main_gcmp_ext_encode(bin, (const struct pco_main_gcmp_ext){0, ##__VA_ARGS__})
static unsigned _pco_main_gcmp_ext_encode(uint8_t *bin, const struct pco_main_gcmp_ext s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_SNGL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SNGL_OP, PCO_SNGL_OP_GAMMA);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_SNGL, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_GAMMAOP, PCO_GAMMA_OP_CMP);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_SNGL, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_SNGL, s.s0abs);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_gexp {
   bool s0neg : 1;
   bool s0abs : 1;
};

#define pco_main_gexp_encode(bin, ...) _pco_main_gexp_encode(bin, (const struct pco_main_gexp){0, ##__VA_ARGS__})
static unsigned _pco_main_gexp_encode(uint8_t *bin, const struct pco_main_gexp s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_SNGL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SNGL_OP, PCO_SNGL_OP_GAMMA);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_SNGL, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_GAMMAOP, PCO_GAMMA_OP_EXP);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_SNGL, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_SNGL, s.s0abs);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_pck {
   bool prog : 1;
   bool rtz : 1;
   bool scale : 1;
   enum pco_pck_format pck_format : 5;
};

#define pco_main_pck_encode(bin, ...) _pco_main_pck_encode(bin, (const struct pco_main_pck){0, ##__VA_ARGS__})
static unsigned _pco_main_pck_encode(uint8_t *bin, const struct pco_main_pck s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_SNGL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SNGL_OP, PCO_SNGL_OP_PCK);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_PROG, s.prog);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RTZ, s.rtz);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SCALE, s.scale);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_PCK_FORMAT, s.pck_format);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_upck {
   enum pco_upck_elem elem : 2;
   bool scale_rtz : 1;
   enum pco_pck_format pck_format : 5;
};

#define pco_main_upck_encode(bin, ...) _pco_main_upck_encode(bin, (const struct pco_main_upck){0, ##__VA_ARGS__})
static unsigned _pco_main_upck_encode(uint8_t *bin, const struct pco_main_upck s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_SNGL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SNGL_OP, PCO_SNGL_OP_PCK);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_UPCK_ELEM, s.elem);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SCALE_RTZ, s.scale_rtz);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_PCK_FORMAT, s.pck_format);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_fred {
   enum pco_red_part red_part : 1;
   unsigned iter : 3;
   enum pco_red_type red_type : 1;
   bool pwen : 1;
   bool s0neg : 1;
   bool s0abs : 1;
};

#define pco_main_fred_encode(bin, ...) _pco_main_fred_encode(bin, (const struct pco_main_fred){0, ##__VA_ARGS__})
static unsigned _pco_main_fred_encode(uint8_t *bin, const struct pco_main_fred s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_SNGL);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SNGL_OP, PCO_SNGL_OP_RED);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RED_PART, s.red_part);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_ITER, s.iter);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RED_TYPE, s.red_type);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_PWEN_RED, s.pwen);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_SNGL, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_SNGL, s.s0abs);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_int8_16 {
   enum pco_int8_16_op int8_16_op : 2;
   bool s : 1;
   enum pco_int8_16_fmt f : 1;
};

#define pco_main_int8_16_encode(bin, ...) _pco_main_int8_16_encode(bin, (const struct pco_main_int8_16){0, ##__VA_ARGS__})
static unsigned _pco_main_int8_16_encode(uint8_t *bin, const struct pco_main_int8_16 s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_INT8_16);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_INT8_16_OP, s.int8_16_op);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S_I816, s.s);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_F_I816, s.f);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_int8_16_ext {
   enum pco_int8_16_op int8_16_op : 2;
   bool s : 1;
   enum pco_int8_16_fmt f : 1;
   enum pco_s2ch s2ch : 1;
   bool s2neg : 1;
   bool s2abs : 1;
   bool s1abs : 1;
   bool s0neg : 1;
   bool s0abs : 1;
   bool sat : 1;
};

#define pco_main_int8_16_ext_encode(bin, ...) _pco_main_int8_16_ext_encode(bin, (const struct pco_main_int8_16_ext){0, ##__VA_ARGS__})
static unsigned _pco_main_int8_16_ext_encode(uint8_t *bin, const struct pco_main_int8_16_ext s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_INT8_16);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_INT8_16_OP, s.int8_16_op);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S_I816, s.s);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_F_I816, s.f);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2CH, s.s2ch);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_I816, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2NEG_I816, s.s2neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2ABS_I816, s.s2abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1ABS_I816, s.s1abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_I816, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_I816, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SAT_I816, s.sat);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_int8_16_ext_sel {
   enum pco_int8_16_op int8_16_op : 2;
   bool s : 1;
   enum pco_int8_16_fmt f : 1;
   enum pco_s2ch s2ch : 1;
   bool s2neg : 1;
   bool s2abs : 1;
   bool s1abs : 1;
   bool s0neg : 1;
   bool s0abs : 1;
   bool sat : 1;
   enum pco_s01ch s1ch : 2;
   enum pco_s01ch s0ch : 2;
};

#define pco_main_int8_16_ext_sel_encode(bin, ...) _pco_main_int8_16_ext_sel_encode(bin, (const struct pco_main_int8_16_ext_sel){0, ##__VA_ARGS__})
static unsigned _pco_main_int8_16_ext_sel_encode(uint8_t *bin, const struct pco_main_int8_16_ext_sel s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_INT8_16);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_INT8_16_OP, s.int8_16_op);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S_I816, s.s);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_F_I816, s.f);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2CH, s.s2ch);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_I816, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2NEG_I816, s.s2neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2ABS_I816, s.s2abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1ABS_I816, s.s1abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_I816, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_I816, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SAT_I816, s.sat);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD2_I816, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1CH, s.s1ch);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0CH, s.s0ch);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_fmad {
   bool s0neg : 1;
   bool s0abs : 1;
   bool s2neg : 1;
   bool sat : 1;
};

#define pco_main_fmad_encode(bin, ...) _pco_main_fmad_encode(bin, (const struct pco_main_fmad){0, ##__VA_ARGS__})
static unsigned _pco_main_fmad_encode(uint8_t *bin, const struct pco_main_fmad s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_FMAD_MOVC);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_FMA, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_FMA, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2NEG_FMA, s.s2neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SAT_FMA, s.sat);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_fmad_ext {
   bool s0neg : 1;
   bool s0abs : 1;
   bool s2neg : 1;
   bool sat : 1;
   bool lp : 1;
   bool s1abs : 1;
   bool s1neg : 1;
   bool s2flr : 1;
   bool s2abs : 1;
};

#define pco_main_fmad_ext_encode(bin, ...) _pco_main_fmad_ext_encode(bin, (const struct pco_main_fmad_ext){0, ##__VA_ARGS__})
static unsigned _pco_main_fmad_ext_encode(uint8_t *bin, const struct pco_main_fmad_ext s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_FMAD_MOVC);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_FMA, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_FMA, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2NEG_FMA, s.s2neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_SAT_FMA, s.sat);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_FMA, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_LP_FMA, s.lp);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1ABS_FMA, s.s1abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1NEG_FMA, s.s1neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2FLR_FMA, s.s2flr);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2ABS_FMA, s.s2abs);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_int32_64 {
   bool s : 1;
   bool s2neg : 1;
   enum pco_int32_64_op int32_64_op : 2;
};

#define pco_main_int32_64_encode(bin, ...) _pco_main_int32_64_encode(bin, (const struct pco_main_int32_64){0, ##__VA_ARGS__})
static unsigned _pco_main_int32_64_encode(uint8_t *bin, const struct pco_main_int32_64 s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_INT32_64_TST);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S_I3264, s.s);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2NEG_I3264, s.s2neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_INT32_64_OP, s.int32_64_op);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_int32_64_ext {
   bool s : 1;
   bool s2neg : 1;
   enum pco_int32_64_op int32_64_op : 2;
   bool cin : 1;
   bool s1neg : 1;
   bool s0neg : 1;
   bool s0abs : 1;
   bool s1abs : 1;
   bool s2abs : 1;
};

#define pco_main_int32_64_ext_encode(bin, ...) _pco_main_int32_64_ext_encode(bin, (const struct pco_main_int32_64_ext){0, ##__VA_ARGS__})
static unsigned _pco_main_int32_64_ext_encode(uint8_t *bin, const struct pco_main_int32_64_ext s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_INT32_64_TST);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S_I3264, s.s);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2NEG_I3264, s.s2neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_INT32_64_OP, s.int32_64_op);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_I3264, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_CIN_I3264, s.cin);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1NEG_I3264, s.s1neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0NEG_I3264, s.s0neg);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S0ABS_I3264, s.s0abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S1ABS_I3264, s.s1abs);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_S2ABS_I3264, s.s2abs);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_movc {
   enum pco_movw01 movw1 : 2;
   enum pco_movw01 movw0 : 2;
};

#define pco_main_movc_encode(bin, ...) _pco_main_movc_encode(bin, (const struct pco_main_movc){0, ##__VA_ARGS__})
static unsigned _pco_main_movc_encode(uint8_t *bin, const struct pco_main_movc s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_FMAD_MOVC);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MOVW1, s.movw1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MOVW0, s.movw0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_movc_ext {
   enum pco_movw01 movw1 : 2;
   enum pco_movw01 movw0 : 2;
   enum pco_maskw0 maskw0 : 4;
   bool aw : 1;
   bool p2end : 1;
};

#define pco_main_movc_ext_encode(bin, ...) _pco_main_movc_ext_encode(bin, (const struct pco_main_movc_ext){0, ##__VA_ARGS__})
static unsigned _pco_main_movc_ext_encode(uint8_t *bin, const struct pco_main_movc_ext s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_FMAD_MOVC);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MOVW1, s.movw1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MOVW0, s.movw0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_MOVC, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MASKW0, s.maskw0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_AW, s.aw);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_P2END_MOVC, s.p2end);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_tst {
   enum pco_tst_op tst_op : 4;
   bool pwen : 1;
};

#define pco_main_tst_encode(bin, ...) _pco_main_tst_encode(bin, (const struct pco_main_tst){0, ##__VA_ARGS__})
static unsigned _pco_main_tst_encode(uint8_t *bin, const struct pco_main_tst s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_INT32_64_TST);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 0);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_TST_OP_3BIT, s.tst_op);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_PWEN_TST, s.pwen);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_main_tst_ext {
   enum pco_tst_op tst_op : 4;
   bool pwen : 1;
   enum pco_tst_type type : 3;
   bool p2end : 1;
   unsigned elem : 2;
};

#define pco_main_tst_ext_encode(bin, ...) _pco_main_tst_ext_encode(bin, (const struct pco_main_tst_ext){0, ##__VA_ARGS__})
static unsigned _pco_main_tst_ext_encode(uint8_t *bin, const struct pco_main_tst_ext s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_MAIN_OP, PCO_MAIN_OP_INT32_64_TST);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_EXT0, 1);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_TST_OP_4BIT, s.tst_op);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_PWEN_TST, s.pwen);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_TST_TYPE, s.type);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_P2END_TST, s.p2end);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_TST_ELEM, s.elem);
   bits_encoded += pco_main_encode_field(bin, PCO_MAIN_RSVD1_TST, 0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

/** backend */
enum pco_backend_field {
   PCO_BACKEND_BACKEND_OP,
   PCO_BACKEND_DSEL,
   PCO_BACKEND_IMM_UVSW,
   PCO_BACKEND_UVSW_OP,
   PCO_BACKEND_RSVD1,
   PCO_BACKEND_SRCSEL,
   PCO_BACKEND_IMM_ADDR_UVSW,
   PCO_BACKEND_RSVD1_UVSW_,
   PCO_BACKEND_STREAM_ID,
   PCO_BACKEND_MSK_OP,
   PCO_BACKEND_SM,
   PCO_BACKEND_MSK_MODE,
   PCO_BACKEND_EXEADDRSRC,
   PCO_BACKEND_PHAS_END,
   PCO_BACKEND_PHAS_TYPE,
   PCO_BACKEND_PHAS_RATE,
   PCO_BACKEND_COMMONTMP,
   PCO_BACKEND_RSVD0_SETL,
   PCO_BACKEND_RESSEL,
   PCO_BACKEND_RSVD0_VISTEST,
   PCO_BACKEND_PWEN_VISTEST,
   PCO_BACKEND_VISTEST_OP,
   PCO_BACKEND_IFB,
   PCO_BACKEND_P_FITR,
   PCO_BACKEND_DRC,
   PCO_BACKEND_RSVD0_FITR,
   PCO_BACKEND_ITER_MODE,
   PCO_BACKEND_RSVD1_FITR,
   PCO_BACKEND_SAT_FITR,
   PCO_BACKEND_COUNT_FITR,
   PCO_BACKEND_RSVD0_EMITPIX,
   PCO_BACKEND_FREEP,
   PCO_BACKEND_RSVD0_EMITPIX_,
   PCO_BACKEND_RSVD0_DMA,
   PCO_BACKEND_DMA_OP,
   PCO_BACKEND_IMMBL,
   PCO_BACKEND_SRCSELADD_LDST,
   PCO_BACKEND_SRCSELBL_LDST,
   PCO_BACKEND_CACHEMODE_LD,
   PCO_BACKEND_CACHEMODE_ST,
   PCO_BACKEND_RSVD2_LD,
   PCO_BACKEND_RSVD2_ST,
   PCO_BACKEND_TILED,
   PCO_BACKEND_SRCSELDATA,
   PCO_BACKEND_DSIZE,
   PCO_BACKEND_BURSTLEN,
   PCO_BACKEND_RSVD2_ST_,
   PCO_BACKEND_RSVD3_ST,
   PCO_BACKEND_SRCMASK,
   PCO_BACKEND_FCNORM,
   PCO_BACKEND_EXTB,
   PCO_BACKEND_DMN,
   PCO_BACKEND_EXTA,
   PCO_BACKEND_CHAN,
   PCO_BACKEND_LODM,
   PCO_BACKEND_PPLOD,
   PCO_BACKEND_PROJ,
   PCO_BACKEND_SBMODE,
   PCO_BACKEND_NNCOORDS,
   PCO_BACKEND_SNO,
   PCO_BACKEND_SOO,
   PCO_BACKEND_TAO,
   PCO_BACKEND_EXTC,
   PCO_BACKEND_RSVD3_SMP,
   PCO_BACKEND_F16,
   PCO_BACKEND_SWAP,
   PCO_BACKEND_CACHEMODE_SMP_LD_ST,
   PCO_BACKEND_SMP_W,
   PCO_BACKEND_RSVD4_SMP,
   PCO_BACKEND_DIRECT,
   PCO_BACKEND_F16INPUT,
   PCO_BACKEND_COMPARISON,
   PCO_BACKEND_INTEGER,
   PCO_BACKEND_ARRAY,
   PCO_BACKEND_ATOMIC_OP,
   PCO_BACKEND_RSVD1_ATOMIC,
   PCO_BACKEND_RSVD2_ATOMIC,
   PCO_BACKEND_DSTSEL,
};

static unsigned pco_backend_bytes(enum pco_backend_variant variant)
{
   switch (variant) {
   case PCO_BACKEND_NONE: return 0;
   case PCO_BACKEND_UVSW_WRITE_REG: return 2;
   case PCO_BACKEND_UVSW_WRITE_IMM: return 2;
   case PCO_BACKEND_UVSW_EMIT: return 1;
   case PCO_BACKEND_UVSW_EMIT_IMM: return 2;
   case PCO_BACKEND_UVSW_CUT: return 1;
   case PCO_BACKEND_UVSW_CUT_IMM: return 2;
   case PCO_BACKEND_UVSW_EMIT_CUT: return 1;
   case PCO_BACKEND_UVSW_EMIT_CUT_IMM: return 2;
   case PCO_BACKEND_UVSW_ENDTASK: return 1;
   case PCO_BACKEND_UVSW_EMIT_ENDTASK: return 1;
   case PCO_BACKEND_UVSW_WRITE_EMIT_ENDTASK_REG: return 2;
   case PCO_BACKEND_UVSW_WRITE_EMIT_ENDTASK_IMM: return 2;
   case PCO_BACKEND_MOVMSK: return 1;
   case PCO_BACKEND_MOVMSK_SM: return 2;
   case PCO_BACKEND_SAVMSK: return 1;
   case PCO_BACKEND_PHAS_REG: return 2;
   case PCO_BACKEND_PHAS_IMM: return 2;
   case PCO_BACKEND_SETL: return 1;
   case PCO_BACKEND_VISTEST_DEPTHF: return 1;
   case PCO_BACKEND_VISTEST_ATST: return 1;
   case PCO_BACKEND_FITR: return 2;
   case PCO_BACKEND_EMITPIX: return 1;
   case PCO_BACKEND_IDF: return 2;
   case PCO_BACKEND_LD_IMMBL: return 3;
   case PCO_BACKEND_LD_REGBL: return 2;
   case PCO_BACKEND_ST_IMMBL: return 3;
   case PCO_BACKEND_ST_REGBL: return 3;
   case PCO_BACKEND_ST_IMMBL_TILED: return 4;
   case PCO_BACKEND_ST_REGBL_TILED: return 4;
   case PCO_BACKEND_SMP_BRIEF: return 2;
   case PCO_BACKEND_SMP_EXTA: return 3;
   case PCO_BACKEND_SMP_EXTB: return 3;
   case PCO_BACKEND_SMP_EXTAB: return 4;
   case PCO_BACKEND_SMP_EXTABC: return 5;
   case PCO_BACKEND_ATOMIC: return 3;
   default: break;
   }

   UNREACHABLE("");
}

static unsigned pco_backend_encode_field(uint8_t *bin, enum pco_backend_field field, uint64_t val)
{
   uint64_t enc_val;

   switch (field) {
   case PCO_BACKEND_BACKEND_OP:
      enc_val = val;
      assert(pco_backend_op_valid(enc_val));
      bin[0] &= 0x1f;
      bin[0] |= (enc_val & 0x7) << 5;
      return 3;

   case PCO_BACKEND_DSEL:
      enc_val = val;
      assert(pco_dsel_valid(enc_val));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_IMM_UVSW:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xf7;
      bin[0] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BACKEND_UVSW_OP:
      enc_val = val;
      assert(pco_uvsw_op_valid(enc_val));
      bin[0] &= 0xf8;
      bin[0] |= (enc_val & 0x7);
      return 3;

   case PCO_BACKEND_RSVD1:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 5));
      bin[1] &= 0x7;
      bin[1] |= (enc_val & 0x1f) << 3;
      return 5;

   case PCO_BACKEND_SRCSEL:
      enc_val = val;
      assert(pco_srcsel_valid(enc_val));
      bin[1] &= 0xf8;
      bin[1] |= (enc_val & 0x7);
      return 3;

   case PCO_BACKEND_IMM_ADDR_UVSW:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[1] &= 0x0;
      bin[1] |= (enc_val & 0xff);
      return 8;

   case PCO_BACKEND_RSVD1_UVSW_:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 6));
      bin[1] &= 0x3;
      bin[1] |= (enc_val & 0x3f) << 2;
      return 6;

   case PCO_BACKEND_STREAM_ID:
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[1] &= 0xfc;
      bin[1] |= (enc_val & 0x3);
      return 2;

   case PCO_BACKEND_MSK_OP:
      enc_val = val;
      assert(pco_msk_op_valid(enc_val));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_SM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xf7;
      bin[0] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BACKEND_MSK_MODE:
      enc_val = val;
      assert(pco_msk_mode_valid(enc_val));
      bin[0] &= 0xf8;
      bin[0] |= (enc_val & 0x7);
      return 3;

   case PCO_BACKEND_EXEADDRSRC:
      enc_val = val;
      assert(pco_srcsel_valid(enc_val));
      bin[0] &= 0xe3;
      bin[0] |= (enc_val & 0x7) << 2;
      return 3;

   case PCO_BACKEND_PHAS_END:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfd;
      bin[0] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_BACKEND_PHAS_TYPE:
      enc_val = val;
      assert(pco_phas_type_valid(enc_val));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_BACKEND_PHAS_RATE:
      enc_val = val;
      assert(pco_phas_rate_valid(enc_val));
      bin[1] &= 0x3f;
      bin[1] |= (enc_val & 0x3) << 6;
      return 2;

   case PCO_BACKEND_COMMONTMP:
      assert(!(val & 0b11));
      enc_val = val >> 2;
      assert(enc_val < (1ULL << 6));
      bin[1] &= 0xc0;
      bin[1] |= (enc_val & 0x3f);
      return 6;

   case PCO_BACKEND_RSVD0_SETL:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 4));
      bin[0] &= 0xe1;
      bin[0] |= (enc_val & 0xf) << 1;
      return 4;

   case PCO_BACKEND_RESSEL:
      enc_val = val;
      assert(pco_dsel_valid(enc_val));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_BACKEND_RSVD0_VISTEST:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[0] &= 0xe7;
      bin[0] |= (enc_val & 0x3) << 3;
      return 2;

   case PCO_BACKEND_PWEN_VISTEST:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfb;
      bin[0] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_BACKEND_VISTEST_OP:
      enc_val = val;
      assert(pco_vistest_op_valid(enc_val));
      bin[0] &= 0xfd;
      bin[0] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_BACKEND_IFB:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_BACKEND_P_FITR:
      enc_val = val;
      assert(pco_persp_ctl1_valid(enc_val));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_DRC:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xf7;
      bin[0] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BACKEND_RSVD0_FITR:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfb;
      bin[0] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_BACKEND_ITER_MODE:
      enc_val = val;
      assert(pco_iter_mode_valid(enc_val));
      bin[0] &= 0xfc;
      bin[0] |= (enc_val & 0x3);
      return 2;

   case PCO_BACKEND_RSVD1_FITR:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 3));
      bin[1] &= 0x1f;
      bin[1] |= (enc_val & 0x7) << 5;
      return 3;

   case PCO_BACKEND_SAT_FITR:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xef;
      bin[1] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_COUNT_FITR:
      assert(val >= 1 && val <= 16);
      enc_val = val % 16;
      assert(enc_val < (1ULL << 4));
      bin[1] &= 0xf0;
      bin[1] |= (enc_val & 0xf);
      return 4;

   case PCO_BACKEND_RSVD0_EMITPIX:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 3));
      bin[0] &= 0xe3;
      bin[0] |= (enc_val & 0x7) << 2;
      return 3;

   case PCO_BACKEND_FREEP:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfd;
      bin[0] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_BACKEND_RSVD0_EMITPIX_:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_BACKEND_RSVD0_DMA:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_DMA_OP:
      enc_val = val;
      assert(pco_dma_op_valid(enc_val));
      bin[0] &= 0xf8;
      bin[0] |= (enc_val & 0x7);
      return 3;

   case PCO_BACKEND_IMMBL:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_SRCSELADD_LDST:
      enc_val = val;
      assert(pco_srcsel_valid(enc_val));
      bin[1] &= 0x1f;
      bin[1] |= (enc_val & 0x7) << 5;
      return 3;

   case PCO_BACKEND_SRCSELBL_LDST:
      enc_val = val;
      assert(pco_srcsel_valid(enc_val));
      bin[1] &= 0xe3;
      bin[1] |= (enc_val & 0x7) << 2;
      return 3;

   case PCO_BACKEND_CACHEMODE_LD:
      enc_val = val;
      assert(pco_cachemode_ld_valid(enc_val));
      bin[1] &= 0xfc;
      bin[1] |= (enc_val & 0x3);
      return 2;

   case PCO_BACKEND_CACHEMODE_ST:
      enc_val = val;
      assert(pco_cachemode_st_valid(enc_val));
      bin[1] &= 0xfc;
      bin[1] |= (enc_val & 0x3);
      return 2;

   case PCO_BACKEND_RSVD2_LD:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 7));
      bin[2] &= 0xfd;
      bin[2] |= (enc_val & 0x1) << 1;
      bin[2] &= 0xf3;
      bin[2] |= ((enc_val >> 1) & 0x3) << 2;
      bin[2] &= 0x8f;
      bin[2] |= ((enc_val >> 3) & 0x7) << 4;
      bin[2] &= 0x7f;
      bin[2] |= ((enc_val >> 6) & 0x1) << 7;
      return 7;

   case PCO_BACKEND_RSVD2_ST:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xfd;
      bin[2] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_BACKEND_TILED:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0x7f;
      bin[2] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_BACKEND_SRCSELDATA:
      enc_val = val;
      assert(pco_srcsel_valid(enc_val));
      bin[2] &= 0x8f;
      bin[2] |= (enc_val & 0x7) << 4;
      return 3;

   case PCO_BACKEND_DSIZE:
      enc_val = val;
      assert(pco_dsize_valid(enc_val));
      bin[2] &= 0xf3;
      bin[2] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_BACKEND_BURSTLEN:
      assert(val >= 1 && val <= 16);
      enc_val = val % 16;
      assert(enc_val < (1ULL << 4));
      bin[1] &= 0xe3;
      bin[1] |= (enc_val & 0x7) << 2;
      bin[2] &= 0xfe;
      bin[2] |= ((enc_val >> 3) & 0x1);
      return 4;

   case PCO_BACKEND_RSVD2_ST_:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xfe;
      bin[2] |= (enc_val & 0x1);
      return 1;

   case PCO_BACKEND_RSVD3_ST:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 5));
      bin[3] &= 0x7;
      bin[3] |= (enc_val & 0x1f) << 3;
      return 5;

   case PCO_BACKEND_SRCMASK:
      enc_val = val;
      assert(pco_srcsel_valid(enc_val));
      bin[3] &= 0xf8;
      bin[3] |= (enc_val & 0x7);
      return 3;

   case PCO_BACKEND_FCNORM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_EXTB:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0x7f;
      bin[1] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_BACKEND_DMN:
      enc_val = val;
      assert(pco_dmn_valid(enc_val));
      bin[1] &= 0x9f;
      bin[1] |= (enc_val & 0x3) << 5;
      return 2;

   case PCO_BACKEND_EXTA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xef;
      bin[1] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_CHAN:
      assert(val >= 1 && val <= 4);
      enc_val = val - 1;
      assert(enc_val < (1ULL << 2));
      bin[1] &= 0xf3;
      bin[1] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_BACKEND_LODM:
      enc_val = val;
      assert(pco_lodm_valid(enc_val));
      bin[1] &= 0xfc;
      bin[1] |= (enc_val & 0x3);
      return 2;

   case PCO_BACKEND_PPLOD:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0x7f;
      bin[2] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_BACKEND_PROJ:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xbf;
      bin[2] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_BACKEND_SBMODE:
      enc_val = val;
      assert(pco_sbmode_valid(enc_val));
      bin[2] &= 0xcf;
      bin[2] |= (enc_val & 0x3) << 4;
      return 2;

   case PCO_BACKEND_NNCOORDS:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xf7;
      bin[2] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BACKEND_SNO:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xfb;
      bin[2] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_BACKEND_SOO:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xfd;
      bin[2] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_BACKEND_TAO:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xfe;
      bin[2] |= (enc_val & 0x1);
      return 1;

   case PCO_BACKEND_EXTC:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[3] &= 0x7f;
      bin[3] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_BACKEND_RSVD3_SMP:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[3] &= 0x9f;
      bin[3] |= (enc_val & 0x3) << 5;
      return 2;

   case PCO_BACKEND_F16:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[3] &= 0xef;
      bin[3] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_SWAP:
      enc_val = val;
      assert(pco_sched_ctrl1_valid(enc_val));
      bin[3] &= 0xf7;
      bin[3] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BACKEND_CACHEMODE_SMP_LD_ST:
      enc_val = val;
      assert(pco_cachemode_ld_st_valid(enc_val));
      bin[3] &= 0xf9;
      bin[3] |= (enc_val & 0x3) << 1;
      return 2;

   case PCO_BACKEND_SMP_W:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[3] &= 0xfe;
      bin[3] |= (enc_val & 0x1);
      return 1;

   case PCO_BACKEND_RSVD4_SMP:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 3));
      bin[4] &= 0x1f;
      bin[4] |= (enc_val & 0x7) << 5;
      return 3;

   case PCO_BACKEND_DIRECT:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xef;
      bin[4] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BACKEND_F16INPUT:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xf7;
      bin[4] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BACKEND_COMPARISON:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xfb;
      bin[4] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_BACKEND_INTEGER:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xfd;
      bin[4] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_BACKEND_ARRAY:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xfe;
      bin[4] |= (enc_val & 0x1);
      return 1;

   case PCO_BACKEND_ATOMIC_OP:
      enc_val = val;
      assert(pco_atomic_op_valid(enc_val));
      bin[1] &= 0xf;
      bin[1] |= (enc_val & 0xf) << 4;
      return 4;

   case PCO_BACKEND_RSVD1_ATOMIC:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[1] &= 0xf7;
      bin[1] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BACKEND_RSVD2_ATOMIC:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 5));
      bin[2] &= 0x7;
      bin[2] |= (enc_val & 0x1f) << 3;
      return 5;

   case PCO_BACKEND_DSTSEL:
      enc_val = val;
      assert(pco_srcsel_valid(enc_val));
      bin[2] &= 0xf8;
      bin[2] |= (enc_val & 0x7);
      return 3;

   default:
      break;
   }

   UNREACHABLE("");
}

struct pco_backend_uvsw_write_reg {
   enum pco_dsel dsel : 1;
   enum pco_srcsel srcsel : 3;
};

#define pco_backend_uvsw_write_reg_encode(bin, ...) _pco_backend_uvsw_write_reg_encode(bin, (const struct pco_backend_uvsw_write_reg){0, ##__VA_ARGS__})
static unsigned _pco_backend_uvsw_write_reg_encode(uint8_t *bin, const struct pco_backend_uvsw_write_reg s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, s.dsel);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_WRITE);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSEL, s.srcsel);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_uvsw_write_imm {
   enum pco_dsel dsel : 1;
   unsigned imm_addr : 8;
};

#define pco_backend_uvsw_write_imm_encode(bin, ...) _pco_backend_uvsw_write_imm_encode(bin, (const struct pco_backend_uvsw_write_imm){0, ##__VA_ARGS__})
static unsigned _pco_backend_uvsw_write_imm_encode(uint8_t *bin, const struct pco_backend_uvsw_write_imm s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, s.dsel);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_WRITE);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_ADDR_UVSW, s.imm_addr);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_backend_uvsw_emit_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_EMIT);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_uvsw_emit_imm {
   unsigned stream_id : 2;
};

#define pco_backend_uvsw_emit_imm_encode(bin, ...) _pco_backend_uvsw_emit_imm_encode(bin, (const struct pco_backend_uvsw_emit_imm){0, ##__VA_ARGS__})
static unsigned _pco_backend_uvsw_emit_imm_encode(uint8_t *bin, const struct pco_backend_uvsw_emit_imm s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_EMIT);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1_UVSW_, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_STREAM_ID, s.stream_id);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_backend_uvsw_cut_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_CUT);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_uvsw_cut_imm {
   unsigned stream_id : 2;
};

#define pco_backend_uvsw_cut_imm_encode(bin, ...) _pco_backend_uvsw_cut_imm_encode(bin, (const struct pco_backend_uvsw_cut_imm){0, ##__VA_ARGS__})
static unsigned _pco_backend_uvsw_cut_imm_encode(uint8_t *bin, const struct pco_backend_uvsw_cut_imm s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_CUT);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1_UVSW_, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_STREAM_ID, s.stream_id);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_backend_uvsw_emit_cut_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_EMIT_CUT);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_uvsw_emit_cut_imm {
   unsigned stream_id : 2;
};

#define pco_backend_uvsw_emit_cut_imm_encode(bin, ...) _pco_backend_uvsw_emit_cut_imm_encode(bin, (const struct pco_backend_uvsw_emit_cut_imm){0, ##__VA_ARGS__})
static unsigned _pco_backend_uvsw_emit_cut_imm_encode(uint8_t *bin, const struct pco_backend_uvsw_emit_cut_imm s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_EMIT_CUT);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1_UVSW_, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_STREAM_ID, s.stream_id);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_backend_uvsw_endtask_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_ENDTASK);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_backend_uvsw_emit_endtask_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_EMIT_ENDTASK);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_uvsw_write_emit_endtask_reg {
   enum pco_dsel dsel : 1;
   enum pco_srcsel srcsel : 3;
};

#define pco_backend_uvsw_write_emit_endtask_reg_encode(bin, ...) _pco_backend_uvsw_write_emit_endtask_reg_encode(bin, (const struct pco_backend_uvsw_write_emit_endtask_reg){0, ##__VA_ARGS__})
static unsigned _pco_backend_uvsw_write_emit_endtask_reg_encode(uint8_t *bin, const struct pco_backend_uvsw_write_emit_endtask_reg s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, s.dsel);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_WRITE_EMIT_ENDTASK);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSEL, s.srcsel);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_uvsw_write_emit_endtask_imm {
   enum pco_dsel dsel : 1;
   unsigned imm_addr : 8;
};

#define pco_backend_uvsw_write_emit_endtask_imm_encode(bin, ...) _pco_backend_uvsw_write_emit_endtask_imm_encode(bin, (const struct pco_backend_uvsw_write_emit_endtask_imm){0, ##__VA_ARGS__})
static unsigned _pco_backend_uvsw_write_emit_endtask_imm_encode(uint8_t *bin, const struct pco_backend_uvsw_write_emit_endtask_imm s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_UVSW);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSEL, s.dsel);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_UVSW, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_UVSW_OP, PCO_UVSW_OP_WRITE_EMIT_ENDTASK);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMM_ADDR_UVSW, s.imm_addr);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_backend_movmsk_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_MSK);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_MSK_OP, PCO_MSK_OP_MOV);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SM, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_MSK_MODE, PCO_MSK_MODE_ICM);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_movmsk_sm {
   enum pco_srcsel srcsel : 3;
};

#define pco_backend_movmsk_sm_encode(bin, ...) _pco_backend_movmsk_sm_encode(bin, (const struct pco_backend_movmsk_sm){0, ##__VA_ARGS__})
static unsigned _pco_backend_movmsk_sm_encode(uint8_t *bin, const struct pco_backend_movmsk_sm s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_MSK);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_MSK_OP, PCO_MSK_OP_MOV);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SM, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_MSK_MODE, PCO_MSK_MODE_ICM);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSEL, s.srcsel);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_savmsk {
   enum pco_msk_mode msk_mode : 3;
};

#define pco_backend_savmsk_encode(bin, ...) _pco_backend_savmsk_encode(bin, (const struct pco_backend_savmsk){0, ##__VA_ARGS__})
static unsigned _pco_backend_savmsk_encode(uint8_t *bin, const struct pco_backend_savmsk s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_MSK);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_MSK_OP, PCO_MSK_OP_SAV);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SM, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_MSK_MODE, s.msk_mode);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_phas_reg {
   enum pco_srcsel exeaddrsrc : 3;
   enum pco_srcsel paramsrc : 3;
};

#define pco_backend_phas_reg_encode(bin, ...) _pco_backend_phas_reg_encode(bin, (const struct pco_backend_phas_reg){0, ##__VA_ARGS__})
static unsigned _pco_backend_phas_reg_encode(uint8_t *bin, const struct pco_backend_phas_reg s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_PHAS);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXEADDRSRC, s.exeaddrsrc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PHAS_END, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PHAS_TYPE, PCO_PHAS_TYPE_REG);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSEL, s.paramsrc);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_phas_imm {
   enum pco_srcsel exeaddrsrc : 3;
   bool end : 1;
   enum pco_phas_rate rate : 2;
   unsigned commontmp : 8;
};

#define pco_backend_phas_imm_encode(bin, ...) _pco_backend_phas_imm_encode(bin, (const struct pco_backend_phas_imm){0, ##__VA_ARGS__})
static unsigned _pco_backend_phas_imm_encode(uint8_t *bin, const struct pco_backend_phas_imm s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_PHAS);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXEADDRSRC, s.exeaddrsrc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PHAS_END, s.end);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PHAS_TYPE, PCO_PHAS_TYPE_IMM);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PHAS_RATE, s.rate);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_COMMONTMP, s.commontmp);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_setl {
   enum pco_dsel ressel : 1;
};

#define pco_backend_setl_encode(bin, ...) _pco_backend_setl_encode(bin, (const struct pco_backend_setl){0, ##__VA_ARGS__})
static unsigned _pco_backend_setl_encode(uint8_t *bin, const struct pco_backend_setl s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_SETL);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD0_SETL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RESSEL, s.ressel);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_backend_vistest_depthf_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_VISTEST);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD0_VISTEST, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PWEN_VISTEST, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_VISTEST_OP, PCO_VISTEST_OP_DEPTHF);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IFB, 0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_vistest_atst {
   bool pwen : 1;
   bool ifb : 1;
};

#define pco_backend_vistest_atst_encode(bin, ...) _pco_backend_vistest_atst_encode(bin, (const struct pco_backend_vistest_atst){0, ##__VA_ARGS__})
static unsigned _pco_backend_vistest_atst_encode(uint8_t *bin, const struct pco_backend_vistest_atst s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_VISTEST);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD0_VISTEST, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PWEN_VISTEST, s.pwen);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_VISTEST_OP, PCO_VISTEST_OP_ATST);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IFB, s.ifb);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_fitr {
   enum pco_persp_ctl p : 2;
   unsigned drc : 1;
   enum pco_iter_mode iter_mode : 2;
   bool sat : 1;
   unsigned count : 5;
};

#define pco_backend_fitr_encode(bin, ...) _pco_backend_fitr_encode(bin, (const struct pco_backend_fitr){0, ##__VA_ARGS__})
static unsigned _pco_backend_fitr_encode(uint8_t *bin, const struct pco_backend_fitr s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_FITR);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_P_FITR, s.p);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD0_FITR, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_ITER_MODE, s.iter_mode);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1_FITR, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SAT_FITR, s.sat);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_COUNT_FITR, s.count);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_emitpix {
   bool freep : 1;
};

#define pco_backend_emitpix_encode(bin, ...) _pco_backend_emitpix_encode(bin, (const struct pco_backend_emitpix){0, ##__VA_ARGS__})
static unsigned _pco_backend_emitpix_encode(uint8_t *bin, const struct pco_backend_emitpix s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_EMIT);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD0_EMITPIX, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_FREEP, s.freep);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD0_EMITPIX_, 0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_idf {
   unsigned drc : 1;
   enum pco_srcsel srcseladd : 3;
};

#define pco_backend_idf_encode(bin, ...) _pco_backend_idf_encode(bin, (const struct pco_backend_idf){0, ##__VA_ARGS__})
static unsigned _pco_backend_idf_encode(uint8_t *bin, const struct pco_backend_idf s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD0_DMA, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_IDF);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSEL, s.srcseladd);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_ld_immbl {
   unsigned drc : 1;
   enum pco_srcsel srcseladd : 3;
   unsigned burstlen : 5;
   enum pco_cachemode_ld cachemode_ld : 2;
};

#define pco_backend_ld_immbl_encode(bin, ...) _pco_backend_ld_immbl_encode(bin, (const struct pco_backend_ld_immbl){0, ##__VA_ARGS__})
static unsigned _pco_backend_ld_immbl_encode(uint8_t *bin, const struct pco_backend_ld_immbl s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMMBL, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_LD);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELADD_LDST, s.srcseladd);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BURSTLEN, s.burstlen);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_LD, s.cachemode_ld);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD2_LD, 0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_ld_regbl {
   unsigned drc : 1;
   enum pco_srcsel srcseladd : 3;
   enum pco_srcsel srcselbl : 3;
   enum pco_cachemode_ld cachemode_ld : 2;
};

#define pco_backend_ld_regbl_encode(bin, ...) _pco_backend_ld_regbl_encode(bin, (const struct pco_backend_ld_regbl){0, ##__VA_ARGS__})
static unsigned _pco_backend_ld_regbl_encode(uint8_t *bin, const struct pco_backend_ld_regbl s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMMBL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_LD);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELADD_LDST, s.srcseladd);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELBL_LDST, s.srcselbl);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_LD, s.cachemode_ld);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_st_immbl {
   unsigned drc : 1;
   enum pco_srcsel srcseladd : 3;
   unsigned burstlen : 5;
   enum pco_cachemode_st cachemode_st : 2;
   enum pco_srcsel srcseldata : 3;
   enum pco_dsize dsize : 2;
};

#define pco_backend_st_immbl_encode(bin, ...) _pco_backend_st_immbl_encode(bin, (const struct pco_backend_st_immbl){0, ##__VA_ARGS__})
static unsigned _pco_backend_st_immbl_encode(uint8_t *bin, const struct pco_backend_st_immbl s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMMBL, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_ST);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELADD_LDST, s.srcseladd);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BURSTLEN, s.burstlen);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_ST, s.cachemode_st);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_TILED, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELDATA, s.srcseldata);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSIZE, s.dsize);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD2_ST, 0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_st_regbl {
   unsigned drc : 1;
   enum pco_srcsel srcseladd : 3;
   enum pco_srcsel srcselbl : 3;
   enum pco_cachemode_st cachemode_st : 2;
   enum pco_srcsel srcseldata : 3;
   enum pco_dsize dsize : 2;
};

#define pco_backend_st_regbl_encode(bin, ...) _pco_backend_st_regbl_encode(bin, (const struct pco_backend_st_regbl){0, ##__VA_ARGS__})
static unsigned _pco_backend_st_regbl_encode(uint8_t *bin, const struct pco_backend_st_regbl s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMMBL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_ST);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELADD_LDST, s.srcseladd);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELBL_LDST, s.srcselbl);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_ST, s.cachemode_st);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_TILED, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELDATA, s.srcseldata);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSIZE, s.dsize);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD2_ST, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD2_ST_, 0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_st_immbl_tiled {
   unsigned drc : 1;
   enum pco_srcsel srcseladd : 3;
   unsigned burstlen : 5;
   enum pco_cachemode_st cachemode_st : 2;
   enum pco_srcsel srcseldata : 3;
   enum pco_dsize dsize : 2;
   enum pco_srcsel srcmask : 3;
};

#define pco_backend_st_immbl_tiled_encode(bin, ...) _pco_backend_st_immbl_tiled_encode(bin, (const struct pco_backend_st_immbl_tiled){0, ##__VA_ARGS__})
static unsigned _pco_backend_st_immbl_tiled_encode(uint8_t *bin, const struct pco_backend_st_immbl_tiled s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMMBL, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_ST);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELADD_LDST, s.srcseladd);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BURSTLEN, s.burstlen);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_ST, s.cachemode_st);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_TILED, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELDATA, s.srcseldata);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSIZE, s.dsize);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD2_ST, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD3_ST, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCMASK, s.srcmask);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_st_regbl_tiled {
   unsigned drc : 1;
   enum pco_srcsel srcseladd : 3;
   enum pco_srcsel srcselbl : 3;
   enum pco_cachemode_st cachemode_st : 2;
   enum pco_srcsel srcseldata : 3;
   enum pco_dsize dsize : 2;
   enum pco_srcsel srcmask : 3;
};

#define pco_backend_st_regbl_tiled_encode(bin, ...) _pco_backend_st_regbl_tiled_encode(bin, (const struct pco_backend_st_regbl_tiled){0, ##__VA_ARGS__})
static unsigned _pco_backend_st_regbl_tiled_encode(uint8_t *bin, const struct pco_backend_st_regbl_tiled s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_IMMBL, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_ST);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELADD_LDST, s.srcseladd);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELBL_LDST, s.srcselbl);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_ST, s.cachemode_st);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_TILED, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSELDATA, s.srcseldata);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSIZE, s.dsize);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD2_ST, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD2_ST_, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD3_ST, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCMASK, s.srcmask);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_smp_brief {
   bool fcnorm : 1;
   unsigned drc : 1;
   enum pco_dmn dmn : 2;
   unsigned chan : 3;
   enum pco_lodm lodm : 2;
};

#define pco_backend_smp_brief_encode(bin, ...) _pco_backend_smp_brief_encode(bin, (const struct pco_backend_smp_brief){0, ##__VA_ARGS__})
static unsigned _pco_backend_smp_brief_encode(uint8_t *bin, const struct pco_backend_smp_brief s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_FCNORM, s.fcnorm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_SMP);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTB, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMN, s.dmn);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTA, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CHAN, s.chan);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_LODM, s.lodm);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_smp_exta {
   bool fcnorm : 1;
   unsigned drc : 1;
   enum pco_dmn dmn : 2;
   unsigned chan : 3;
   enum pco_lodm lodm : 2;
   bool pplod : 1;
   bool proj : 1;
   enum pco_sbmode sbmode : 2;
   bool nncoords : 1;
   bool sno : 1;
   bool soo : 1;
   bool tao : 1;
};

#define pco_backend_smp_exta_encode(bin, ...) _pco_backend_smp_exta_encode(bin, (const struct pco_backend_smp_exta){0, ##__VA_ARGS__})
static unsigned _pco_backend_smp_exta_encode(uint8_t *bin, const struct pco_backend_smp_exta s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_FCNORM, s.fcnorm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_SMP);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTB, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMN, s.dmn);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTA, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CHAN, s.chan);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_LODM, s.lodm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PPLOD, s.pplod);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PROJ, s.proj);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SBMODE, s.sbmode);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_NNCOORDS, s.nncoords);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SNO, s.sno);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SOO, s.soo);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_TAO, s.tao);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_smp_extb {
   bool fcnorm : 1;
   unsigned drc : 1;
   enum pco_dmn dmn : 2;
   unsigned chan : 3;
   enum pco_lodm lodm : 2;
   bool f16 : 1;
   enum pco_sched_ctrl swap : 2;
   enum pco_cachemode_ld_st cachemode_ld_st : 2;
   bool w : 1;
};

#define pco_backend_smp_extb_encode(bin, ...) _pco_backend_smp_extb_encode(bin, (const struct pco_backend_smp_extb){0, ##__VA_ARGS__})
static unsigned _pco_backend_smp_extb_encode(uint8_t *bin, const struct pco_backend_smp_extb s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_FCNORM, s.fcnorm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_SMP);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTB, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMN, s.dmn);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTA, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CHAN, s.chan);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_LODM, s.lodm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTC, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD3_SMP, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_F16, s.f16);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SWAP, s.swap);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_SMP_LD_ST, s.cachemode_ld_st);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SMP_W, s.w);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_smp_extab {
   bool fcnorm : 1;
   unsigned drc : 1;
   enum pco_dmn dmn : 2;
   unsigned chan : 3;
   enum pco_lodm lodm : 2;
   bool pplod : 1;
   bool proj : 1;
   enum pco_sbmode sbmode : 2;
   bool nncoords : 1;
   bool sno : 1;
   bool soo : 1;
   bool tao : 1;
   bool f16 : 1;
   enum pco_sched_ctrl swap : 2;
   enum pco_cachemode_ld_st cachemode_ld_st : 2;
   bool w : 1;
};

#define pco_backend_smp_extab_encode(bin, ...) _pco_backend_smp_extab_encode(bin, (const struct pco_backend_smp_extab){0, ##__VA_ARGS__})
static unsigned _pco_backend_smp_extab_encode(uint8_t *bin, const struct pco_backend_smp_extab s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_FCNORM, s.fcnorm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_SMP);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTB, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMN, s.dmn);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTA, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CHAN, s.chan);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_LODM, s.lodm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PPLOD, s.pplod);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PROJ, s.proj);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SBMODE, s.sbmode);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_NNCOORDS, s.nncoords);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SNO, s.sno);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SOO, s.soo);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_TAO, s.tao);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTC, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD3_SMP, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_F16, s.f16);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SWAP, s.swap);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_SMP_LD_ST, s.cachemode_ld_st);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SMP_W, s.w);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_smp_extabc {
   bool fcnorm : 1;
   unsigned drc : 1;
   enum pco_dmn dmn : 2;
   unsigned chan : 3;
   enum pco_lodm lodm : 2;
   bool pplod : 1;
   bool proj : 1;
   enum pco_sbmode sbmode : 2;
   bool nncoords : 1;
   bool sno : 1;
   bool soo : 1;
   bool tao : 1;
   bool f16 : 1;
   enum pco_sched_ctrl swap : 2;
   enum pco_cachemode_ld_st cachemode_ld_st : 2;
   bool w : 1;
   bool integer : 1;
   bool array : 1;
};

#define pco_backend_smp_extabc_encode(bin, ...) _pco_backend_smp_extabc_encode(bin, (const struct pco_backend_smp_extabc){0, ##__VA_ARGS__})
static unsigned _pco_backend_smp_extabc_encode(uint8_t *bin, const struct pco_backend_smp_extabc s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_FCNORM, s.fcnorm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_SMP);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTB, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMN, s.dmn);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTA, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CHAN, s.chan);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_LODM, s.lodm);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PPLOD, s.pplod);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_PROJ, s.proj);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SBMODE, s.sbmode);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_NNCOORDS, s.nncoords);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SNO, s.sno);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SOO, s.soo);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_TAO, s.tao);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_EXTC, 1);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD3_SMP, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_F16, s.f16);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SWAP, s.swap);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_CACHEMODE_SMP_LD_ST, s.cachemode_ld_st);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SMP_W, s.w);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD4_SMP, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DIRECT, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_F16INPUT, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_COMPARISON, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_INTEGER, s.integer);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_ARRAY, s.array);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_backend_atomic {
   unsigned drc : 1;
   enum pco_atomic_op atomic_op : 4;
   enum pco_srcsel srcsel : 3;
   enum pco_srcsel dstsel : 3;
};

#define pco_backend_atomic_encode(bin, ...) _pco_backend_atomic_encode(bin, (const struct pco_backend_atomic){0, ##__VA_ARGS__})
static unsigned _pco_backend_atomic_encode(uint8_t *bin, const struct pco_backend_atomic s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_BACKEND_OP, PCO_BACKEND_OP_DMA);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD0_DMA, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DRC, s.drc);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DMA_OP, PCO_DMA_OP_ATOMIC);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_ATOMIC_OP, s.atomic_op);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD1_ATOMIC, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_SRCSEL, s.srcsel);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_RSVD2_ATOMIC, 0);
   bits_encoded += pco_backend_encode_field(bin, PCO_BACKEND_DSTSEL, s.dstsel);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

/** bitwise */
enum pco_bitwise_field {
   PCO_BITWISE_PH0,
   PCO_BITWISE_PH1,
   PCO_BITWISE_COUNT_SRC,
   PCO_BITWISE_BITMASK_IMM,
   PCO_BITWISE_COUNT_OP,
   PCO_BITWISE_BITMASK_SRC_OP,
   PCO_BITWISE_BITMASK_IMM_OP,
   PCO_BITWISE_SHIFT1_OP,
   PCO_BITWISE_IMM16,
   PCO_BITWISE_IMM32,
   PCO_BITWISE_MSKB,
   PCO_BITWISE_RSVD0_PH1,
   PCO_BITWISE_MSKA,
   PCO_BITWISE_LOGICAL_OP,
   PCO_BITWISE_PWEN,
   PCO_BITWISE_TST_SRC,
   PCO_BITWISE_TST_OP,
   PCO_BITWISE_SHIFT2_OP,
};

static unsigned pco_bitwise_bytes(enum pco_bitwise_variant variant)
{
   switch (variant) {
   case PCO_BITWISE_NONE: return 0;
   case PCO_BITWISE_PHASE0_SRC: return 1;
   case PCO_BITWISE_PHASE0_IMM16: return 3;
   case PCO_BITWISE_PHASE0_IMM32: return 5;
   case PCO_BITWISE_PHASE1: return 1;
   case PCO_BITWISE_PHASE2: return 1;
   default: break;
   }

   UNREACHABLE("");
}

static unsigned pco_bitwise_encode_field(uint8_t *bin, enum pco_bitwise_field field, uint64_t val)
{
   uint64_t enc_val;

   switch (field) {
   case PCO_BITWISE_PH0:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0x7f;
      bin[0] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_BITWISE_PH1:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_BITWISE_COUNT_SRC:
      enc_val = val;
      assert(pco_count_src_valid(enc_val));
      bin[0] &= 0xbf;
      bin[0] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_BITWISE_BITMASK_IMM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BITWISE_COUNT_OP:
      enc_val = val;
      assert(pco_count_op_valid(enc_val));
      bin[0] &= 0xdf;
      bin[0] |= (enc_val & 0x1) << 5;
      bin[0] &= 0xfd;
      bin[0] |= ((enc_val >> 1) & 0x1) << 1;
      return 2;

   case PCO_BITWISE_BITMASK_SRC_OP:
      enc_val = val;
      assert(pco_bitmask_src_op_valid(enc_val));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_BITWISE_BITMASK_IMM_OP:
      enc_val = val;
      assert(pco_bitmask_imm_op_valid(enc_val));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_BITWISE_SHIFT1_OP:
      enc_val = val;
      assert(pco_shift1_op_valid(enc_val));
      bin[0] &= 0xf3;
      bin[0] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_BITWISE_IMM16:
      enc_val = val;
      assert(enc_val < (1ULL << 16));
      bin[1] &= 0x0;
      bin[1] |= (enc_val & 0xff);
      bin[2] &= 0x0;
      bin[2] |= ((enc_val >> 8) & 0xff);
      return 16;

   case PCO_BITWISE_IMM32:
      enc_val = val;
      assert(enc_val < (1ULL << 32));
      bin[1] &= 0x0;
      bin[1] |= (enc_val & 0xff);
      bin[2] &= 0x0;
      bin[2] |= ((enc_val >> 8) & 0xff);
      bin[3] &= 0x0;
      bin[3] |= ((enc_val >> 16) & 0xff);
      bin[4] &= 0x0;
      bin[4] |= ((enc_val >> 24) & 0xff);
      return 32;

   case PCO_BITWISE_MSKB:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xdf;
      bin[0] |= (enc_val & 0x1) << 5;
      return 1;

   case PCO_BITWISE_RSVD0_PH1:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BITWISE_MSKA:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xf7;
      bin[0] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BITWISE_LOGICAL_OP:
      enc_val = val;
      assert(pco_logical_op_valid(enc_val));
      bin[0] &= 0xf8;
      bin[0] |= (enc_val & 0x7);
      return 3;

   case PCO_BITWISE_PWEN:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xdf;
      bin[0] |= (enc_val & 0x1) << 5;
      return 1;

   case PCO_BITWISE_TST_SRC:
      enc_val = val;
      assert(pco_bw_tst_src_valid(enc_val));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_BITWISE_TST_OP:
      enc_val = val;
      assert(pco_bw_tst_op_valid(enc_val));
      bin[0] &= 0xf7;
      bin[0] |= (enc_val & 0x1) << 3;
      return 1;

   case PCO_BITWISE_SHIFT2_OP:
      enc_val = val;
      assert(pco_shift2_op_valid(enc_val));
      bin[0] &= 0xf8;
      bin[0] |= (enc_val & 0x7);
      return 3;

   default:
      break;
   }

   UNREACHABLE("");
}

struct pco_bitwise_phase0_src {
   enum pco_count_src count_src : 1;
   enum pco_count_op count_op : 2;
   enum pco_bitmask_src_op bitmask_src_op : 1;
   enum pco_shift1_op shift1_op : 2;
};

#define pco_bitwise_phase0_src_encode(bin, ...) _pco_bitwise_phase0_src_encode(bin, (const struct pco_bitwise_phase0_src){0, ##__VA_ARGS__})
static unsigned _pco_bitwise_phase0_src_encode(uint8_t *bin, const struct pco_bitwise_phase0_src s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_PH0, 0);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_COUNT_SRC, s.count_src);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_BITMASK_IMM, 0);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_COUNT_OP, s.count_op);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_BITMASK_SRC_OP, s.bitmask_src_op);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_SHIFT1_OP, s.shift1_op);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_bitwise_phase0_imm16 {
   enum pco_count_src count_src : 1;
   enum pco_count_op count_op : 2;
   enum pco_shift1_op shift1_op : 2;
   unsigned imm16 : 16;
};

#define pco_bitwise_phase0_imm16_encode(bin, ...) _pco_bitwise_phase0_imm16_encode(bin, (const struct pco_bitwise_phase0_imm16){0, ##__VA_ARGS__})
static unsigned _pco_bitwise_phase0_imm16_encode(uint8_t *bin, const struct pco_bitwise_phase0_imm16 s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_PH0, 0);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_COUNT_SRC, s.count_src);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_BITMASK_IMM, 1);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_COUNT_OP, s.count_op);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_BITMASK_IMM_OP, PCO_BITMASK_IMM_OP_BYP16);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_SHIFT1_OP, s.shift1_op);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_IMM16, s.imm16);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_bitwise_phase0_imm32 {
   enum pco_count_src count_src : 1;
   enum pco_count_op count_op : 2;
   enum pco_shift1_op shift1_op : 2;
   unsigned imm32 : 32;
};

#define pco_bitwise_phase0_imm32_encode(bin, ...) _pco_bitwise_phase0_imm32_encode(bin, (const struct pco_bitwise_phase0_imm32){0, ##__VA_ARGS__})
static unsigned _pco_bitwise_phase0_imm32_encode(uint8_t *bin, const struct pco_bitwise_phase0_imm32 s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_PH0, 0);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_COUNT_SRC, s.count_src);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_BITMASK_IMM, 1);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_COUNT_OP, s.count_op);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_BITMASK_IMM_OP, PCO_BITMASK_IMM_OP_BYP32);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_SHIFT1_OP, s.shift1_op);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_IMM32, s.imm32);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_bitwise_phase1 {
   bool mskb : 1;
   bool mska : 1;
   enum pco_logical_op logical_op : 3;
};

#define pco_bitwise_phase1_encode(bin, ...) _pco_bitwise_phase1_encode(bin, (const struct pco_bitwise_phase1){0, ##__VA_ARGS__})
static unsigned _pco_bitwise_phase1_encode(uint8_t *bin, const struct pco_bitwise_phase1 s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_PH0, 0);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_PH1, 1);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_MSKB, s.mskb);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_RSVD0_PH1, 0);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_MSKA, s.mska);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_LOGICAL_OP, s.logical_op);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_bitwise_phase2 {
   bool pwen : 1;
   enum pco_bw_tst_src tst_src : 1;
   enum pco_bw_tst_op bw_tst_op : 1;
   enum pco_shift2_op shift2_op : 3;
};

#define pco_bitwise_phase2_encode(bin, ...) _pco_bitwise_phase2_encode(bin, (const struct pco_bitwise_phase2){0, ##__VA_ARGS__})
static unsigned _pco_bitwise_phase2_encode(uint8_t *bin, const struct pco_bitwise_phase2 s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_PH0, 0);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_PH1, 0);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_PWEN, s.pwen);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_TST_SRC, s.tst_src);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_TST_OP, s.bw_tst_op);
   bits_encoded += pco_bitwise_encode_field(bin, PCO_BITWISE_SHIFT2_OP, s.shift2_op);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

/** ctrl */
enum pco_ctrl_field {
   PCO_CTRL_RSVD_BRANCH,
   PCO_CTRL_LINK,
   PCO_CTRL_BPRED,
   PCO_CTRL_ABS,
   PCO_CTRL_BRANCH_OFFSET,
   PCO_CTRL_RSVD_CND,
   PCO_CTRL_ADJUST,
   PCO_CTRL_PCND,
   PCO_CTRL_CNDINST,
   PCO_CTRL_LR,
   PCO_CTRL_RSVD_MUTEX,
   PCO_CTRL_ID,
   PCO_CTRL_RSVD_NOP,
   PCO_CTRL_RSVD_SBO,
   PCO_CTRL_TGT,
   PCO_CTRL_SBO_OFFSET,
   PCO_CTRL_DEST,
   PCO_CTRL_COFF,
   PCO_CTRL_P_ITR,
   PCO_CTRL_WOFF_B2,
   PCO_CTRL_DITR_MODE,
   PCO_CTRL_ITR_COUNT_B3,
   PCO_CTRL_COFF_IDX_CTRL_B3,
   PCO_CTRL_WOFF_IDX_CTRL_B3,
   PCO_CTRL_RSVD4_7_DITR,
   PCO_CTRL_F16_B4,
   PCO_CTRL_RSVD4_5_DITR,
   PCO_CTRL_SCHED_CTRL_B4,
   PCO_CTRL_DRC_B4,
   PCO_CTRL_SAT,
   PCO_CTRL_TEXOFF,
   PCO_CTRL_DRC_B2,
   PCO_CTRL_ITRSMP_MODE1,
   PCO_CTRL_SMPOFF,
   PCO_CTRL_DMN,
   PCO_CTRL_WOFF_B4,
   PCO_CTRL_PROJ,
   PCO_CTRL_EXT5,
   PCO_CTRL_F16_B5,
   PCO_CTRL_SCHED_CTRL_B5,
   PCO_CTRL_CHAN,
   PCO_CTRL_NNCOORDS,
   PCO_CTRL_FCNORM,
   PCO_CTRL_EXT6,
   PCO_CTRL_ITR_COUNT_B6,
   PCO_CTRL_COMPARISON,
   PCO_CTRL_RSVD6_ITRSMP,
   PCO_CTRL_RSVD7_ITRSMP,
   PCO_CTRL_COFF_IDX_CTRL_B7,
   PCO_CTRL_WOFF_IDX_CTRL_B7,
   PCO_CTRL_ITRSMP_MODE,
};

static unsigned pco_ctrl_bytes(enum pco_ctrl_variant variant)
{
   switch (variant) {
   case PCO_CTRL_NONE: return 0;
   case PCO_CTRL_BRANCH: return 5;
   case PCO_CTRL_LAPC: return 0;
   case PCO_CTRL_SAVL: return 0;
   case PCO_CTRL_CND: return 1;
   case PCO_CTRL_WOP: return 0;
   case PCO_CTRL_WDF: return 0;
   case PCO_CTRL_MUTEX: return 1;
   case PCO_CTRL_NOP: return 1;
   case PCO_CTRL_SBO: return 2;
   case PCO_CTRL_DITR: return 5;
   case PCO_CTRL_ITRSMP: return 6;
   case PCO_CTRL_ITRSMP_EXT: return 7;
   case PCO_CTRL_ITRSMP_EXT2: return 8;
   default: break;
   }

   UNREACHABLE("");
}

static unsigned pco_ctrl_encode_field(uint8_t *bin, enum pco_ctrl_field field, uint64_t val)
{
   uint64_t enc_val;

   switch (field) {
   case PCO_CTRL_RSVD_BRANCH:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 5));
      bin[0] &= 0x7f;
      bin[0] |= (enc_val & 0x1) << 7;
      bin[0] &= 0x9f;
      bin[0] |= ((enc_val >> 1) & 0x3) << 5;
      bin[0] &= 0xfe;
      bin[0] |= ((enc_val >> 3) & 0x1);
      bin[1] &= 0xfe;
      bin[1] |= ((enc_val >> 4) & 0x1);
      return 5;

   case PCO_CTRL_LINK:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xef;
      bin[0] |= (enc_val & 0x1) << 4;
      return 1;

   case PCO_CTRL_BPRED:
      enc_val = val;
      assert(pco_bpred_valid(enc_val));
      bin[0] &= 0xf3;
      bin[0] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_CTRL_ABS:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0xfd;
      bin[0] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_CTRL_BRANCH_OFFSET:
      assert(!(val & 0b1));
      enc_val = val >> 1;
      assert(enc_val < (1ULL << 31));
      bin[1] &= 0x1;
      bin[1] |= (enc_val & 0x7f) << 1;
      bin[2] &= 0x0;
      bin[2] |= ((enc_val >> 7) & 0xff);
      bin[3] &= 0x0;
      bin[3] |= ((enc_val >> 15) & 0xff);
      bin[4] &= 0x0;
      bin[4] |= ((enc_val >> 23) & 0xff);
      return 31;

   case PCO_CTRL_RSVD_CND:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[0] &= 0x7f;
      bin[0] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_CTRL_ADJUST:
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[0] &= 0x9f;
      bin[0] |= (enc_val & 0x3) << 5;
      return 2;

   case PCO_CTRL_PCND:
      enc_val = val;
      assert(pco_pcnd_valid(enc_val));
      bin[0] &= 0xe7;
      bin[0] |= (enc_val & 0x3) << 3;
      return 2;

   case PCO_CTRL_CNDINST:
      enc_val = val;
      assert(pco_cndinst_valid(enc_val));
      bin[0] &= 0xf8;
      bin[0] |= (enc_val & 0x7);
      return 3;

   case PCO_CTRL_LR:
      enc_val = val;
      assert(pco_lr_valid(enc_val));
      bin[0] &= 0x3f;
      bin[0] |= (enc_val & 0x3) << 6;
      return 2;

   case PCO_CTRL_RSVD_MUTEX:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[0] &= 0xcf;
      bin[0] |= (enc_val & 0x3) << 4;
      return 2;

   case PCO_CTRL_ID:
      enc_val = val;
      assert(enc_val < (1ULL << 4));
      bin[0] &= 0xf0;
      bin[0] |= (enc_val & 0xf);
      return 4;

   case PCO_CTRL_RSVD_NOP:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[0] &= 0x0;
      bin[0] |= (enc_val & 0xff);
      return 8;

   case PCO_CTRL_RSVD_SBO:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 7));
      bin[1] &= 0x1;
      bin[1] |= (enc_val & 0x7f) << 1;
      return 7;

   case PCO_CTRL_TGT:
      enc_val = val;
      assert(pco_tgt_valid(enc_val));
      bin[0] &= 0xfe;
      bin[0] |= (enc_val & 0x1);
      return 1;

   case PCO_CTRL_SBO_OFFSET:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[0] &= 0x1;
      bin[0] |= (enc_val & 0x7f) << 1;
      bin[1] &= 0xfe;
      bin[1] |= ((enc_val >> 7) & 0x1);
      return 8;

   case PCO_CTRL_DEST:
      enc_val = val;
      assert(enc_val < (1ULL << 8));
      bin[0] &= 0x0;
      bin[0] |= (enc_val & 0xff);
      return 8;

   case PCO_CTRL_COFF:
      assert(!(val & 0b11));
      enc_val = val >> 2;
      assert(enc_val < (1ULL << 6));
      bin[1] &= 0x3;
      bin[1] |= (enc_val & 0x3f) << 2;
      return 6;

   case PCO_CTRL_P_ITR:
      enc_val = val;
      assert(pco_persp_ctl_valid(enc_val));
      bin[1] &= 0xfc;
      bin[1] |= (enc_val & 0x3);
      return 2;

   case PCO_CTRL_WOFF_B2:
      assert(!(val & 0b11));
      enc_val = val >> 2;
      assert(enc_val < (1ULL << 6));
      bin[2] &= 0x3;
      bin[2] |= (enc_val & 0x3f) << 2;
      return 6;

   case PCO_CTRL_DITR_MODE:
      enc_val = val;
      assert(pco_iter_mode_valid(enc_val));
      bin[2] &= 0xfc;
      bin[2] |= (enc_val & 0x3);
      return 2;

   case PCO_CTRL_ITR_COUNT_B3:
      assert(val >= 1 && val <= 16);
      enc_val = val % 16;
      assert(enc_val < (1ULL << 4));
      bin[3] &= 0xf;
      bin[3] |= (enc_val & 0xf) << 4;
      return 4;

   case PCO_CTRL_COFF_IDX_CTRL_B3:
      enc_val = val;
      assert(pco_idx_ctrl_valid(enc_val));
      bin[3] &= 0xf3;
      bin[3] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_CTRL_WOFF_IDX_CTRL_B3:
      enc_val = val;
      assert(pco_idx_ctrl_valid(enc_val));
      bin[3] &= 0xfc;
      bin[3] |= (enc_val & 0x3);
      return 2;

   case PCO_CTRL_RSVD4_7_DITR:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0x7f;
      bin[4] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_CTRL_F16_B4:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xbf;
      bin[4] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_CTRL_RSVD4_5_DITR:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[4] &= 0xcf;
      bin[4] |= (enc_val & 0x3) << 4;
      return 2;

   case PCO_CTRL_SCHED_CTRL_B4:
      enc_val = val;
      assert(pco_sched_ctrl_valid(enc_val));
      bin[4] &= 0xf3;
      bin[4] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_CTRL_DRC_B4:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xfd;
      bin[4] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_CTRL_SAT:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xfe;
      bin[4] |= (enc_val & 0x1);
      return 1;

   case PCO_CTRL_TEXOFF:
      assert(!(val & 0b11));
      enc_val = val >> 2;
      assert(enc_val < (1ULL << 6));
      bin[2] &= 0x3;
      bin[2] |= (enc_val & 0x3f) << 2;
      return 6;

   case PCO_CTRL_DRC_B2:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[2] &= 0xfd;
      bin[2] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_CTRL_ITRSMP_MODE1:
      enc_val = val;
      assert(pco_iter_mode1_valid(enc_val));
      bin[2] &= 0xfe;
      bin[2] |= (enc_val & 0x1);
      return 1;

   case PCO_CTRL_SMPOFF:
      assert(!(val & 0b11));
      enc_val = val >> 2;
      assert(enc_val < (1ULL << 6));
      bin[3] &= 0x3;
      bin[3] |= (enc_val & 0x3f) << 2;
      return 6;

   case PCO_CTRL_DMN:
      enc_val = val;
      assert(pco_dmn_valid(enc_val));
      bin[3] &= 0xfc;
      bin[3] |= (enc_val & 0x3);
      return 2;

   case PCO_CTRL_WOFF_B4:
      assert(!(val & 0b11));
      enc_val = val >> 2;
      assert(enc_val < (1ULL << 6));
      bin[4] &= 0x3;
      bin[4] |= (enc_val & 0x3f) << 2;
      return 6;

   case PCO_CTRL_PROJ:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[4] &= 0xfd;
      bin[4] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_CTRL_EXT5:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[5] &= 0x7f;
      bin[5] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_CTRL_F16_B5:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[5] &= 0xbf;
      bin[5] |= (enc_val & 0x1) << 6;
      return 1;

   case PCO_CTRL_SCHED_CTRL_B5:
      enc_val = val;
      assert(pco_sched_ctrl_valid(enc_val));
      bin[5] &= 0xcf;
      bin[5] |= (enc_val & 0x3) << 4;
      return 2;

   case PCO_CTRL_CHAN:
      assert(val >= 1 && val <= 4);
      enc_val = val - 1;
      assert(enc_val < (1ULL << 2));
      bin[5] &= 0xf3;
      bin[5] |= (enc_val & 0x3) << 2;
      return 2;

   case PCO_CTRL_NNCOORDS:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[5] &= 0xfd;
      bin[5] |= (enc_val & 0x1) << 1;
      return 1;

   case PCO_CTRL_FCNORM:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[5] &= 0xfe;
      bin[5] |= (enc_val & 0x1);
      return 1;

   case PCO_CTRL_EXT6:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[6] &= 0x7f;
      bin[6] |= (enc_val & 0x1) << 7;
      return 1;

   case PCO_CTRL_ITR_COUNT_B6:
      assert(val >= 1 && val <= 16);
      enc_val = val % 16;
      assert(enc_val < (1ULL << 4));
      bin[6] &= 0x87;
      bin[6] |= (enc_val & 0xf) << 3;
      return 4;

   case PCO_CTRL_COMPARISON:
      enc_val = val;
      assert(enc_val < (1ULL << 1));
      bin[6] &= 0xfb;
      bin[6] |= (enc_val & 0x1) << 2;
      return 1;

   case PCO_CTRL_RSVD6_ITRSMP:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 2));
      bin[6] &= 0xfc;
      bin[6] |= (enc_val & 0x3);
      return 2;

   case PCO_CTRL_RSVD7_ITRSMP:
      assert(val == 0);
      enc_val = val;
      assert(enc_val < (1ULL << 3));
      bin[7] &= 0x1f;
      bin[7] |= (enc_val & 0x7) << 5;
      return 3;

   case PCO_CTRL_COFF_IDX_CTRL_B7:
      enc_val = val;
      assert(pco_idx_ctrl_valid(enc_val));
      bin[7] &= 0xe7;
      bin[7] |= (enc_val & 0x3) << 3;
      return 2;

   case PCO_CTRL_WOFF_IDX_CTRL_B7:
      enc_val = val;
      assert(pco_idx_ctrl_valid(enc_val));
      bin[7] &= 0xf9;
      bin[7] |= (enc_val & 0x3) << 1;
      return 2;

   case PCO_CTRL_ITRSMP_MODE:
      enc_val = val;
      assert(pco_iter_mode_valid(enc_val));
      bin[2] &= 0xfe;
      bin[2] |= (enc_val & 0x1);
      bin[7] &= 0xfe;
      bin[7] |= ((enc_val >> 1) & 0x1);
      return 2;

   default:
      break;
   }

   UNREACHABLE("");
}

struct pco_ctrl_branch {
   bool link : 1;
   enum pco_bpred bpred : 2;
   bool abs : 1;
   unsigned offset : 32;
};

#define pco_ctrl_branch_encode(bin, ...) _pco_ctrl_branch_encode(bin, (const struct pco_ctrl_branch){0, ##__VA_ARGS__})
static unsigned _pco_ctrl_branch_encode(uint8_t *bin, const struct pco_ctrl_branch s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD_BRANCH, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_LINK, s.link);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_BPRED, s.bpred);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ABS, s.abs);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_BRANCH_OFFSET, s.offset);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_ctrl_lapc_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;


   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_ctrl_savl_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;


   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_ctrl_cnd {
   unsigned adjust : 2;
   enum pco_pcnd pcnd : 2;
   enum pco_cndinst cndinst : 3;
};

#define pco_ctrl_cnd_encode(bin, ...) _pco_ctrl_cnd_encode(bin, (const struct pco_ctrl_cnd){0, ##__VA_ARGS__})
static unsigned _pco_ctrl_cnd_encode(uint8_t *bin, const struct pco_ctrl_cnd s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD_CND, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ADJUST, s.adjust);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_PCND, s.pcnd);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_CNDINST, s.cndinst);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_ctrl_wop_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;


   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_ctrl_wdf_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;


   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_ctrl_mutex {
   enum pco_lr lr : 2;
   unsigned id : 4;
};

#define pco_ctrl_mutex_encode(bin, ...) _pco_ctrl_mutex_encode(bin, (const struct pco_ctrl_mutex){0, ##__VA_ARGS__})
static unsigned _pco_ctrl_mutex_encode(uint8_t *bin, const struct pco_ctrl_mutex s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD_MUTEX, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_LR, s.lr);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ID, s.id);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}


static unsigned pco_ctrl_nop_encode(uint8_t *bin)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD_NOP, 0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_ctrl_sbo {
   enum pco_tgt tgt : 1;
   unsigned offset : 8;
};

#define pco_ctrl_sbo_encode(bin, ...) _pco_ctrl_sbo_encode(bin, (const struct pco_ctrl_sbo){0, ##__VA_ARGS__})
static unsigned _pco_ctrl_sbo_encode(uint8_t *bin, const struct pco_ctrl_sbo s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD_SBO, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_TGT, s.tgt);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SBO_OFFSET, s.offset);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_ctrl_ditr {
   unsigned dest : 8;
   unsigned coff : 8;
   enum pco_persp_ctl p : 2;
   unsigned woff : 8;
   enum pco_iter_mode mode : 2;
   unsigned count : 5;
   enum pco_idx_ctrl coff_idx_ctrl : 2;
   enum pco_idx_ctrl woff_idx_ctrl : 2;
   bool f16 : 1;
   enum pco_sched_ctrl sched_ctrl : 2;
   unsigned drc : 1;
   bool sat : 1;
};

#define pco_ctrl_ditr_encode(bin, ...) _pco_ctrl_ditr_encode(bin, (const struct pco_ctrl_ditr){0, ##__VA_ARGS__})
static unsigned _pco_ctrl_ditr_encode(uint8_t *bin, const struct pco_ctrl_ditr s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DEST, s.dest);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_COFF, s.coff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_P_ITR, s.p);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_WOFF_B2, s.woff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DITR_MODE, s.mode);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ITR_COUNT_B3, s.count);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_COFF_IDX_CTRL_B3, s.coff_idx_ctrl);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_WOFF_IDX_CTRL_B3, s.woff_idx_ctrl);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD4_7_DITR, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_F16_B4, s.f16);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD4_5_DITR, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SCHED_CTRL_B4, s.sched_ctrl);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DRC_B4, s.drc);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SAT, s.sat);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_ctrl_itrsmp {
   unsigned dest : 8;
   unsigned coff : 8;
   enum pco_persp_ctl p : 2;
   unsigned texoff : 8;
   unsigned drc : 1;
   enum pco_iter_mode mode : 2;
   unsigned smpoff : 8;
   enum pco_dmn dmn : 2;
   unsigned woff : 8;
   bool proj : 1;
   bool sat : 1;
   bool f16 : 1;
   enum pco_sched_ctrl sched_ctrl : 2;
   unsigned chan : 3;
   bool nncoords : 1;
   bool fcnorm : 1;
};

#define pco_ctrl_itrsmp_encode(bin, ...) _pco_ctrl_itrsmp_encode(bin, (const struct pco_ctrl_itrsmp){0, ##__VA_ARGS__})
static unsigned _pco_ctrl_itrsmp_encode(uint8_t *bin, const struct pco_ctrl_itrsmp s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DEST, s.dest);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_COFF, s.coff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_P_ITR, s.p);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_TEXOFF, s.texoff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DRC_B2, s.drc);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ITRSMP_MODE1, s.mode);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SMPOFF, s.smpoff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DMN, s.dmn);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_WOFF_B4, s.woff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_PROJ, s.proj);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SAT, s.sat);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_EXT5, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_F16_B5, s.f16);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SCHED_CTRL_B5, s.sched_ctrl);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_CHAN, s.chan);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_NNCOORDS, s.nncoords);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_FCNORM, s.fcnorm);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_ctrl_itrsmp_ext {
   unsigned dest : 8;
   unsigned coff : 8;
   enum pco_persp_ctl p : 2;
   unsigned texoff : 8;
   unsigned drc : 1;
   enum pco_iter_mode mode : 2;
   unsigned smpoff : 8;
   enum pco_dmn dmn : 2;
   unsigned woff : 8;
   bool proj : 1;
   bool sat : 1;
   bool f16 : 1;
   enum pco_sched_ctrl sched_ctrl : 2;
   unsigned chan : 3;
   bool nncoords : 1;
   bool fcnorm : 1;
   unsigned count : 5;
   bool comparison : 1;
};

#define pco_ctrl_itrsmp_ext_encode(bin, ...) _pco_ctrl_itrsmp_ext_encode(bin, (const struct pco_ctrl_itrsmp_ext){0, ##__VA_ARGS__})
static unsigned _pco_ctrl_itrsmp_ext_encode(uint8_t *bin, const struct pco_ctrl_itrsmp_ext s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DEST, s.dest);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_COFF, s.coff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_P_ITR, s.p);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_TEXOFF, s.texoff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DRC_B2, s.drc);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ITRSMP_MODE1, s.mode);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SMPOFF, s.smpoff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DMN, s.dmn);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_WOFF_B4, s.woff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_PROJ, s.proj);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SAT, s.sat);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_EXT5, 1);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_F16_B5, s.f16);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SCHED_CTRL_B5, s.sched_ctrl);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_CHAN, s.chan);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_NNCOORDS, s.nncoords);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_FCNORM, s.fcnorm);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_EXT6, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ITR_COUNT_B6, s.count);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_COMPARISON, s.comparison);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD6_ITRSMP, 0);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

struct pco_ctrl_itrsmp_ext2 {
   unsigned dest : 8;
   unsigned coff : 8;
   enum pco_persp_ctl p : 2;
   unsigned texoff : 8;
   unsigned drc : 1;
   unsigned smpoff : 8;
   enum pco_dmn dmn : 2;
   unsigned woff : 8;
   bool proj : 1;
   bool sat : 1;
   bool f16 : 1;
   enum pco_sched_ctrl sched_ctrl : 2;
   unsigned chan : 3;
   bool nncoords : 1;
   bool fcnorm : 1;
   unsigned count : 5;
   bool comparison : 1;
   enum pco_idx_ctrl coff_idx_ctrl : 2;
   enum pco_idx_ctrl woff_idx_ctrl : 2;
   enum pco_iter_mode mode : 2;
};

#define pco_ctrl_itrsmp_ext2_encode(bin, ...) _pco_ctrl_itrsmp_ext2_encode(bin, (const struct pco_ctrl_itrsmp_ext2){0, ##__VA_ARGS__})
static unsigned _pco_ctrl_itrsmp_ext2_encode(uint8_t *bin, const struct pco_ctrl_itrsmp_ext2 s)
{
   unsigned bits_encoded = 0;

   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DEST, s.dest);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_COFF, s.coff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_P_ITR, s.p);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_TEXOFF, s.texoff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DRC_B2, s.drc);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SMPOFF, s.smpoff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_DMN, s.dmn);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_WOFF_B4, s.woff);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_PROJ, s.proj);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SAT, s.sat);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_EXT5, 1);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_F16_B5, s.f16);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_SCHED_CTRL_B5, s.sched_ctrl);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_CHAN, s.chan);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_NNCOORDS, s.nncoords);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_FCNORM, s.fcnorm);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_EXT6, 1);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ITR_COUNT_B6, s.count);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_COMPARISON, s.comparison);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD6_ITRSMP, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_RSVD7_ITRSMP, 0);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_COFF_IDX_CTRL_B7, s.coff_idx_ctrl);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_WOFF_IDX_CTRL_B7, s.woff_idx_ctrl);
   bits_encoded += pco_ctrl_encode_field(bin, PCO_CTRL_ITRSMP_MODE, s.mode);

   assert(!(bits_encoded % 8));
   return bits_encoded / 8;
}

#endif /* PCO_ISA_H */
