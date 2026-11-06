#ifndef DUMP_OWNED_NAME_H
#define DUMP_OWNED_NAME_H
// Match only generated 0.2.x metadata, never arbitrary folder contents.
template<class Text> bool IsDumpOwned(const Text& text){
    const char prefix[]="N6600_";
    int length=text.Length();if(length<22)return false;
    for(int i=0;i<6;++i)if(text[i]!=prefix[i])return false;
    for(int i=6;i<21;++i){if(i==14){if(text[i]!='_')return false;}else if(text[i]<'0'||text[i]>'9')return false;}
    const char* suffixes[]={"_manifest.tsv","_manifest.tsv.part","_transfer.log"};
    for(int s=0;s<3;++s){int n=0;while(suffixes[s][n])++n;if(length!=21+n)continue;
        bool equal=true;for(int i=0;i<n;++i)if(text[21+i]!=suffixes[s][i]){equal=false;break;}if(equal)return true;}
    return false;
}
#endif
