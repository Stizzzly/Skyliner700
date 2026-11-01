#ifndef SKY_RECIPROCAL_H
#define SKY_RECIPROCAL_H
#include "fixed.h"
// Shared normalized reciprocal seed/refinement. Input must be positive.
// 1 KiB seed table and two integer Newton refinements; ARMv4T needs no CLZ.
static inline unsigned NormalizedInverse(unsigned n,int& shift){
    shift=0;
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
    return x;
}
// Exact Q40 inverse for positive Q16.16 depths >= 2m.
static inline unsigned DepthInverse(int depth){
    int shift;unsigned x=NormalizedInverse((unsigned)depth,shift);
    unsigned inverse=x>>(23-shift);
    // Restore exact floor(2^40/depth); two-step error < 6 normalized units, < 1 Q40 unit.
    if((UWide)(inverse+1)*(unsigned)depth<=(1ULL<<40))++inverse;
    return inverse;
}
// Exact Q32 reciprocal for any positive signed-32-bit edge step.
// Reuse the same 1 KiB seed table; correction runs once per edge, not per row.
static inline UWide EdgeInverse(unsigned divisor){
    int shift;unsigned x=NormalizedInverse(divisor,shift);
    UWide inverse=x>>(31-shift);
    while((inverse+1)*divisor<=(1ULL<<32))++inverse;
    return inverse;
}
static inline unsigned EdgeFloor(unsigned numerator,unsigned divisor,UWide inverse){
    unsigned q=(unsigned)(((UWide)numerator*inverse)>>32);
    if(numerator-(UWide)q*divisor>=divisor)++q;
    return q;
}
#endif
