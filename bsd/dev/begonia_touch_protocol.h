/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef BEGONIA_TOUCH_PROTOCOL_H
#define BEGONIA_TOUCH_PROTOCOL_H
/* 12-bit, MSB aligned ADC reply from BU21025. */
static unsigned begonia_touch_adc(unsigned char hi,unsigned char lo)
{return ((unsigned)hi<<4)|(lo>>4);}
/* 1=contact, 0=release, -1=invalid/noisy sample (preserve previous state). */
static int begonia_touch_point(unsigned x,unsigned y,unsigned z,unsigned *px,unsigned *py)
{
    if(x>4095 || y>4095 || z>4095)return -1;
    if(z<=5 || z==4095)return 0;
    /* Board resistive-panel plausibility threshold, RX=730, RY=305. */
    int64_t resistance=(int64_t)((730*x)/4096)*((4096000/z)-1000)-
        (int64_t)(1000-(y*1000)/4096)*305;
    if(resistance>900000)return -1;
    /* Measured physical corner bounds, 2026-10-21 (two passes).
     * Landscape horizontal axis is portrait Y: invert it to match panel. */
    int cx=(int)x-150,cy=(int)y-320;
    if(cx<0)cx=0;else if(cx>3800)cx=3800;
    if(cy<0)cy=0;else if(cy>3510)cy=3510;
    *px=((3510-cy)*799)/3510;*py=1279-(cx*1279)/3800;
    return 1;
}
#endif
