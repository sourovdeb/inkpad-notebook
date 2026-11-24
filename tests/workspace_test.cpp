#include "workspace_core.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace Workspace;
int main() {
    assert(validUtf8("Bonjour été €"));assert(!validUtf8(std::string("\xc0\xaf",2)));
    assert(!validUtf8(std::string("a\0b",3)));assert(!validUtf8("\xed\xa0\x80"));
    Editor e;e.load("Bonjour été\n");assert(e.text.back()=='\n');
    e.move(9);assert(e.cursor==8); // byte offset inside é snaps backwards
    e.move(e.text.size()-1);assert(e.erase(true));assert(e.text=="Bonjour ét\n");
    assert(e.undo());assert(e.text=="Bonjour été\n");assert(e.redo());assert(e.text=="Bonjour ét\n");
    e.load("été");e.anchor=0;e.cursor=e.text.size();assert(e.wrap("**"));assert(e.text=="**été**");assert(e.selected()=="été");
    assert(e.insert("bonjour"));assert(e.text=="**bonjour**");assert(e.undo());
    e.load("One\nTwo\n");e.move(5);assert(e.heading(2));assert(e.text=="One\n## Two\n");assert(e.heading(0));assert(e.text=="One\nTwo\n");
    e.move(0);assert(e.find("Two"));assert(e.selected()=="Two");assert(e.find("Two"));assert(!e.find("absent"));
    auto h=html("# Title\n**été** and *italic*\n<script>&\n","A<&",true);
    assert(h.find("<h1>Title</h1>")!=std::string::npos);assert(h.find("<strong>été</strong>")!=std::string::npos);
    assert(h.find("<em>italic</em>")!=std::string::npos);assert(h.find("<script>")==std::string::npos);assert(h.find("&lt;script&gt;&amp;")!=std::string::npos);
    assert(plain("# Title\n**été**\n",true)=="Title\nété\n");
    assert(plain("# literal **\n",false)=="# literal **\n");
    assert(plain("unmatched * keep",true)=="unmatched * keep");
    assert(html("***both***","test",true).find("<strong><em>both</em></strong>")!=std::string::npos);
    assert(!validName("../draft"));assert(!validName(".hidden"));assert(validName("été.md"));assert(!validName("a:b"));
    e.load(std::string(MAX_DOCUMENT,'x'));assert(!e.insert("x"));assert(e.text.size()==MAX_DOCUMENT);
    char dir[]="/tmp/pb-workspace-test-XXXXXX";assert(mkdtemp(dir));std::string path=std::string(dir)+"/draft.md",err,out;
    assert(saveDocument(path,"Bonjour été\n\n",err));assert(readDocument(path,out,err));assert(out=="Bonjour été\n\n");
    assert(!saveDocument(std::string(dir)+"/missing/draft","bad",err));assert(readDocument(path,out,err));assert(out=="Bonjour été\n\n");
    {std::ofstream f(path,std::ios::binary);f<<std::string(MAX_DOCUMENT+1,'x');}assert(!readDocument(path,out,err));
    unlink(path.c_str());rmdir(dir);
    std::cout<<"workspace core: UTF-8, edits, undo, selection, formatting, exports, size limit, atomic save OK\n";
}
