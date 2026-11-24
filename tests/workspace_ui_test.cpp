#include <cassert>
#include <iostream>
#define WORKSPACE_TEST_ROOT "/tmp/pb-workspace-ui-test"
#define main workspace_device_main
#include "../src/workspace.cpp"
#undef main
static void type(WorkspaceApp &a,const string &s) {
    for(char c:s) {int code=0;for(auto &kv:a.keys.plain) if(kv.second==string(1,c)) code=kv.first;assert(code);a.key(code,1);a.key(code,0);}
}
int main(int argc,char **argv) {
    assert(argc==2);mkdir(ROOT.c_str(),0700);mkdir(CONFIG.c_str(),0700);
    std::ifstream map(argv[1]);std::ofstream copy(CONFIG+"/map.keys");copy<<map.rdbuf();copy.close();
    string error;assert(Workspace::saveDocument(ROOT+"/legacy.txt","Bonjour été\n",error));
    WorkspaceApp a;assert(a.home);assert(a.cards.size()>=2);
    a.open(ROOT+"/legacy.txt");assert(!a.home);a.editor.move(a.editor.text.size());type(a,"bonjour");assert(a.editor.text=="Bonjour été\nbonjour");assert(a.save());
    a.command(401);assert(markdown(a.path));assert(a.editor.text=="Bonjour été\n# bonjour");assert(exists(ROOT+"/legacy.txt"));
    a.editor.move(a.editor.text.size());a.command(405);assert(a.editor.selected()=="text");type(a,"ami");assert(a.editor.text.find("**ami**")!=string::npos);assert(a.save());
    string out=a.exportFile(".html"),htmlText;assert(!out.empty());assert(Workspace::readDocument(out,htmlText,error));assert(htmlText.find("<h1>")!=string::npos);assert(htmlText.find("<strong>ami</strong>")!=string::npos);
    a.preview();assert(!Mock::book.empty());assert(exists(Mock::book));
    // Pointer selection, long-press context menu, menu close, styles and JSON export.
    const string savedBeforeTouch=a.saved;
    a.editor.load("hello world");a.layoutDirty=true;a.paint(true);
    a.event(EVT_POINTERDOWN,40,a.top+2);a.event(EVT_POINTERLONG,40,a.top+2);
    assert(a.modal);assert(a.editor.selected()=="hello");a.command(900);assert(!a.modal);
    a.command(204);assert(a.clipboard=="hello");
    a.editor.move(0);a.event(EVT_POINTERDOWN,40,a.top+2);a.event(EVT_POINTERMOVE,130,a.top+2);a.event(EVT_POINTERUP,130,a.top+2);assert(!a.editor.selected().empty());
    a.command(310);assert(a.pageStyle==PAGE_RULED);a.command(318);assert(a.pageColor==0xfff8e7);a.command(319);assert(a.pageColor==0xe4efff);a.command(311);assert(a.pageStyle==PAGE_GRID);a.command(322);assert(a.inkColor==WHITE);a.command(317);assert(a.pageStyle==PAGE_WHITE&&a.pageColor==WHITE&&a.inkColor==BLACK);
    string json=a.exportFile(".json"),jsonText;assert(Workspace::readDocument(json,jsonText,error));assert(jsonText.find("\"content\"")!=string::npos&&jsonText.find("hello world")!=string::npos);
    a.showMenu(0);assert(a.modal);a.command(900);assert(!a.modal);
    a.editor.load(savedBeforeTouch);a.layoutDirty=true;
    a.editor.move(0);a.key(KEY_RIGHT,1);assert(a.editor.cursor==1);a.key(KEY_LEFTSHIFT,1);a.key(KEY_RIGHT,1);a.key(KEY_LEFTSHIFT,0);assert(a.editor.selected()=="o");
    a.key(KEY_BACKSPACE,1);assert(a.editor.text.substr(0,6)=="Bnjour");a.command(201);assert(a.editor.text.substr(0,7)=="Bonjour");
    a.command(305);assert(a.width==1872);a.command(304);assert(a.width==1404);
    a.showMenu(3);assert(a.modal);assert(a.menuItems[0].index==401);Mock::menu(0);assert(!a.modal);
    a.editor.insert("!");a.changed(true);a.lastEdit=nowMs()-3000;a.tick();assert(a.editor.text==a.saved);
    string before=a.editor.text;a.event(EVT_HIDE,0,0);type(a,"x");assert(a.editor.text==before);a.event(EVT_SHOW,0,0);
    // External edits cannot silently be overwritten.
    assert(Workspace::saveDocument(a.path,"external",error));a.editor.insert("mine");assert(!a.save(false));assert(a.saveFailed);
    a.promptAction=104;char newName[]="recovered.md";a.acceptPrompt(newName);assert(a.path==ROOT+"/recovered.md");assert(a.editor.text==a.saved);
    a.goHome();assert(a.home);a.command(101);char collision[]="legacy.txt";WorkspaceApp::keyboardCallback(collision);string original;assert(Workspace::readDocument(ROOT+"/legacy.txt",original,error));assert(original=="Bonjour été\nbonjour");
    // Every painted box stays within portrait and landscape bounds.
    for(int orientation: {0,2}) {a.command(orientation==0?304:305);for(const auto&r:a.cards) assert(r.x>=0&&r.y>=0&&r.x+r.w<=a.width&&r.y+r.h<=a.bottom);}
    a.open(ROOT+"/recovered.md");
    string longText;for(int i=0;i<1000;++i) longText+="Bonjour été. ";
    a.editor.load(longText);a.layoutDirty=true;a.buildRows();size_t position=0;
    for(auto r:a.rows) {assert(r.first==position);assert(r.last>=r.first);position=r.last;}assert(position==longText.size());
    a.editor.move(longText.size());a.followCursor=true;a.paint(false);assert(a.firstRow>0);
    a.editor.load(a.saved); // Leave fixture unchanged after layout checks.
    std::cout<<"workspace UI test double: keyboard, cards, formatting, preview dispatch, selection, rotation, autosave, external-conflict recovery OK\n";
}
