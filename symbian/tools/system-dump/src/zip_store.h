#ifndef SYSTEM_DUMP_ZIP_STORE_H
#define SYSTEM_DUMP_ZIP_STORE_H
#include <string.h>
#include "crc_table.h"
// Streaming ZIP32 STORE writer; IO errors propagate from Output. No allocation,
// compression or seek-back header patching. Central directory is spooled to disk.
template<class Output> class ZipStore {
    Output& out;Output& central;
    unsigned count,crc,expected,written,start;
    unsigned short nameLength,dosTime,dosDate;
    char name[1024];bool active,directory;
    unsigned char scratch[4096]; // member storage: avoid a 4 KiB EKA1 stack frame
    static void U16(Output& file,unsigned value){
        unsigned char b[2]={(unsigned char)value,(unsigned char)(value>>8)};file.Write(b,2);
    }
    static void U32(Output& file,unsigned value){U16(file,value);U16(file,value>>16);}
public:
    ZipStore(Output& a,Output& c):out(a),central(c),count(0),active(false){}
    bool Begin(const char* path,unsigned length,unsigned size,bool folder=false,unsigned time=0,unsigned date=33){
        if(active||count>=65535||!length||length>sizeof(name))return false;
        memcpy(name,path,length);nameLength=(unsigned short)length;
        expected=size;written=0;crc=0xffffffffu;directory=folder;
        dosTime=(unsigned short)time;dosDate=(unsigned short)date;start=out.Position();active=true;
        U32(out,0x04034b50);U16(out,20);U16(out,0x0808);U16(out,0);
        U16(out,dosTime);U16(out,dosDate);U32(out,0);U32(out,0);U32(out,0);
        U16(out,nameLength);U16(out,0);out.Write(name,nameLength);return true;
    }
    bool Data(const void* bytes,unsigned length){
        if(!active||length>expected-written)return false;
        const unsigned char* p=(const unsigned char*)bytes;
        for(unsigned i=0;i<length;++i)crc=ZipCrcTable[(crc^p[i])&255]^(crc>>8);
        out.Write(bytes,length);written+=length;return true;
    }
    bool End(){
        if(!active||written!=expected)return false;
        unsigned finalCrc=crc^0xffffffffu;
        U32(out,0x08074b50);U32(out,finalCrc);U32(out,written);U32(out,written);
        U32(central,0x02014b50);U16(central,20);U16(central,20);U16(central,0x0808);U16(central,0);
        U16(central,dosTime);U16(central,dosDate);U32(central,finalCrc);U32(central,written);U32(central,written);
        U16(central,nameLength);U16(central,0);U16(central,0);U16(central,0);U16(central,0);
        U32(central,directory?0x10:0);U32(central,start);central.Write(name,nameLength);
        ++count;active=false;return true;
    }
    void Abort(){if(active){out.Truncate(start);active=false;}}
    bool Finish(){
        if(active)return false;
        unsigned offset=out.Position(),length=central.Position();
        central.Rewind();unsigned remaining=length;
        while(remaining){unsigned n=central.Read(scratch,remaining>sizeof(scratch)?sizeof(scratch):remaining);
            if(!n)return false;out.Write(scratch,n);remaining-=n;}
        U32(out,0x06054b50);U16(out,0);U16(out,0);U16(out,count);U16(out,count);
        U32(out,length);U32(out,offset);U16(out,0);return true;
    }
    unsigned Count()const{return count;}
};
#endif
