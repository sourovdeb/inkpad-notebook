#include "workspace_core.h"
#include <fstream>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
namespace Workspace {
bool validUtf8(const std::string &s) {
    for (size_t i=0; i<s.size();) {
        unsigned char c=s[i++]; unsigned int v; int n;
        if (c<0x80) { if (!c) return false; continue; }
        if (c>=0xc2 && c<=0xdf) {v=c&31;n=1;}
        else if(c>=0xe0 && c<=0xef) {v=c&15;n=2;}
        else if(c>=0xf0 && c<=0xf4) {v=c&7;n=3;}
        else return false;
        int count=n;
        while(n--) { if(i==s.size()) return false; c=s[i++]; if((c&0xc0)!=0x80) return false; v=(v<<6)|(c&63); }
        if ((count==2 && v<0x800)||(count==3 && v<0x10000)||v>0x10ffff||(v>=0xd800 && v<=0xdfff)) return false;
    }
    return true;
}
size_t previous(const std::string &s,size_t p) { p=std::min(p,s.size()); if(p) --p; while(p && (static_cast<unsigned char>(s[p])&0xc0)==0x80) --p; return p; }
size_t next(const std::string &s,size_t p) { if(p<s.size()) ++p; while(p<s.size() && (static_cast<unsigned char>(s[p])&0xc0)==0x80) ++p; return p; }
bool validName(const std::string &s) {
    if(s.empty()||s.size()>120||s=="."||s==".."||s[0]=='.'||s.back()==' '||s.back()=='.'||!validUtf8(s)) return false;
    for(unsigned char c:s) if(c<32||c==127||std::string("/\\:*?\"<>|").find(c)!=std::string::npos) return false;
    return true;
}
bool readDocument(const std::string &path,std::string &out,std::string &error) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in) {error="Cannot open document";return false;}
    auto n=in.tellg();
    if(n<0||n>static_cast<std::streamoff>(MAX_DOCUMENT)) {error="Document exceeds 512 KiB";return false;}
    out.resize(static_cast<size_t>(n)); in.seekg(0);
    if(n && !in.read(&out[0],n)) {error="Cannot read document";return false;}
    if(!validUtf8(out)) {error="Open UTF-8 text only";return false;}
    return true;
}
bool saveDocument(const std::string &path,const std::string &text,std::string &error) {
    std::string pattern=path+".XXXXXX"; std::vector<char> tmp(pattern.begin(),pattern.end()); tmp.push_back(0);
    int fd=mkstemp(tmp.data()); if(fd<0) {error=std::strerror(errno);return false;}
    bool good=true; size_t p=0;
    while(p<text.size()) { ssize_t n=write(fd,text.data()+p,text.size()-p); if(n<0 && errno==EINTR) continue; if(n<=0) {good=false;break;} p+=static_cast<size_t>(n); }
    if(good && fsync(fd)!=0) good=false;
    if(close(fd)!=0) good=false;
    if(good && rename(tmp.data(),path.c_str())!=0) good=false;
    if(!good) {error="Save failed; draft remains in memory";unlink(tmp.data());return false;}
    // Directory sync is best-effort: FAT implementations may reject fsync.
    const auto slash=path.find_last_of('/');
    std::string parent=slash==std::string::npos?".":path.substr(0,slash);
    int dir=open(parent.c_str(),O_RDONLY|O_DIRECTORY); if(dir>=0) {fsync(dir);close(dir);}
    return true;
}
void Editor::load(const std::string &s) {text=s;cursor=anchor=s.size();history.clear();future.clear();}
void Editor::remember() {if(history.size()==16) history.erase(history.begin());history.push_back({text,cursor,anchor});future.clear();}
void Editor::move(size_t p,bool extend) {cursor=std::min(p,text.size());while(cursor<text.size() && (static_cast<unsigned char>(text[cursor])&0xc0)==0x80) --cursor;if(!extend) anchor=cursor;}
bool Editor::insert(const std::string &s) {
    if(!validUtf8(s)||text.size()-(high()-low())+s.size()>MAX_DOCUMENT) return false;
    remember();size_t p=low();text.replace(p,high()-p,s);cursor=anchor=p+s.size();return true;
}
bool Editor::erase(bool backwards) {
    if(cursor==anchor) {if(backwards) anchor=previous(text,cursor);else anchor=next(text,cursor);}
    if(cursor==anchor) return false;
    return insert("");
}
bool Editor::undo() {if(history.empty()) return false;future.push_back({text,cursor,anchor});auto s=history.back();history.pop_back();text=s.text;cursor=s.cursor;anchor=s.anchor;return true;}
bool Editor::redo() {if(future.empty()) return false;history.push_back({text,cursor,anchor});auto s=future.back();future.pop_back();text=s.text;cursor=s.cursor;anchor=s.anchor;return true;}
bool Editor::wrap(const std::string &m) {
    size_t p=low();std::string value=selected();if(value.empty()) value="text";
    if(value.size()>=2*m.size() && value.compare(0,m.size(),m)==0 && value.compare(value.size()-m.size(),m.size(),m)==0) return insert(value.substr(m.size(),value.size()-2*m.size()));
    if(!insert(m+value+m)) return false;
    anchor=p+m.size();cursor=anchor+value.size();return true;
}
bool Editor::heading(int level) {
    size_t start=cursor?text.rfind('\n',cursor-1):std::string::npos;
    start=start==std::string::npos?0:start+1; size_t end=start;
    while(end<text.size() && text[end]=='#' && end-start<6) ++end;
    if(end<text.size() && text[end]==' ' && end>start) ++end; else end=start;
    std::string prefix=level?std::string(level,'#')+" ":"";
    if(text.size()-(end-start)+prefix.size()>MAX_DOCUMENT) return false;
    remember();text.replace(start,end-start,prefix);cursor=anchor=start+prefix.size();return true;
}
bool Editor::find(const std::string &term) {if(term.empty()) return false;size_t p=text.find(term,high());if(p==std::string::npos) p=text.find(term);if(p==std::string::npos) return false;anchor=p;cursor=p+term.size();return true;}
static std::string escaped(const std::string &s) {std::string out;for(char c:s) {switch(c) {case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;case '"':out+="&quot;";break;default:out+=c;}}return out;}
static void inlineSpans(const std::string &s,int style,std::vector<Span> &out,int depth=0) {
    if(depth>4) {out.push_back({s,style});return;}
    size_t p=0,start=0;
    while(p<s.size()) {
        if(s[p]!='*') {++p;continue;}
        std::string mark=s.compare(p,3,"***")==0?"***":s.compare(p,2,"**")==0?"**":"*";
        size_t end=s.find(mark,p+mark.size());
        if(end==std::string::npos||end==p+mark.size()) {p+=mark.size();continue;}
        if(p>start) out.push_back({s.substr(start,p-start),style});
        inlineSpans(s.substr(p+mark.size(),end-p-mark.size()),style|(mark.size()==3?3:mark.size()==2?1:2),out,depth+1);
        p=end+mark.size();start=p;
    }
    if(start<s.size()) out.push_back({s.substr(start),style});
}
std::vector<Block> parseMarkdown(const std::string &text) {
    std::vector<Block> blocks;size_t start=0;
    do {
        size_t end=text.find('\n',start);std::string line=text.substr(start,end==std::string::npos?end:end-start);
        if(!line.empty() && line.back()=='\r') line.pop_back();
        int h=0;while(h<3 && static_cast<size_t>(h)<line.size() && line[h]=='#') ++h;
        if(h && line.size()>static_cast<size_t>(h) && line[h]==' ') line.erase(0,h+1);else h=0;
        Block b{h,{}};inlineSpans(line,0,b.spans);blocks.push_back(b);
        if(end==std::string::npos) break;
        start=end+1;
    } while(start<=text.size());
    return blocks;
}
std::string plain(const std::string &text,bool markdown) {
    if(!markdown) return text;
    std::string out;bool first=true;
    for(const auto &b:parseMarkdown(text)) {if(!first) out+='\n';first=false;for(const auto &s:b.spans) out+=s.text;}
    return out;
}
std::string html(const std::string &text,const std::string &title,bool markdown) {
    std::string out="<!doctype html>\n<html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>"+escaped(title)+"</title><style>body{font-family:serif;max-width:42em;margin:2em auto;padding:0 1em;line-height:1.55;color:#000;background:#fff}p{white-space:pre-wrap;min-height:1em}h1{font-size:2em}h2{font-size:1.6em}h3{font-size:1.3em}pre{white-space:pre-wrap;overflow-wrap:anywhere}</style></head><body>\n";
    if(!markdown) out+="<pre>"+escaped(text)+"</pre>\n";
    else for(const auto &b:parseMarkdown(text)) {
        std::string tag=b.heading?"h"+std::to_string(b.heading):"p";out+="<"+tag+">";
        for(const auto &s:b.spans) {if(s.style&1) out+="<strong>";if(s.style&2) out+="<em>";out+=escaped(s.text);if(s.style&2) out+="</em>";if(s.style&1) out+="</strong>";}
        out+="</"+tag+">\n";
    }
    return out+"</body></html>\n";
}
}
