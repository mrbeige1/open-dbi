// keyset.cpp - see keyset.h. Clean-room prod.keys parser.
#include "keyset.h"
#include <cstdio>
#include <cctype>
#include <cstring>

namespace dbi::crypto {
namespace {
int hexNib(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
// Decode hex into out; returns bytes decoded (up to maxOut).
size_t hexDecode(const std::string& s, uint8_t* out, size_t maxOut){
    size_t n=0; for(size_t i=0;i+1<s.size() && n<maxOut;i+=2){ int hi=hexNib(s[i]),lo=hexNib(s[i+1]); if(hi<0||lo<0)break; out[n++]=(uint8_t)((hi<<4)|lo);} return n;
}
std::string trim(const std::string& s){ size_t a=0,b=s.size(); while(a<b&&std::isspace((unsigned char)s[a]))++a; while(b>a&&std::isspace((unsigned char)s[b-1]))--b; return s.substr(a,b-a); }
// If `name` is "<prefix>_XX", return gen XX and strip; else -1.
int genSuffix(std::string& name, const char* prefix){
    size_t pl=std::strlen(prefix);
    if(name.size()==pl+3 && name.compare(0,pl,prefix)==0 && name[pl]=='_'){
        int hi=hexNib(name[pl+1]),lo=hexNib(name[pl+2]); if(hi>=0&&lo>=0) return (hi<<4)|lo;
    }
    return -1;
}
} // namespace

int Keyset::parse(const std::string& text){
    int count=0; size_t pos=0;
    while(pos<=text.size()){
        size_t nl=text.find('\n',pos);
        std::string line=trim(text.substr(pos,(nl==std::string::npos?text.size():nl)-pos));
        pos=(nl==std::string::npos)?text.size()+1:nl+1;
        if(line.empty()||line[0]=='#'||line[0]==';')continue;
        size_t eq=line.find('=');
        if(eq==std::string::npos)continue;
        std::string name=trim(line.substr(0,eq)), val=trim(line.substr(eq+1));
        int g;
        if(name=="header_key"){ if(hexDecode(val,header_key,32)==32){has_header_key=true;++count;} continue; }
        if((g=genSuffix(name,"key_area_key_application"))>=0 && g<MAX_GEN){ if(hexDecode(val,kak_application[g],16)==16)++count; continue; }
        if((g=genSuffix(name,"key_area_key_ocean"))>=0 && g<MAX_GEN){ if(hexDecode(val,kak_ocean[g],16)==16)++count; continue; }
        if((g=genSuffix(name,"key_area_key_system"))>=0 && g<MAX_GEN){ if(hexDecode(val,kak_system[g],16)==16)++count; continue; }
        if((g=genSuffix(name,"titlekek"))>=0 && g<MAX_GEN){ if(hexDecode(val,titlekek[g],16)==16)++count; continue; }
        if((g=genSuffix(name,"master_key"))>=0 && g<MAX_GEN){ if(hexDecode(val,master_key[g],16)==16)++count; continue; }
    }
    return count;
}

int Keyset::load(const char* path){
    std::FILE* f=std::fopen(path,"rb"); if(!f)return -1;
    std::fseek(f,0,SEEK_END); long sz=std::ftell(f); std::fseek(f,0,SEEK_SET);
    std::string buf; buf.resize(sz>0?(size_t)sz:0);
    if(sz>0) { size_t rd = std::fread(&buf[0],1,(size_t)sz,f); buf.resize(rd); }
    std::fclose(f);
    return parse(buf);
}

} // namespace dbi::crypto
