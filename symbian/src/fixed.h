#ifndef SKY_FIXED_H
#define SKY_FIXED_H
// ARMI permits interworking, but our application code must remain ARM/A32.
#if defined(__SYMBIAN32__) && (defined(__thumb__) || defined(__thumb))
#error Skyliner700 requires 32-bit ARM code; build ARMI, not THUMB.
#endif
// Signed Q16.16. No floating-point constructors or conversions are provided.
// SKY_CONST values and lookup tables are generated offline with Python.
typedef long long Wide;
typedef unsigned long long UWide;
class Fix {
public:
    int raw;
    Fix():raw(0) {}
    Fix(int integer):raw(Narrow((Wide)integer*65536)) {}
    static Fix Raw(int value){Fix f;f.raw=value;return f;}
    int Int() const {return raw/65536;}
    static int Narrow(Wide v){return v>2147483647LL?2147483647:(v<(-2147483647LL-1)?(-2147483647-1):(int)v);}
    Fix operator+(Fix b)const{return Raw(Narrow((Wide)raw+b.raw));}
    Fix operator-(Fix b)const{return Raw(Narrow((Wide)raw-b.raw));}
    Fix operator-()const{return Raw(raw==(-2147483647-1)?2147483647:-raw);}
    Fix operator*(Fix b)const{return Raw(Narrow(((Wide)raw*b.raw)>>16));}
    Fix operator/(Fix b)const{return Raw(b.raw?Narrow(((Wide)raw*65536)/b.raw):0);}
    Fix& operator+=(Fix b){*this=*this+b;return *this;}
    Fix& operator-=(Fix b){*this=*this-b;return *this;}
    Fix& operator*=(Fix b){*this=*this*b;return *this;}
    bool operator<(Fix b)const{return raw<b.raw;}
    bool operator>(Fix b)const{return raw>b.raw;}
    bool operator<=(Fix b)const{return raw<=b.raw;}
    bool operator>=(Fix b)const{return raw>=b.raw;}
    bool operator==(Fix b)const{return raw==b.raw;}
    bool operator!=(Fix b)const{return raw!=b.raw;}
};
inline Fix operator+(int a,Fix b){return Fix(a)+b;}
inline Fix operator-(int a,Fix b){return Fix(a)-b;}
inline Fix operator*(int a,Fix b){return Fix(a)*b;}
inline Fix operator/(int a,Fix b){return Fix(a)/b;}
inline int FxInt(Fix a){return a.Int();}
inline int FxInt(int a){return a;}
inline Fix FxAbs(Fix a){return a.raw<0?-a:a;}
inline Fix FxMax(Fix a,Fix b){return a>b?a:b;}
inline int FxFloor(Fix a){return a.raw>=0?a.raw/65536:-1-(int)((-(Wide)a.raw-1)/65536);}
Fix FxSin(Fix radians);
Fix FxCos(Fix radians);
Fix FxTan(Fix radians);
Fix FxSqrt(Fix value);
Fix FxAtan2(Fix y,Fix x);
Fix FxWrapAngle(Fix radians);
#endif
