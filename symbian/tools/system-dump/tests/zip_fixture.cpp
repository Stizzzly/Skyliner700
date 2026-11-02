#include "zip_store.h"
#include <vector>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
struct MemoryOutput {
    std::vector<unsigned char> bytes;unsigned readPosition;int fail;
    MemoryOutput():readPosition(0),fail(0){}
    void Write(const void* p,unsigned n){if(fail)throw fail;const unsigned char* b=(const unsigned char*)p;bytes.insert(bytes.end(),b,b+n);}
    unsigned Position()const{return (unsigned)bytes.size();}
    void Truncate(unsigned n){bytes.resize(n);}
    void Rewind(){readPosition=0;}
    unsigned Read(void* data,unsigned size){unsigned n=(unsigned)bytes.size()-readPosition;if(n>size)n=size;
        if(n)memcpy(data,&bytes[readPosition],n);readPosition+=n;return n;}
};
static void Check(bool ok){if(!ok){fprintf(stderr,"ZIP fixture assertion failed\n");exit(1);}}
static void Add(ZipStore<MemoryOutput>& zip,const char* name,const void* data,unsigned size,bool dir=false){
    Check(zip.Begin(name,(unsigned)strlen(name),size,dir));
    const unsigned char* p=(const unsigned char*)data;
    for(unsigned i=0;i<size;){unsigned n=size-i;if(n>7919)n=7919;Check(zip.Data(p+i,n));i+=n;}
    Check(zip.End());
}
int main(int argc,char** argv){
    Check(argc==2);MemoryOutput out,central;ZipStore<MemoryOutput> zip(out,central);
    std::vector<unsigned char> payload(300001);
    for(unsigned i=0;i<payload.size();++i)payload[i]=(unsigned char)((i*31u+(i>>8))&255);
    Add(zip,"Z/system/empty/",0,0,true);Add(zip,"Z/system/zero.bin",0,0);
    Add(zip,"Z/system/pattern.dll",&payload[0],(unsigned)payload.size());
    Add(zip,"Z/system/\xd1\x82\xd0\xb5\xd1\x81\xd1\x82.bin","123456789",9);
    Check(zip.Begin("Z/failed.dll",12,100));Check(zip.Data("incomplete",10));Check(!zip.End());
    Check(!zip.Finish());zip.Abort();
    Add(zip,"Z/after.bin","end",3);
    char longPath[801];memset(longPath,'a',800);longPath[800]=0;Add(zip,longPath,"long",4);
    Check(zip.Count()==6&&zip.Finish());
    std::ofstream file(argv[1],std::ios::binary);file.write((const char*)&out.bytes[0],out.bytes.size());file.close();Check(bool(file));
    MemoryOutput a,b;ZipStore<MemoryOutput> invalid(a,b);
    char oversized[1025];memset(oversized,'x',sizeof(oversized));Check(!invalid.Begin(oversized,sizeof(oversized),0));
    Check(a.Position()==0&&invalid.Begin("x",1,1));Check(!invalid.Begin("y",1,1));Check(!invalid.Data("xx",2));invalid.Abort();Check(a.Position()==0);
    a.fail=71;bool caught=false;try{invalid.Begin("x",1,1);}catch(int e){caught=(e==71);}Check(caught);
    puts("PASS: streaming ZIP, UTF-8/long names, directories, zero files, chunked data, abort/rollback, size limits and IO error propagation.");
}
