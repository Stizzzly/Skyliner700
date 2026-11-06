#include "owned_name.h"
#include <string>
#include <stdio.h>
#include <stdlib.h>
struct Text {std::string s;Text(const char* p):s(p){}int Length()const{return (int)s.size();}char operator[](int i)const{return s[i];}};
static void Check(const char* path,bool expected){if(IsDumpOwned(Text(path))!=expected){fprintf(stderr,"Wrong ownership: %s\n",path);exit(1);}}
int main(){
 const char* endings[]={"_manifest.tsv","_manifest.tsv.part","_transfer.log"};
 for(int i=0;i<3;++i){std::string base="N6600_20261102_123456";std::string name=base+endings[i];Check(name.c_str(),true);
  Check((name+".backup").c_str(),false);Check(("dir/"+name).c_str(),false);
  for(int n=0;n<21;++n){std::string bad=name;bad[n]='!';Check(bad.c_str(),false);}}
 const char* others[]={"notes.txt","N6600_20261102_123456_000001.bin","N6600_20261102_123456.zip","N6600_report.log","..",".","Z:\\system\\libs\\euser.dll","N6600_20261102_123456_manifest.tsv/../other"};
 for(unsigned i=0;i<sizeof(others)/sizeof(others[0]);++i)Check(others[i],false);
 puts("PASS: generated metadata ownership; all other filenames, suffixes and paths refused.");
}
