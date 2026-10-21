/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Bounded, allocation-free ELF32 MIPS / firmware-info-v1 parser for BVNC
 * 22.40.54.30. Format: MediaTek/Imagination m1.11ED5425693 rgx_fw_info.h,
 * rgx_mips.h and rgxfwimageutils.c. This is NOT a firmware execution API.
 *
 * The caller owns readable image_bytes at image and must keep them immutable
 * for the entire call (including unpack). Output storage must be writable and
 * exclusively owned. Capacity is caller-supplied, never inferred from the ELF.
 * All format validation and output preflight precede output writes. Failures
 * leave every caller output unchanged. No allocation, MMIO, cache or DMA here.
 * Unpack zeroes allocation holes/BSS, then copies PT_LOAD file bytes. It does
 * not patch boot parameters. Parsed sizes/permissions do not authorize execution.
 */
#ifndef A04BR3_ROGUE_FIRMWARE_H
#define A04BR3_ROGUE_FIRMWARE_H
#include <stdint.h>
#include <stddef.h>

enum {
    A04_RGX_FW_LIMIT = 32 * 1024 * 1024,
    A04_RGX_FW_INFO_BYTES = 4096,
    A04_RGX_FW_SECTION_COUNT = 6,
    A04_RGX_FW_MAX_SEGMENTS = 64
};
enum a04_rgx_fw_error {
    A04_RGX_FW_OK = 0,
    A04_RGX_FW_ARGUMENT = 1,
    A04_RGX_FW_SIZE = 2,
    A04_RGX_FW_INFO = 3,
    A04_RGX_FW_BVNC = 4,
    A04_RGX_FW_LAYOUT = 5,
    A04_RGX_FW_ELF = 6,
    A04_RGX_FW_SEGMENT = 7,
    A04_RGX_FW_ENTRY = 8,
    A04_RGX_FW_CAPACITY = 9,
    A04_RGX_FW_ALIAS = 10
};
struct a04_rgx_fw_section {
    uint32_t id, kind, base, maximum, allocated, offset;
};
struct a04_rgx_fw_segment {
    uint32_t kind, target, source, address, filesz, memsz, permissions, alignment;
};
struct a04_rgx_fw_layout {
    uint32_t entry_point, code_bytes, data_bytes, segment_count;
    /* Canonical order: MIPS_CODE=4 through MIPS_STACK=9, indexed by id-4. */
    struct a04_rgx_fw_section sections[A04_RGX_FW_SECTION_COUNT];
    /* PT_LOAD only; array tail is zero. Other program headers are ignored. */
    struct a04_rgx_fw_segment segments[A04_RGX_FW_MAX_SEGMENTS];
};

/* Readers below are private helpers: call only after checking their complete
 * 2/4/8-byte spans with a04_rgx_fw_span, or within a checked fixed-size record.
 * Byte loads deliberately support unaligned input and avoid host endianness. */
static inline uint32_t a04_rgx_fw_u16(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}
static inline uint32_t a04_rgx_fw_u32(const unsigned char *p)
{
    return a04_rgx_fw_u16(p) | (a04_rgx_fw_u16(p + 2) << 16);
}
static inline uint64_t a04_rgx_fw_u64(const unsigned char *p)
{
    return (uint64_t)a04_rgx_fw_u32(p) | ((uint64_t)a04_rgx_fw_u32(p + 4) << 32);
}
static inline int a04_rgx_fw_span(size_t total, size_t offset, size_t count)
{
    return offset <= total && count <= total - offset;
}
static inline int a04_rgx_fw_pointer(const void *p, size_t count)
{
    return p && count <= (uintptr_t)-1 - (uintptr_t)p;
}
static inline int a04_rgx_fw_alias(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x <= y ? y - x < an : x - y < bn;
}
static inline void a04_rgx_fw_zero(void *p, size_t count)
{
    unsigned char *bytes = (unsigned char *)p;
    for (size_t n = 0; n < count; ++n) bytes[n] = 0;
}
static inline int a04_rgx_fw_overlap(uint64_t a, uint64_t an, uint64_t b, uint64_t bn)
{
    return a < b + bn && b < a + an;
}

static inline int a04_rgx_fw_parse(const void *image, size_t image_bytes,
                                  struct a04_rgx_fw_layout *out)
{
    if (!image || !out) return A04_RGX_FW_ARGUMENT;
    if (image_bytes <= A04_RGX_FW_INFO_BYTES || image_bytes > A04_RGX_FW_LIMIT)
        return A04_RGX_FW_SIZE;
    if (!a04_rgx_fw_pointer(image, image_bytes) || !a04_rgx_fw_pointer(out, sizeof(*out)))
        return A04_RGX_FW_ARGUMENT;
    if (a04_rgx_fw_alias(image, image_bytes, out, sizeof(*out))) return A04_RGX_FW_ALIAS;
    const unsigned char *bytes = (const unsigned char *)image;
    const size_t info = image_bytes - A04_RGX_FW_INFO_BYTES;
    if (!a04_rgx_fw_span(image_bytes, info, 32)) return A04_RGX_FW_SIZE;
    const unsigned char *header = bytes + info;
    uint32_t count = a04_rgx_fw_u32(header + 8);
    if (a04_rgx_fw_u32(header) != 1 || a04_rgx_fw_u32(header + 4) != 32 ||
        a04_rgx_fw_u32(header + 12) != 24 || !count || count > 10 ||
        !a04_rgx_fw_span(A04_RGX_FW_INFO_BYTES, 32, (size_t)count * 24) ||
        a04_rgx_fw_u32(header + 24) != 0) return A04_RGX_FW_INFO;
    if (a04_rgx_fw_u64(header + 16) != 0x001600280036001eULL) return A04_RGX_FW_BVNC;
    struct a04_rgx_fw_layout candidate;
    a04_rgx_fw_zero(&candidate, sizeof(candidate));
    unsigned seen = 0;
    for (uint32_t i = 0; i < count; ++i) {
        size_t offset = info + 32 + (size_t)i * 24;
        if (!a04_rgx_fw_span(image_bytes, offset, 24)) return A04_RGX_FW_LAYOUT;
        const unsigned char *p = bytes + offset;
        struct a04_rgx_fw_section s;
        s.id = a04_rgx_fw_u32(p); s.kind = a04_rgx_fw_u32(p + 4);
        s.base = a04_rgx_fw_u32(p + 8); s.maximum = a04_rgx_fw_u32(p + 12);
        s.allocated = a04_rgx_fw_u32(p + 16); s.offset = a04_rgx_fw_u32(p + 20);
        if (s.id < 4 || s.id > 9 || s.kind != (s.id < 7 ? 1U : 2U) ||
            (seen & (1U << (s.id - 4)))) return A04_RGX_FW_LAYOUT;
        if (!s.allocated || s.allocated > s.maximum ||
            (uint64_t)s.base + s.allocated > 0x100000000ULL ||
            !a04_rgx_fw_span(A04_RGX_FW_LIMIT, s.offset, s.allocated))
            return A04_RGX_FW_LAYOUT;
        for (unsigned j = 0; j < A04_RGX_FW_SECTION_COUNT; ++j) {
            if (!(seen & (1U << j))) continue;
            const struct a04_rgx_fw_section *old = &candidate.sections[j];
            if (a04_rgx_fw_overlap(s.base, s.allocated, old->base, old->allocated) ||
                (s.kind == old->kind &&
                 a04_rgx_fw_overlap(s.offset, s.allocated, old->offset, old->allocated)))
                return A04_RGX_FW_LAYOUT;
        }
        candidate.sections[s.id - 4] = s;
        seen |= 1U << (s.id - 4);
        uint32_t end = s.offset + s.allocated;
        if (s.kind == 1 && end > candidate.code_bytes) candidate.code_bytes = end;
        if (s.kind == 2 && end > candidate.data_bytes) candidate.data_bytes = end;
    }
    if (seen != 63) return A04_RGX_FW_LAYOUT;
    /* ELF must fit before the information trailer, not merely inside image. */
    if (!a04_rgx_fw_span(info, 0, 52)) return A04_RGX_FW_ELF;
    if (bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F' ||
        bytes[4] != 1 || bytes[5] != 1 || bytes[6] != 1 ||
        a04_rgx_fw_u16(bytes + 16) != 2 || a04_rgx_fw_u16(bytes + 18) != 8 ||
        a04_rgx_fw_u32(bytes + 20) != 1 || a04_rgx_fw_u16(bytes + 40) != 52 ||
        a04_rgx_fw_u16(bytes + 42) != 32) return A04_RGX_FW_ELF;
    candidate.entry_point = a04_rgx_fw_u32(bytes + 24);
    uint32_t phoff = a04_rgx_fw_u32(bytes + 28), phnum = a04_rgx_fw_u16(bytes + 44);
    if (!phnum || phnum > A04_RGX_FW_MAX_SEGMENTS || phoff < 52 ||
        !a04_rgx_fw_span(info, phoff, (size_t)phnum * 32)) return A04_RGX_FW_ELF;
    int entry_found = 0;
    for (uint32_t i = 0; i < phnum; ++i) {
        size_t position = (size_t)phoff + (size_t)i * 32;
        if (!a04_rgx_fw_span(info, position, 32)) return A04_RGX_FW_ELF;
        const unsigned char *p = bytes + position;
        if (a04_rgx_fw_u32(p) != 1) continue;
        struct a04_rgx_fw_segment s;
        s.source = a04_rgx_fw_u32(p + 4); s.address = a04_rgx_fw_u32(p + 8);
        s.filesz = a04_rgx_fw_u32(p + 16); s.memsz = a04_rgx_fw_u32(p + 20);
        s.permissions = a04_rgx_fw_u32(p + 24); s.alignment = a04_rgx_fw_u32(p + 28);
        if (s.filesz > s.memsz || !a04_rgx_fw_span(info, s.source, s.filesz) ||
            (uint64_t)s.address + s.memsz > 0x100000000ULL) return A04_RGX_FW_SEGMENT;
        const struct a04_rgx_fw_section *match = NULL;
        for (unsigned j = 0; j < A04_RGX_FW_SECTION_COUNT; ++j) {
            const struct a04_rgx_fw_section *section = &candidate.sections[j];
            if (s.address >= section->base &&
                (uint64_t)s.address + s.memsz <= (uint64_t)section->base + section->allocated) {
                if (match) return A04_RGX_FW_SEGMENT;
                match = section;
            }
        }
        if (!match) return A04_RGX_FW_SEGMENT;
        s.kind = match->kind; s.target = match->offset + (s.address - match->base);
        uint32_t capacity = s.kind == 1 ? candidate.code_bytes : candidate.data_bytes;
        if (!a04_rgx_fw_span(capacity, s.target, s.memsz)) return A04_RGX_FW_SEGMENT;
        for (unsigned j = 0; j < candidate.segment_count; ++j) {
            const struct a04_rgx_fw_segment *old = &candidate.segments[j];
            if (s.kind == old->kind &&
                a04_rgx_fw_overlap(s.target, s.memsz, old->target, old->memsz))
                return A04_RGX_FW_SEGMENT;
        }
        if (candidate.segment_count >= A04_RGX_FW_MAX_SEGMENTS) return A04_RGX_FW_SEGMENT;
        candidate.segments[candidate.segment_count++] = s;
        uint32_t entry = candidate.entry_point & ~1U; /* microMIPS mode bit */
        if ((s.permissions & 1) && entry >= s.address &&
            (uint64_t)entry < (uint64_t)s.address + s.memsz) entry_found = 1;
    }
    if (!entry_found) return A04_RGX_FW_ENTRY;
    *out = candidate;
    return A04_RGX_FW_OK;
}

static inline int a04_rgx_fw_unpack(const void *image, size_t image_bytes,
    void *code, size_t code_capacity, void *data, size_t data_capacity,
    struct a04_rgx_fw_layout *out)
{
    /* Reparse rather than trusting a caller-supplied/mutable layout. */
    struct a04_rgx_fw_layout layout;
    int error = a04_rgx_fw_parse(image, image_bytes, &layout);
    if (error) return error;
    if (!code || !data) return A04_RGX_FW_ARGUMENT;
    if (code_capacity < layout.code_bytes || data_capacity < layout.data_bytes)
        return A04_RGX_FW_CAPACITY;
    if (!a04_rgx_fw_pointer(code, layout.code_bytes) ||
        !a04_rgx_fw_pointer(data, layout.data_bytes) ||
        (out && !a04_rgx_fw_pointer(out, sizeof(*out)))) return A04_RGX_FW_ARGUMENT;
    if (a04_rgx_fw_alias(code, layout.code_bytes, data, layout.data_bytes) ||
        a04_rgx_fw_alias(image, image_bytes, code, layout.code_bytes) ||
        a04_rgx_fw_alias(image, image_bytes, data, layout.data_bytes) ||
        (out && (a04_rgx_fw_alias(out, sizeof(*out), image, image_bytes) ||
                 a04_rgx_fw_alias(out, sizeof(*out), code, layout.code_bytes) ||
                 a04_rgx_fw_alias(out, sizeof(*out), data, layout.data_bytes))))
        return A04_RGX_FW_ALIAS;
    a04_rgx_fw_zero(code, layout.code_bytes);
    a04_rgx_fw_zero(data, layout.data_bytes);
    const unsigned char *bytes = (const unsigned char *)image;
    for (unsigned i = 0; i < layout.segment_count; ++i) {
        const struct a04_rgx_fw_segment *s = &layout.segments[i];
        unsigned char *target = (unsigned char *)(s->kind == 1 ? code : data) + s->target;
        for (uint32_t n = 0; n < s->filesz; ++n) target[n] = bytes[s->source + n];
    }
    if (out) *out = layout;
    return A04_RGX_FW_OK;
}
#endif
