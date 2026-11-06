#include "fixed.h"
#include "reciprocal.h"
#include "tan_table.inc"
#include "fixed_tables.inc"
enum { PI_RAW=205887, HALF_PI_RAW=102944, TWO_PI_RAW=411775 };
Fix FxWrapAngle(Fix v){int r=v.raw%TWO_PI_RAW;if(r>PI_RAW)r-=TWO_PI_RAW;if(r<-PI_RAW)r+=TWO_PI_RAW;return Fix::Raw(r);}
static int DegreeIndex(Fix v){
    int r=v.raw%TWO_PI_RAW;if(r<0)r+=TWO_PI_RAW;
    // Wrapped r*360 fits signed 32 bits; no 64-bit division is needed.
    int index=(r*360+TWO_PI_RAW/2)/TWO_PI_RAW;
    return index==360?0:index;
}
Fix FxSin(Fix v){return Fix::Raw(sineTable[DegreeIndex(v)]);}
Fix FxCos(Fix v){int index=DegreeIndex(v)+90;if(index>=360)index-=360;return Fix::Raw(sineTable[index]);}
Fix FxTan(Fix v){return Fix::Raw(tangentTable[DegreeIndex(v)]);}
Fix FxSqrt(Fix v){
    if(v.raw<=0)return Fix::Raw(0);
    unsigned n=(unsigned)v.raw;int shift=0;
    while(n<65536u){n<<=2;--shift;}
    while(n>=262144u){n>>=2;++shift;}
    unsigned offset=n-65536u,index=offset>>8,fraction=offset&255;
    unsigned root=(unsigned)(sqrtTable[index]+((sqrtTable[index+1]-sqrtTable[index])*(int)fraction)/256);
    return Fix::Raw((int)(shift>=0?root<<shift:root>>(-shift)));
}
Fix FxAtan2(Fix y,Fix x){
    unsigned ax=(unsigned)(x.raw<0?-(Wide)x.raw:x.raw),ay=(unsigned)(y.raw<0?-(Wide)y.raw:y.raw);
    if(!(ax|ay))return Fix::Raw(0);
    unsigned small=ax<ay?ax:ay,large=ax<ay?ay:ax;
    unsigned scaled=OctantRatio(small,large);
    unsigned index=scaled>>16,fraction=scaled&65535;
    int angle=atanTable[index];
    if(index<256)angle+=(int)(((Wide)(atanTable[index+1]-angle)*fraction)>>16);
    if(ay>ax)angle=HALF_PI_RAW-angle;
    if(x.raw<0)angle=PI_RAW-angle;
    if(y.raw<0)angle=-angle;
    return Fix::Raw(angle);
}
