/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Offline lowering of one zero-clear tile into the existing native208 task.
 * Original-board REA FUN13e2e4: address shared words + CSR row pitch;
 * existing fixed32x32 ISP extent/EOT/PDS/task fields remain unchanged.
 * This is a canonical native operation, NOT an AGX packet/shader decoder or
 * IOSurface import. Native format22 is not asserted to be iOS BGRA.
 * Consumer must acquire/cache/pin/program the resources and prove pixels.
 * The current fixed-profile runtime rejects this generalized image; no live
 * admission, MMIO, registry/capability, fence or success callback is added.
 */
#ifndef A04BR3_ROGUE_CLEAR_TILE_H
#define A04BR3_ROGUE_CLEAR_TILE_H
#include "A04BR3RogueTransferFill.h"
struct a04_rgx_clear_tile_request {
 uint32_t native_format,width,height,pitch_pixels,x,y;
 uint32_t colour[4]; /* exact raw8 channels; this subset permits zero ONLY */
};
struct a04_rgx_clear_tile {
 a04_rgx_tq_clear task;
 uint32_t first_byte,end_byte,row_bytes;
};
/* All source extent/stride arithmetic uses the actual owned map, never an
 * unretained IOSurface physical pointer. Restrict origin/stride to32pixel
 * multiples (128byte alignment) and tile to32x32: no unproved partial tiles,
 * render-state fields, odd-stride rounding, MSAA/twiddling or format mapping.
 * On rejection every output/resource byte is unchanged.
 */
static inline int a04_rgx_clear_tile_build(a04_rgx_clear_tile *out,
 const a04_rgx_render_resources *r,const a04_rgx_bif *pds,const a04_rgx_bif *usc,
 const a04_rgx_clear_tile_request *request)
{
 if(!a04_rgx_fw_pointer(out,sizeof(*out)) || ((uintptr_t)out&7) ||
    !a04_rgx_fw_pointer(request,sizeof(*request)) || !r || !pds || !usc ||
    a04_rgx_fw_alias(out,sizeof(*out),request,sizeof(*request)) ||
    a04_rgx_fw_alias(out,sizeof(*out),r,sizeof(*r)) ||
    a04_rgx_fw_alias(out,sizeof(*out),pds,sizeof(*pds)) ||
    a04_rgx_fw_alias(out,sizeof(*out),usc,sizeof(*usc)))return 1;
 const a04_rgx_clear_tile_request q=*request;
 if(q.native_format!=0x22 || q.width<32 || q.height<32 ||
    q.pitch_pixels<q.width || (q.pitch_pixels&31) || (q.x&31) || (q.y&31) ||
    q.x>q.width-32 || q.y>q.height-32 ||
    (q.colour[0]|q.colour[1]|q.colour[2]|q.colour[3]))return 1;
 if(!a04_rgx_render_view_valid(r) || q.pitch_pixels>r->bif.map_bytes/4)return 1;
 uint64_t row=(uint64_t)q.pitch_pixels*4;
 uint64_t extent=row*q.height;
 if(extent>r->bif.map_bytes)return 1;
 uint64_t first=row*q.y+(uint64_t)q.x*4;
 uint64_t end=first+31*row+128;
 if(end>extent || end>r->bif.map_bytes)return 1;
 uint64_t address=r->bif.va+first;
 if(address<r->bif.va || address>=(UINT64_C(1)<<40) || (address&127))return 1;
 const a04_rgx_bif heaps[]={r->bif,*pds,*usc};
 for(const auto &h:heaps){
  const a04_rgx_render_buffer buffers[]={h.pc,h.pd,h.pt,h.data};
  for(const auto &b:buffers)
   if(a04_rgx_fw_alias(out,sizeof(*out),b.cpu,b.bytes) ||
      a04_rgx_fw_alias(request,sizeof(*request),b.cpu,b.bytes))return 1;
 }
 if(a04_rgx_fw_alias(out,sizeof(*out),r->bank.cpu,r->bank.bytes) ||
    a04_rgx_fw_alias(request,sizeof(*request),r->bank.cpu,r->bank.bytes))return 1;
 a04_rgx_clear_tile generated={};
 if(a04_rgx_tq_clear_build(&generated.task,r,pds,usc))return 1;
 /* Native linear format22 path. Original full-surface stride is pixels,
  * clipped relative32x32 origin0. VA is rebased to the requested tile; no
  * global clip coordinates or unseen AGX commands are forwarded.
  * The source stride mask encodes even-pixel pitch; our alignment is stricter.
  */
 uint64_t csr0=(UINT64_C(2)<<56)|
  (((uint64_t)(q.pitch_pixels-1)&UINT64_C(0x1fffe))<<33)|
  (UINT64_C(1)<<23)|(UINT64_C(2)<<26)|(UINT64_C(3)<<29);
 a04_rgx_tq_put64((uint8_t *)&generated.task.payload+0x80,csr0);
 if(a04_rgx_pixel_event_build(&generated.task.pixel,0,A04_RGX_EOT_SINGLE_RT_TEMPS,
    (uint32_t)(address>>2),(uint32_t)(address>>34)|(1U<<19)))return 1;
 generated.first_byte=(uint32_t)first;generated.end_byte=(uint32_t)end;
 generated.row_bytes=(uint32_t)row;*out=generated;return 0;
}
#endif
