/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Offline-only decoder of the OBSERVED iOS16.7.16 AGX A11/IOGPU submit26
 * envelope. Source: owned standalone REA, agx-protocol-contract.json:
 * IOGPUCommandQueueSubmitCommands + AGX fill/stride getters, record56bytes.
 * No IOService/Metal capability, shmem allocation/ownership proof, payload
 * grammar, shader translation, callback invocation or firmware submission.
 * IDs and callback tokens remain opaque data. Reserved words are preserved:
 * observed producer zeros are NOT evidence that other values are invalid.
 * 1..64 records is a local offline resource bound, NOT a native ABI limit.
 */
#ifndef A04BR3_ROGUE_IOGPU_ENVELOPE_H
#define A04BR3_ROGUE_IOGPU_ENVELOPE_H
#include <stdint.h>
#include <stddef.h>
enum { A04_IOGPU_A11_RECORD_BYTES=56,A04_IOGPU_OFFLINE_RECORD_LIMIT=64 };
struct a04_iogpu56_record {
    uint32_t command_shmem_id,segments_shmem_id,optional08_shmem_id,reserved0c;
    uint64_t scheduled_token,completed_token;
    uint32_t optional20_shmem_id,reserved24,deadline_namespace,reserved2c;
    uint64_t deadline;
};
struct a04_iogpu56_envelope {uint64_t queue_id;uint32_t flags,count;};
static inline int a04_iogpu_span(const void *p,size_t n)
{return p && n && (uintptr_t)p<=UINTPTR_MAX-n;}
static inline int a04_iogpu_overlap(const void *a,size_t an,const void *b,size_t bn)
{return (uintptr_t)a<(uintptr_t)b+bn && (uintptr_t)b<(uintptr_t)a+an;}
static inline uint32_t a04_iogpu_le32(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline uint64_t a04_iogpu_le64(const unsigned char *p)
{return a04_iogpu_le32(p)|(uint64_t)a04_iogpu_le32(p+4)<<32;}
/* Success means DECODED ONLY. No ID/token is dereferenced or treated as an
 * address, and no command bytes behind the IDs are inspected/accepted here.
 * All structural checks precede output writes; rejected buffers stay intact. */
static inline int a04_iogpu56_decode(const uint64_t *scalar,size_t scalar_count,
    const void *input,size_t input_bytes,struct a04_iogpu56_record *records,
    size_t record_capacity,struct a04_iogpu56_envelope *meta)
{
    if(scalar_count!=4 || !a04_iogpu_span(scalar,32) || ((uintptr_t)scalar&7))return 1;
    /* Native callers zero-extend flags32/count32, and AGX supplies stride56.
     * Generic IOGPU stride40 is a different format, deliberately unsupported. */
    uint64_t count=scalar[2];
    if(scalar[1]>UINT32_MAX || count>UINT32_MAX || !count ||
       count>A04_IOGPU_OFFLINE_RECORD_LIMIT || scalar[3]!=A04_IOGPU_A11_RECORD_BYTES ||
       count>SIZE_MAX/A04_IOGPU_A11_RECORD_BYTES)return 1;
    size_t bytes=(size_t)count*A04_IOGPU_A11_RECORD_BYTES;
    if(input_bytes!=bytes || record_capacity<count ||
       !a04_iogpu_span(input,bytes) || !a04_iogpu_span(records,bytes) || ((uintptr_t)records&7) ||
       !a04_iogpu_span(meta,sizeof(*meta)) || ((uintptr_t)meta&7))return 1;
    if(a04_iogpu_overlap(records,bytes,input,bytes) || a04_iogpu_overlap(records,bytes,scalar,32) ||
       a04_iogpu_overlap(meta,sizeof(*meta),input,bytes) || a04_iogpu_overlap(meta,sizeof(*meta),scalar,32) ||
       a04_iogpu_overlap(meta,sizeof(*meta),records,bytes))return 1;
    /* Snapshot scalars before any output so no result depends on overlap. */
    uint64_t queue=scalar[0];uint32_t flags=(uint32_t)scalar[1];
    const unsigned char *p=(const unsigned char *)input;
    for(size_t n=0;n<(size_t)count;n++,p+=56){
        struct a04_iogpu56_record r;
        r.command_shmem_id=a04_iogpu_le32(p);r.segments_shmem_id=a04_iogpu_le32(p+4);
        r.optional08_shmem_id=a04_iogpu_le32(p+8);r.reserved0c=a04_iogpu_le32(p+12);
        r.scheduled_token=a04_iogpu_le64(p+16);r.completed_token=a04_iogpu_le64(p+24);
        r.optional20_shmem_id=a04_iogpu_le32(p+32);r.reserved24=a04_iogpu_le32(p+36);
        r.deadline_namespace=a04_iogpu_le32(p+40);r.reserved2c=a04_iogpu_le32(p+44);
        r.deadline=a04_iogpu_le64(p+48);records[n]=r;
    }
    meta->queue_id=queue;meta->flags=flags;meta->count=(uint32_t)count;return 0;
}
#ifdef __cplusplus
static_assert(sizeof(a04_iogpu56_record)==56 && offsetof(a04_iogpu56_record,deadline)==48,"observed AGX envelope");
#else
_Static_assert(sizeof(struct a04_iogpu56_record)==56 && offsetof(struct a04_iogpu56_record,deadline)==48,"observed AGX envelope");
#endif
#endif
