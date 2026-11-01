#ifndef SKY_RECIPROCAL_H
#define SKY_RECIPROCAL_H
#include "fixed.h"
// Exact Q40 inverse for positive Q16.16 depths >= 2m. No runtime division.
// 1 KiB seed table and two integer Newton refinements; ARMv4T needs no CLZ.
static inline unsigned DepthInverse(int depth){
    unsigned n=(unsigned)depth;int shift=0;
    if(n<0x00010000u){n<<=16;shift+=16;}
    if(n<0x01000000u){n<<=8;shift+=8;}
    if(n<0x10000000u){n<<=4;shift+=4;}
    if(n<0x40000000u){n<<=2;shift+=2;}
    if(n<0x80000000u){n<<=1;shift+=1;}
    static const unsigned seeds[256]={
#include "reciprocal_table.inc"
    };
    unsigned x=seeds[(n>>23)-256];
    // Underestimate 2^63/n: products and residual stay unsigned/in range.
    UWide residual=(1ULL<<63)-(UWide)n*x;
    x+=(unsigned)(((UWide)x*(residual>>32))>>31);
    residual=(1ULL<<63)-(UWide)n*x;
    x+=(unsigned)(((UWide)x*(residual>>32))>>31);
    unsigned inverse=(unsigned)(x>>(23-shift));
    // Restore exact floor(2^40/depth); two-step error < 6 normalized units, < 1 Q40 unit.
    if((UWide)(inverse+1)*(unsigned)depth<=(1ULL<<40))++inverse;
    return inverse;
}
#endif
