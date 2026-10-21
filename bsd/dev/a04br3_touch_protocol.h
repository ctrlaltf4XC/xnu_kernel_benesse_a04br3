/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_TOUCH_PROTOCOL_H
#define A04BR3_TOUCH_PROTOCOL_H
/* 12-bit, MSB aligned ADC reply from BU21025. */
static unsigned a04br3_touch_adc(unsigned char hi,unsigned char lo)
{return ((unsigned)hi<<4)|(lo>>4);}
/* 1=contact, 0=release, -1=invalid/noisy sample (preserve previous state). */
static int a04br3_touch_point(unsigned x,unsigned y,unsigned z,unsigned *px,unsigned *py)
{
    if(x>4095 || y>4095 || z>4095)return -1;
    if(z<=5 || z==4095)return 0;
    /* Board resistive-panel plausibility threshold, RX=730, RY=305. */
    int64_t resistance=(int64_t)((730*x)/4096)*((4096000/z)-1000)-
        (int64_t)(1000-(y*1000)/4096)*305;
    if(resistance>900000)return -1;
    *px=((4095-y)*800)/4096;*py=(x*1280)/4096;
    return 1;
}
#endif
