// InkPad Notebook 0.3.0. GPL-3.0, alongside the retained upstream code.
// PB743K3: same native extended-key events and keymap as the proven 0.1 build.
#include "inkview.h"
#include "keymap_parse.h"
#include "workspace_core.h"
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using std::string;
using Workspace::Editor;

// These functions exist in newer InkView builds. Weak references keep the
// app loadable on older firmware where the file-transfer helpers are absent.
extern "C" {
    char **EnumBTdevices() __attribute__((weak));
    void OpenBTdevicesMenu(char *title, int x, int y,
                           iv_itemchangehandler handler) __attribute__((weak));
    int BtSendFiles(char *mac, char **files) __attribute__((weak));
}
#ifndef WORKSPACE_TEST_ROOT
static const string ROOT="/mnt/ext1/textEditor";
static const string CONFIG="/mnt/ext1/system/config/textEditor";
#else
static const string ROOT=WORKSPACE_TEST_ROOT;
static const string CONFIG=ROOT+"/config";
#endif
static long long nowMs() {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static string base(const string &s) {return s.substr(s.find_last_of('/')+1);}
static string ext(const string &s) {auto p=s.find_last_of('.');string e=p==string::npos?"":s.substr(p);for(char &c:e) if(c>='A'&&c<='Z') c+=32;return e;}
static string stem(const string &s) {auto p=s.find_last_of('.');return p==string::npos?s:s.substr(0,p);}
static bool markdown(const string &s) {return ext(s)==".md"||ext(s)==".markdown";}
static bool exists(const string &s) {struct stat st;return lstat(s.c_str(),&st)==0;}
static string uniquePath(const string &dir,const string &name) {string p=dir+"/"+name;for(int i=2;exists(p);++i) p=dir+"/"+stem(name)+"-"+std::to_string(i)+ext(name);return p;}
static string stamp() {char s[32];time_t t=time(nullptr);strftime(s,sizeof(s),"%Y-%m-%d",localtime(&t));return s;}
struct Rect {int x,y,w,h;bool contains(int a,int b) const {return a>=x && b>=y && a<x+w && b<y+h;}};
struct Entry {string name,path;bool folder;time_t modified;off_t bytes;};
struct Row {size_t first,last;};

enum PageStyle { PAGE_WHITE = 0, PAGE_RULED = 1, PAGE_GRID = 2, PAGE_DARK = 3 };

static bool isWordByte(unsigned char c)
{
    return c >= 128 || std::isalnum(c) || c == '_' || c == '-';
}

static int parseColor(const string &raw, int fallback)
{
    if (raw.size() != 6)
        return fallback;
    char *end = nullptr;
    const long value = std::strtol(raw.c_str(), &end, 16);
    if (!end || *end != '\0' || value < 0 || value > 0xffffff)
        return fallback;
    return static_cast<int>(value);
}

static string colorText(int color)
{
    char value[8];
    std::snprintf(value, sizeof(value), "%06x", color & 0xffffff);
    return value;
}

static string jsonEscape(const string &value)
{
    string out;
    for (unsigned char c : value)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 32)
                {
                    char escaped[7];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                    out += escaped;
                }
                else
                    out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

class WorkspaceApp {
public:
    Editor editor;
    ParsedKeymap keys;
    string directory=ROOT,path,saved,clipboard,dead,status="Documents";
    std::vector<Entry> entries;
    std::vector<Row> rows;
    std::vector<Rect> cards;
    std::vector<imenu> menuItems;
    std::vector<string> menuText;
    std::map<string,int> widths;
    ifont *body=nullptr,*label=nullptr,*title=nullptr;
    int width=0,height=0,toolbar=0,top=0,bottom=0,line=0,fontSize=44;
    int gridPage=0,gridCount=1,gridColumns=1,gridSelection=0,firstRow=0;
    int repaintCount=0,promptAction=0,lockFd=-1;
    bool home=true,modal=false,hidden=false,paintPending=false,layoutDirty=true,followCursor=true;
    bool leftShift=false,rightShift=false,leftCtrl=false,rightCtrl=false,altgr=false,caps=false;
    bool saveFailed=false;
    int pageStyle=PAGE_WHITE;
    int pageColor=WHITE, inkColor=BLACK, accentColor=0x38566f;
    int ruleColor=0xc7d0d8, selectionColor=0xb4c9d8;
    bool touchActive=false,touchDragging=false,touchLong=false;
    int touchDownX=0,touchDownY=0;
    size_t touchDownPosition=0;
    string touchPendingSelection;
    std::vector<string> pendingBluetoothFiles;
    char bluetoothTitle[64]={0};
    char keyboard[256]={0};
    long long lastEdit=0,lastSave=0;
    static WorkspaceApp *app;
    static void paintCallback() {if(app) {app->paintPending=false;if(!app->modal&&!app->hidden) app->paint(false);}}
    static void tickCallback() {if(app) app->tick();}
    static void menuCallback(int action) {if(app) {app->modal=false;app->paint(true);app->command(action);}}
    static void keyboardCallback(char *s) {if(app) {app->modal=false;app->acceptPrompt(s);}}
    static void bluetoothCallback(char *device) {if(app) app->sendBluetoothNow(device);}

    WorkspaceApp() {
        app=this;SetPanelType(0);
        mkdir(ROOT.c_str(),0777);mkdir(CONFIG.c_str(),0777);
        lockFd=::open("/tmp/pb-typewriter-workspace.lock",O_CREAT|O_RDWR,0600);
        if(lockFd>=0 && flock(lockFd,LOCK_EX|LOCK_NB)!=0) {close(lockFd);lockFd=-1;Message(ICON_INFORMATION,"InkPad Notebook","InkPad Notebook is already running.",2000);CloseApp();return;}
        std::ifstream map(CONFIG+"/map.keys");parseKeymap(map,keys);
        loadPreferences();
        if(keys.plain.empty()) status="Keymap missing: see Help";
        if(!IsBluetoothEnabled()) SetBluetoothOn();
        if(!IsBluetoothAwake()) BluetoothWakeUp();
        scan();metrics();paint(true);SetWeakTimer("workspace-save",tickCallback,1000);
    }
    ~WorkspaceApp() {
        ClearTimer(paintCallback);ClearTimer(tickCallback);
        if(!home && editor.text!=saved) save(false);
        savePreferences();
        closeFonts();if(lockFd>=0) close(lockFd);app=nullptr;
    }
    void closeFonts() {if(body) CloseFont(body);if(label) CloseFont(label);if(title) CloseFont(title);body=label=title=nullptr;}
    void loadPreferences()
    {
        std::ifstream in(CONFIG+"/workspace.conf");
        bool savedPageColor=false,savedInkColor=false,savedRuleColor=false,savedSelectionColor=false;
        int requestedStyle=pageStyle;
        string line;
        while(std::getline(in,line))
        {
            const size_t split=line.find('=');
            if(split==string::npos) continue;
            const string key=line.substr(0,split), value=line.substr(split+1);
            if(key=="page_style") requestedStyle=std::max(0,std::min(3,std::atoi(value.c_str())));
            else if(key=="page_color") {pageColor=parseColor(value,pageColor);savedPageColor=true;}
            else if(key=="ink_color") {inkColor=parseColor(value,inkColor);savedInkColor=true;}
            else if(key=="accent_color") accentColor=parseColor(value,accentColor);
            else if(key=="rule_color") {ruleColor=parseColor(value,ruleColor);savedRuleColor=true;}
            else if(key=="selection_color") {selectionColor=parseColor(value,selectionColor);savedSelectionColor=true;}
            else if(key=="font_size") fontSize=std::max(28,std::min(72,std::atoi(value.c_str())));
        }
        pageStyle=requestedStyle;
        const int savedPage=pageColor,savedInk=inkColor,savedRule=ruleColor,savedSelection=selectionColor;
        applyPageStyle(pageStyle);
        if(savedPageColor) pageColor=savedPage;
        if(savedInkColor) inkColor=savedInk;
        if(savedRuleColor) ruleColor=savedRule;
        if(savedSelectionColor) selectionColor=savedSelection;
    }
    void savePreferences()
    {
        string value="page_style="+std::to_string(pageStyle)+"\n";
        value+="page_color="+colorText(pageColor)+"\n";
        value+="ink_color="+colorText(inkColor)+"\n";
        value+="accent_color="+colorText(accentColor)+"\n";
        value+="rule_color="+colorText(ruleColor)+"\n";
        value+="selection_color="+colorText(selectionColor)+"\n";
        value+="font_size="+std::to_string(fontSize)+"\n";
        string error;
        Workspace::saveDocument(CONFIG+"/workspace.conf",value,error);
    }
    void applyPageStyle(int style)
    {
        pageStyle=std::max(0,std::min(3,style));
        if(pageStyle==PAGE_DARK)
        {
            pageColor=0x101820;
            inkColor=0xe7edf2;
            ruleColor=0x334455;
            selectionColor=0x365b74;
        }
        else if(pageStyle==PAGE_WHITE)
        {
            pageColor=WHITE;
            inkColor=BLACK;
            ruleColor=0xc7d0d8;
            selectionColor=0xb4c9d8;
        }
        layoutDirty=true;
    }
    int selectedInk() const
    {
        const int red=(pageColor>>16)&0xff;
        const int green=(pageColor>>8)&0xff;
        const int blue=pageColor&0xff;
        return (red*299+green*587+blue*114)<128000 ? WHITE : BLACK;
    }
    void setInk(int color)
    {
        inkColor=color;
        layoutDirty=true;
        savePreferences();
        paint(true);
    }
    void setBackground(int color)
    {
        pageColor=color;
        if((color&0xffffff)==0x101820)
        {
            pageStyle=PAGE_DARK;
            if(inkColor==BLACK) inkColor=0xe7edf2;
        }
        else if(pageStyle==PAGE_DARK)
        {
            pageStyle=PAGE_WHITE;
            if(inkColor==0xe7edf2) inkColor=BLACK;
        }
        layoutDirty=true;
        savePreferences();
        paint(true);
    }
    void metrics() {
        width=ScreenWidth();height=ScreenHeight();closeFonts();widths.clear();
        body=OpenFont("LiberationMono",fontSize,FONT_STD);
        label=OpenFont("Roboto",std::max(28,width/44),FONT_STD);
        title=OpenFont("Roboto",std::max(34,width/36),FONT_BOLD);
        toolbar=width<1050?160:80;top=toolbar+100;bottom=height-100;line=fontSize+16;
        layoutDirty=true;firstRow=0;followCursor=true;
    }
    void text(const Rect &r,const string &s,ifont *font,int flags=ALIGN_LEFT) {
        SetFont(font,inkColor);DrawTextRect(r.x,r.y,r.w,r.h,s.c_str(),flags);
    }
    void box(const Rect &r,const string &s,ifont *font) {DrawRect(r.x,r.y,r.w,r.h,accentColor);text({r.x+10,r.y+10,r.w-20,r.h-20},s,font,ALIGN_CENTER);}
    void scan() {
        entries.clear();DIR *dir=opendir(directory.c_str());
        if(!dir) {status="Cannot open folder";return;}
        while(dirent *d=readdir(dir)) {
            string n=d->d_name;if(n.empty()||n[0]=='.') continue;
            struct stat st;string p=directory+"/"+n;if(lstat(p.c_str(),&st)!=0) continue;
            string e=ext(n);bool folder=S_ISDIR(st.st_mode);
            if(folder || (S_ISREG(st.st_mode) && (e.empty()||e==".txt"||e==".md"||e==".markdown"||e==".html"))) entries.push_back({n,p,folder,st.st_mtime,st.st_size});
        }
        closedir(dir);
        std::sort(entries.begin(),entries.end(),[](const Entry&a,const Entry&b){if(a.folder!=b.folder) return a.folder>b.folder;if(a.modified!=b.modified) return a.modified>b.modified;return a.name<b.name;});
        gridPage=0;gridSelection=0;
    }
    void schedulePaint() {if(!paintPending&&!modal&&!hidden) {paintPending=true;SetWeakTimer("workspace-paint",paintCallback,100);}}
    void changed(bool changedText) {
        if(changedText) {lastEdit=nowMs();saveFailed=false;status="Saving...";layoutDirty=true;}
        followCursor=true;schedulePaint();
    }
    void tick() {
        if(!home && editor.text!=saved && !saveFailed && (nowMs()-lastEdit>=2000||nowMs()-lastSave>=5000)) {save(false);schedulePaint();}
        SetWeakTimer("workspace-save",tickCallback,1000);
    }
    bool save(bool notify=true) {
        if(home||path.empty()) return true;
        if(editor.text==saved) {status="Saved";return true;}
        string disk,error;
        if(!Workspace::readDocument(path,disk,error)||disk!=saved) {status="File changed outside app. Save copy.";saveFailed=true;if(notify) Message(ICON_ERROR,"Save",status.c_str(),3000);return false;}
        if(!Workspace::saveDocument(path,editor.text,error)) {status=error;saveFailed=true;if(notify) Message(ICON_ERROR,"Save",error.c_str(),3000);return false;}
        saved=editor.text;lastSave=nowMs();status="Saved";saveFailed=false;return true;
    }
    void goHome() {
        if(!save()) return;
        home=true;dead.clear();path.clear();status="Documents";scan();paint(true);
    }
    void open(const string &p,bool read=false) {
        if(!home&&!save()) return;
        if(ext(p)==".html") {OpenBook(p.c_str(),nullptr,0);return;}
        string value,error;if(!Workspace::readDocument(p,value,error)) {Message(ICON_ERROR,"Open",error.c_str(),2500);return;}
        path=p;editor.load(value);editor.move(0);saved=value;home=false;firstRow=0;layoutDirty=true;followCursor=true;status="Saved";lastSave=nowMs();saveFailed=false;
        paint(true);if(read) preview();
    }
    int glyphWidth(const string &s) {auto it=widths.find(s);if(it!=widths.end()) return it->second;SetFont(body,inkColor);int v=std::max(1,StringWidth(s.c_str()));widths[s]=v;return v;}
    void buildRows() {
        if(!layoutDirty) return;
        rows.clear();size_t start=0,p=0,lastBreak=0;int x=0;int available=width-80;
        while(p<editor.text.size()) {
            size_t q=Workspace::next(editor.text,p);string g=editor.text.substr(p,q-p);
            if(g=="\n") {rows.push_back({start,p});start=q;lastBreak=q;x=0;p=q;continue;}
            int gw=g=="\r"?0:glyphWidth(g=="\t"?"    ":g);
            if(x+gw>available && p>start) {
                size_t split=lastBreak>start?lastBreak:p;
                rows.push_back({start,split});start=split;p=split;lastBreak=split;x=0;continue;
            }
            if(g==" "||g=="\t") lastBreak=q;
            x+=gw;p=q;
        }
        rows.push_back({start,editor.text.size()});layoutDirty=false;
    }
    int cursorRow() {
        buildRows();for(size_t i=0;i<rows.size();++i) {if(editor.cursor<rows[i].last || (editor.cursor==rows[i].last && (i+1==rows.size()||rows[i+1].first!=editor.cursor))) return static_cast<int>(i);}
        return static_cast<int>(rows.size())-1;
    }
    int visibleRows() const {return std::max(1,(bottom-top)/line);}
    int xAt(const Row&r,size_t pos) {int x=40;for(size_t p=r.first;p<std::min(pos,r.last);) {size_t q=Workspace::next(editor.text,p);string g=editor.text.substr(p,q-p);if(g!="\r") x+=glyphWidth(g=="\t"?"    ":g);p=q;}return x;}
    size_t positionAt(const Row&r,int x) {int px=40;for(size_t p=r.first;p<r.last;) {size_t q=Workspace::next(editor.text,p);string g=editor.text.substr(p,q-p);int gw=g=="\r"?0:glyphWidth(g=="\t"?"    ":g);if(x<px+gw/2) return p;px+=gw;p=q;}return r.last;}
    void paint(bool full) {
        if(hidden||modal) return;
        if(ScreenWidth()!=width||ScreenHeight()!=height) metrics();
        FillArea(0,0,width,height,pageColor);
        const char *labels[]={"File","Edit","View","Format","Insert","Help"};int cols=toolbar==160?3:6;
        for(int i=0;i<6;++i) box({(i%cols)*width/cols,(i/cols)*80,width/cols,80},labels[i],label);
        text({30,toolbar+12,width-60,50},home?"Documents / "+base(directory):base(path),title);
        text({30,toolbar+60,width-60,35},home?status:status+"  |  "+(markdown(path)?"Markdown":"TXT")+"  |  "+std::to_string(editor.text.size())+" bytes",label);
        if(home) paintGrid();else paintEditor();
        const char *footHome[]={"Up","Previous","Next","New"};const char *footEdit[]={"Home","Previous","Next","Read"};
        for(int i=0;i<4;++i) box({i*width/4,height-90,width/4,90},home?footHome[i]:footEdit[i],label);
        if(full||++repaintCount>=96) {FullUpdate();repaintCount=0;}else PartialUpdateBW(0,0,width,height);
    }
    void paintGrid() {
        cards.clear();gridColumns=std::max(1,std::min(4,width/420));int side=(width-40-(gridColumns-1)*20)/gridColumns;
        int nrows=std::max(1,(bottom-top)/(side+20));gridCount=gridColumns*nrows;
        int total=static_cast<int>(entries.size())+1;gridPage=std::max(0,std::min(gridPage,(total-1)/gridCount));
        for(int i=0;i<gridCount && gridPage*gridCount+i<total;++i) {
            int index=gridPage*gridCount+i;Rect r{20+(i%gridColumns)*(side+20),top+(i/gridColumns)*(side+20),side,side};cards.push_back(r);DrawRect(r.x,r.y,r.w,r.h,accentColor);
            if(index==gridSelection) DrawRect(r.x+4,r.y+4,r.w-8,r.h-8,selectionColor);
            if(index==0) {text({r.x+20,r.y+side/3,r.w-40,100},"+ New document",title,ALIGN_CENTER);continue;}
            const auto &e=entries[index-1];text({r.x+18,r.y+18,r.w-36,120},e.name,title);
            if(e.folder) {text({r.x+18,r.y+150,r.w-36,100},"Folder",label);}
            else {
                char date[32];strftime(date,sizeof(date),"%Y-%m-%d",localtime(&e.modified));text({r.x+18,r.y+130,r.w-36,50},string(date)+" / "+std::to_string(e.bytes)+" B",label);
                if(ext(e.name)!=".html") {std::ifstream in(e.path,std::ios::binary);char peek[257]={0};in.read(peek,256);string snippet(peek,static_cast<size_t>(in.gcount()));while(!snippet.empty()&&!Workspace::validUtf8(snippet)) snippet.pop_back();text({r.x+18,r.y+190,r.w-36,std::max(35,r.h-270)},snippet,label);}
                text({r.x+18,r.y+r.h-60,r.w/2-20,50},"Read",label);text({r.x+r.w/2,r.y+r.h-60,r.w/2-18,50},ext(e.name)==".html"?"HTML":"Edit",label);
            }
        }
    }
    void paintEditor() {
        buildRows();int cr=cursorRow(),count=visibleRows();
        if(followCursor) {if(cr<firstRow) firstRow=cr;if(cr>=firstRow+count) firstRow=cr-count+1;followCursor=false;}
        firstRow=std::max(0,std::min(firstRow,std::max(0,static_cast<int>(rows.size())-count)));
        if(pageStyle==PAGE_GRID)
        {
            const int spacing=std::max(28,fontSize*2);
            for(int gx=20;gx<width-20;gx+=spacing)
                DrawLine(gx,top,gx,bottom,ruleColor);
        }
        for(int i=firstRow;i<static_cast<int>(rows.size())&&i<firstRow+count;++i) {
            const Row&r=rows[i];int y=top+(i-firstRow)*line,x=40;
            if(pageStyle==PAGE_RULED || pageStyle==PAGE_GRID)
                DrawLine(20,y+line-4,width-20,y+line-4,ruleColor);
            for(size_t p=r.first;p<r.last;) {
                size_t q=Workspace::next(editor.text,p);string g=editor.text.substr(p,q-p);if(g=="\t") g="    ";if(g=="\r") {p=q;continue;}
                int gw=glyphWidth(g);bool selected=p>=editor.low()&&p<editor.high();if(selected) FillArea(x,y,gw,line,selectionColor);
                SetFont(body,selected?selectedInk():inkColor);DrawTextRect(x,y,gw+2,line,g.c_str(),ALIGN_LEFT);x+=gw;p=q;
            }
            if(i==cr) FillArea(xAt(r,editor.cursor),y,3,fontSize+4,inkColor);
        }
    }
    void showMenu(int category) {
        modal=true;leftShift=rightShift=leftCtrl=rightCtrl=altgr=false;
        menuItems.clear();menuText.clear();menuItems.reserve(20);menuText.reserve(20);
        auto add=[&](int id,const string &s,bool enabled=true) {menuText.push_back(s);menuItems.push_back({static_cast<short>(enabled?ITEM_ACTIVE:ITEM_INACTIVE),static_cast<short>(id),const_cast<char*>(menuText.back().c_str()),nullptr});};
        bool edit=!home;
        if(category==0) {add(101,"New document");add(102,"Documents");add(103,"Save",edit);add(104,"Save copy",edit);add(105,"Rename",edit);add(106,"Export TXT",edit);add(107,"Export Markdown",edit);add(108,"Export HTML",edit);add(112,"Export JSON",edit);add(109,"Sharing");add(111,"Bluetooth send",edit);add(110,"Exit");}
        if(category==1) {add(201,"Undo",edit&&!editor.history.empty());add(202,"Redo",edit&&!editor.future.empty());add(203,"Select all",edit);add(204,"Copy",edit);add(205,"Cut",edit);add(206,"Paste",edit&&!clipboard.empty());add(207,"Find",edit);add(208,"Find next",edit);}
        if(category==2) {add(301,"Read preview",edit);add(302,"Increase text size");add(303,"Decrease text size");add(304,"Portrait");add(305,"Landscape");add(306,"Refresh screen");add(307,"Document start",edit);add(308,"Document end",edit);add(309,"White page");add(310,"Ruled page");add(311,"Grid page");add(312,"Dark page");add(313,"Black text");add(314,"Blue text");add(315,"Red text");add(316,"Green text");add(318,"Ivory background");add(319,"Blue background");add(320,"Grey background");add(321,"Black background");add(322,"White text");add(317,"Reset appearance");}
        if(category==3) {add(401,"Title / Heading 1",edit);add(402,"Heading 2",edit);add(403,"Heading 3",edit);add(404,"Body",edit);add(405,"Bold",edit);add(406,"Italic",edit);}
        if(category==4) {add(501,"Date",edit);add(502,"Bullet",edit);add(503,"Line break",edit);add(504,"Text using screen keyboard",edit);}
        if(category==5) {add(601,"Using the editor");add(602,"Keyboard shortcuts");add(109,"Export and sharing");add(603,"About / limitations");}
        if(category==6) {add(204,"Copy",edit);add(205,"Cut",edit);add(206,"Paste",edit&&!clipboard.empty());add(405,"Bold",edit);add(406,"Italic",edit);add(401,"Heading 1",edit);add(402,"Heading 2",edit);add(403,"Heading 3",edit);add(203,"Select all",edit);}
        add(900,"Close menu");
        menuItems.push_back({0,0,nullptr,nullptr});auto r=GetMenuRect(menuItems.data());OpenMenu(menuItems.data(),0,std::max(0,(width-r.w)/2),std::max(toolbar,(height-r.h)/2),menuCallback);
    }
    void prompt(int action,const char *titleText,const string &value="") {
        leftShift=rightShift=leftCtrl=rightCtrl=altgr=false;dead.clear();
        promptAction=action;modal=true;std::memset(keyboard,0,sizeof(keyboard));std::strncpy(keyboard,value.c_str(),sizeof(keyboard)-1);OpenKeyboard(titleText,keyboard,sizeof(keyboard)-1,KBD_NORMAL,keyboardCallback);
    }
    void acceptPrompt(char *s) {
        if(!s||!*s) {paint(true);return;}string value(s),error;
        if(promptAction==207) {search=value;if(!editor.find(search)) Message(ICON_INFORMATION,"Find","No match",1500);changed(false);paint(true);return;}
        if(promptAction==504) {changed(editor.insert(value));paint(true);return;}
        if(promptAction==506) {sendBluetoothNow(&value[0]);return;}
        if(!Workspace::validName(value)) {status="Filename rejected";paint(true);Message(ICON_ERROR,"Filename","Use a filename without slashes.",2500);return;}
        if(ext(value).empty()) value+=promptAction==105?ext(path):".md";
        if(ext(value)!=".txt"&&!markdown(value)) {status="Use .txt or .md";paint(true);Message(ICON_ERROR,"Filename",status.c_str(),2500);return;}
        string target=directory+"/"+value;
        if(exists(target)) {status="That filename already exists";paint(true);Message(ICON_ERROR,"Filename",status.c_str(),2500);return;}
        if(promptAction==101) {if(!Workspace::saveDocument(target,"",error)) Message(ICON_ERROR,"Create",error.c_str(),2500);else open(target);}
        else if(promptAction==104) {if(!Workspace::saveDocument(target,editor.text,error)) Message(ICON_ERROR,"Copy",error.c_str(),2500);else {path=target;saved=editor.text;status="Saved copy";saveFailed=false;}}
        else if(promptAction==105 && save()) {if(std::rename(path.c_str(),target.c_str())!=0) Message(ICON_ERROR,"Rename","Rename failed",2500);else {path=target;status="Renamed";}}
        paint(true);
    }
    bool ensureMarkdown() {
        if(markdown(path)) return true;
        string target=uniquePath(directory,stem(base(path))+".md"),error;
        if(!save()||!Workspace::saveDocument(target,editor.text,error)) {if(!error.empty()) Message(ICON_ERROR,"Format",error.c_str(),2500);return false;}
        path=target;saved=editor.text;status="Markdown copy created";return true;
    }
    string exportFile(const string &suffix,bool previewFile=false) {
        string dir=ROOT+(previewFile?"/.preview":"/Exports");if(mkdir(dir.c_str(),0777)!=0 && !exists(dir)) {Message(ICON_ERROR,"Export","Cannot create export folder",2500);return "";}
        string target=uniquePath(dir,stem(base(path))+suffix),error,value;
        if(suffix==".html") value=Workspace::html(editor.text,base(path),markdown(path));
        else if(suffix==".txt") value=Workspace::plain(editor.text,markdown(path));else value=editor.text;
        if(suffix==".json")
            value="{\n  \"title\": \""+jsonEscape(base(path))+"\",\n  \"source\": \""+jsonEscape(path)+"\",\n  \"format\": \""+(markdown(path)?"markdown":"text")+"\",\n  \"bytes\": "+std::to_string(editor.text.size())+",\n  \"content\": \""+jsonEscape(editor.text)+"\"\n}\n";
        // One preview per document: replace only our generated preview file.
        if(previewFile) target=dir+"/preview.html";
        if(!Workspace::saveDocument(target,value,error)) {Message(ICON_ERROR,"Export",error.c_str(),2500);return "";}
        if(!previewFile) {status="Exported: "+base(target);Message(ICON_INFORMATION,"Export",("Saved to textEditor/Exports/\n"+base(target)).c_str(),3000);}
        return target;
    }
    std::vector<string> makeShareFiles()
    {
        std::vector<string> files;
        const char *suffixes[]={".txt",".md",".html",".json"};
        for(const char *suffix:suffixes)
        {
            const string file=exportFile(suffix);
            if(!file.empty()) files.push_back(file);
        }
        return files;
    }
    void sendBluetoothNow(char *device)
    {
        modal=false;
        if(!device || !*device)
        {
            status="No Bluetooth device selected";
            paint(true);
            return;
        }
        if(!BtSendFiles)
        {
            status="Bluetooth file API is unavailable";
            Message(ICON_ERROR,"Bluetooth","This firmware lacks the InkView file-send API. Use USB, Cloud, or Dropbox.",5000);
            paint(true);
            return;
        }
        std::vector<char*> files;
        for(string &file:pendingBluetoothFiles) files.push_back(const_cast<char*>(file.c_str()));
        files.push_back(nullptr);
        const int result=BtSendFiles(device,files.data());
        if(result==0)
        {
            status="Bluetooth transfer started";
            Message(ICON_INFORMATION,"Bluetooth","The export bundle was sent.",4000);
        }
        else
        {
            status="Bluetooth transfer failed";
            Message(ICON_ERROR,"Bluetooth","The reader rejected this transfer. Check pairing and try again.",5000);
        }
        paint(true);
    }
    void sendBluetooth()
    {
        if(home || !save()) return;
        pendingBluetoothFiles=makeShareFiles();
        if(pendingBluetoothFiles.empty()) return;
        if(OpenBTdevicesMenu)
        {
            std::strncpy(bluetoothTitle,"Send export bundle",sizeof(bluetoothTitle)-1);
            bluetoothTitle[sizeof(bluetoothTitle)-1]='\0';
            modal=true;
            OpenBTdevicesMenu(bluetoothTitle,0,toolbar,bluetoothCallback);
            return;
        }
        prompt(506,"Bluetooth MAC","AA:BB:CC:DD:EE:FF");
    }
    void preview() {if(home||!save()) return;string p=exportFile(".html",true);if(!p.empty()) OpenBook(p.c_str(),nullptr,0);}
    string search;
    void command(int id) {
        bool change=false;
        switch(id) {
        case 101:if(save()) prompt(101,"New document",stamp()+".md");return;
        case 102:goHome();return;
        case 103:save();break;
        case 104:prompt(104,"Save copy",stem(base(path))+"-copy"+ext(path));return;
        case 105:prompt(105,"Rename",base(path));return;
        case 106:exportFile(".txt");break;
        case 107:exportFile(".md");break;
        case 108:exportFile(".html");break;
        case 112:exportFile(".json");break;
        case 109:Message(ICON_INFORMATION,"Sharing","Exports: textEditor/Exports\nUSB: copy files to your computer.\nWi-Fi: use Library > Upload to Cloud.\nDropbox: copy exports to its sync folder.\nBluetooth: choose a paired device.\nSend-to-PocketBook receives email.",10000);return;
        case 111:sendBluetooth();return;
        case 110:if(save()) CloseApp();return;
        case 201:change=editor.undo();break;
        case 202:change=editor.redo();break;
        case 203:editor.anchor=0;editor.cursor=editor.text.size();break;
        case 204:clipboard=editor.selected();status=clipboard.empty()?"Select text first":"Copied within app";break;
        case 205:clipboard=editor.selected();if(!clipboard.empty()) change=editor.insert("");break;
        case 206:change=editor.insert(clipboard);break;
        case 207:prompt(207,"Find",search);return;
        case 208:if(!editor.find(search)) status="No match";break;
        case 301:preview();return;
        case 302:fontSize=std::min(72,fontSize+4);metrics();break;
        case 303:fontSize=std::max(28,fontSize-4);metrics();break;
        case 304:SetOrientation(0);metrics();break;
        case 305:SetOrientation(2);metrics();break;
        case 306:paint(true);return;
        case 307:editor.move(0);break;
        case 308:editor.move(editor.text.size());break;
        case 309:applyPageStyle(PAGE_WHITE);savePreferences();break;
        case 310:pageStyle=PAGE_RULED;savePreferences();break;
        case 311:pageStyle=PAGE_GRID;savePreferences();break;
        case 312:applyPageStyle(PAGE_DARK);savePreferences();break;
        case 313:setInk(BLACK);return;
        case 314:setInk(0x164f86);return;
        case 315:setInk(0x8f1f25);return;
        case 316:setInk(0x17633a);return;
        case 318:setBackground(0xfff8e7);return;
        case 319:setBackground(0xe4efff);return;
        case 320:setBackground(0xe8e8e8);return;
        case 321:setBackground(0x101820);return;
        case 322:setInk(WHITE);return;
        case 317:pageStyle=PAGE_WHITE;pageColor=WHITE;inkColor=BLACK;accentColor=0x38566f;ruleColor=0xc7d0d8;selectionColor=0xb4c9d8;savePreferences();break;
        case 401:case 402:case 403:case 404:if(ensureMarkdown()) change=editor.heading(id==404?0:id-400);break;
        case 405:case 406:if(ensureMarkdown()) change=editor.wrap(id==405?"**":"*");break;
        case 501:change=editor.insert(stamp());break;
        case 502:change=editor.insert("- ");break;
        case 503:change=editor.insert("\n");break;
        case 504:prompt(504,"Insert text");return;
        case 900:modal=false;paint(true);return;
        case 601:Message(ICON_INFORMATION,"Using the editor","Tap a card to edit. Read opens a preview.\nTap text to place the cursor.\nShift + arrows selects text.\nFormat inserts Markdown markers.\nRead shows headings, bold and italic.\nTXT formatting creates a Markdown copy.\nAutosave: after pauses or every 5 seconds.",10000);return;
        case 602:Message(ICON_INFORMATION,"Shortcuts","Ctrl+N New / Ctrl+O Documents\nCtrl+S Save / Ctrl+F Find\nCtrl+Z Undo / Ctrl+Y Redo\nCtrl+A Select all\nCtrl+C/X/V Copy/Cut/Paste\nCtrl+B/I Bold/Italic\nEsc Documents / F1 Help",10000);return;
        case 603:Message(ICON_INFORMATION,"InkPad Notebook 0.3.0","Target: PB743K3 / firmware 6.10\nNative InkView / GPL-3.0\nUTF-8 TXT and Markdown, up to 512 KiB\nTouch selection, colours, lines and grid\nJSON export and Bluetooth bundle send\nClipboard stays within this app.\nDOCX, PDF and mail need a computer.\nReader testing remains required.",10000);return;
        default:return;
        }
        changed(change);paint(true);
    }
    void activate(int index,bool read=false) {
        if(index==0) {command(101);return;}
        if(index<1||index>static_cast<int>(entries.size())) return;
        auto e=entries[index-1];if(e.folder) {directory=e.path;scan();paint(true);}else open(e.path,read);
    }
    void page(int delta) {
        if(home) {gridPage+=delta;gridPage=std::max(0,gridPage);gridSelection=gridPage*gridCount;}
        else {buildRows();firstRow+=delta*visibleRows();followCursor=false;}
        paint(true);
    }
    size_t positionFromPoint(int x,int y)
    {
        buildRows();
        if(y<top || y>=bottom || rows.empty()) return editor.cursor;
        const int row=std::max(0,std::min(static_cast<int>(rows.size())-1,firstRow+(y-top)/line));
        return positionAt(rows[row],x);
    }
    void selectWord(size_t position)
    {
        if(editor.text.empty()) return;
        position=std::min(position,editor.text.size());
        if(position==editor.text.size()) position=Workspace::previous(editor.text,position);
        size_t start=position,end=Workspace::next(editor.text,position);
        const unsigned char current=static_cast<unsigned char>(editor.text[position]);
        if(isWordByte(current))
        {
            while(start>0)
            {
                const size_t before=Workspace::previous(editor.text,start);
                if(!isWordByte(static_cast<unsigned char>(editor.text[before]))) break;
                start=before;
            }
            while(end<editor.text.size() && isWordByte(static_cast<unsigned char>(editor.text[end])))
                end=Workspace::next(editor.text,end);
        }
        editor.anchor=start;editor.cursor=end;touchLong=true;changed(false);
    }
    void touchDown(int x,int y)
    {
        if(modal)
        {
            modal=false;
            paint(true);
            return;
        }
        touchActive=true;touchDragging=false;touchLong=false;touchDownX=x;touchDownY=y;
        touchDownPosition=(!home && y>=top && y<bottom)?positionFromPoint(x,y):editor.cursor;
    }
    void touchMove(int x,int y)
    {
        if(!touchActive || home || modal) return;
        const int dx=x-touchDownX,dy=y-touchDownY;
        if(dx*dx+dy*dy<144) return;
        touchDragging=true;
        editor.anchor=touchDownPosition;
        editor.cursor=positionFromPoint(x,y);
        changed(false);
        paint(false);
    }
    void touchLongPress(int x,int y)
    {
        if(!touchActive || home || modal || y<top || y>=bottom) return;
        selectWord(positionFromPoint(x,y));
        showMenu(6);
    }
    void touchUp(int x,int y)
    {
        if(!touchActive) {tap(x,y);return;}
        touchActive=false;
        if(home || y<top || y>=bottom)
        {
            if(!touchDragging && !touchLong) tap(x,y);
            return;
        }
        if(touchDragging)
        {
            editor.anchor=touchDownPosition;
            editor.cursor=positionFromPoint(x,y);
            changed(false);paint(true);
        }
        else if(!touchLong)
        {
            editor.move(positionFromPoint(x,y));
            changed(false);paint(false);
        }
    }
    void tap(int x,int y) {
        if(modal) {modal=false;paint(true);return;}
        if(y<toolbar) {int cols=toolbar==160?3:6;showMenu(std::min(5,(y/80)*cols+x*cols/width));return;}
        if(y>=height-90) {
            int col=x*4/width;
            if(col==1||col==2) {page(col==1?-1:1);return;}
            if(col==3) {command(home?101:301);return;}
            if(!home) goHome();else if(directory!=ROOT) {directory=directory.substr(0,directory.find_last_of('/'));scan();paint(true);}return;
        }
        if(home) {for(size_t i=0;i<cards.size();++i) if(cards[i].contains(x,y)) {int index=gridPage*gridCount+i;bool read=y>=cards[i].y+cards[i].h-70 && x<cards[i].x+cards[i].w/2;activate(index,read);return;}}
        else if(y>=top&&y<bottom) {buildRows();int r=firstRow+(y-top)/line;if(r<static_cast<int>(rows.size())) {editor.move(positionAt(rows[r],x),leftShift||rightShift);changed(false);}}
    }
    void key(int code,int value) {
        if(hidden) return;
        if(code==KEY_LEFTSHIFT) {leftShift=value!=0;return;}
        if(code==KEY_RIGHTSHIFT) {rightShift=value!=0;return;}
        if(code==KEY_LEFTCTRL) {leftCtrl=value!=0;return;}
        if(code==KEY_RIGHTCTRL) {rightCtrl=value!=0;return;}
        if(code==KEY_RIGHTALT) {altgr=value!=0;return;}
        if(modal)
        {
            if(value!=0 && code==KEY_ESC) {modal=false;paint(true);}
            return;
        }
        if(value==0) return;
        bool shift=leftShift||rightShift;
        if(code==KEY_CAPSLOCK) {if(value==1) caps=!caps;return;}
        if(code==KEY_ESC) {if(!home) goHome();else showMenu(0);return;}
        if(code==KEY_F1) {command(601);return;}
        if((leftCtrl||rightCtrl)&&!altgr) {
            string letter=keys.plain[code];int action=0;
            if(letter=="n") action=101;
            if(letter=="o") action=102;
            if(!home) {if(letter=="s") action=103;if(letter=="z") action=shift?202:201;if(letter=="y") action=202;if(letter=="a") action=203;if(letter=="c") action=204;if(letter=="x") action=205;if(letter=="v") action=206;if(letter=="f") action=207;if(letter=="b") action=405;if(letter=="i") action=406;}
            if(action) command(action);else if(!home&&code==KEY_HOME) {editor.move(0,shift);changed(false);}else if(!home&&code==KEY_END) {editor.move(editor.text.size(),shift);changed(false);}return;
        }
        if(home) {
            if(code==KEY_ENTER) {activate(gridSelection);return;}
            if(code==KEY_PAGEUP||code==KEY_PAGEDOWN) {page(code==KEY_PAGEUP?-1:1);return;}
            int delta=code==KEY_LEFT?-1:code==KEY_RIGHT?1:code==KEY_UP?-gridColumns:code==KEY_DOWN?gridColumns:0;
            if(delta) {gridSelection=std::max(0,std::min(static_cast<int>(entries.size()),gridSelection+delta));gridPage=gridSelection/gridCount;paint(true);}return;
        }
        if(code==KEY_PAGEUP||code==KEY_PAGEDOWN) {page(code==KEY_PAGEUP?-1:1);return;}
        if(code==KEY_LEFT||code==KEY_RIGHT) {editor.move(code==KEY_LEFT?Workspace::previous(editor.text,editor.cursor):Workspace::next(editor.text,editor.cursor),shift);changed(false);return;}
        if(code==KEY_HOME||code==KEY_END||code==KEY_UP||code==KEY_DOWN) {
            int r=cursorRow();size_t p=editor.cursor;
            if(code==KEY_HOME) p=rows[r].first;
            else if(code==KEY_END) p=rows[r].last;
            else {int x=xAt(rows[r],editor.cursor);r=std::max(0,std::min(static_cast<int>(rows.size())-1,r+(code==KEY_UP?-1:1)));p=positionAt(rows[r],x);}
            editor.move(p,shift);changed(false);return;
        }
        if(code==KEY_BACKSPACE||code==KEY_DELETE) {if(!dead.empty()) {dead.clear();return;}changed(editor.erase(code==KEY_BACKSPACE));return;}
        string glyph;
        if(code==KEY_ENTER) glyph="\n";else if(code==KEY_SPACE) glyph=" ";else if(code==KEY_TAB) glyph="    ";
        else {
            string plain=keys.plain[code];bool letter=plain.size()==1 && plain[0]>='a' && plain[0]<='z';
            auto &map=altgr?keys.altgr:((letter?(shift!=caps):shift)?keys.shift:keys.plain);
            auto it=map.find(code);if(it!=map.end()) glyph=it->second;
        }
        if(glyph.empty()) return;
        if(isDeadGlyph(glyph)) {dead=glyph;return;}
        if(!dead.empty()) {glyph=composeAccent(dead,glyph);dead.clear();}
        if(!editor.insert(glyph)) status="Document limit: 512 KiB";else changed(true);
    }
    int event(int type,int p1,int p2) {
        if(type==EVT_POINTERDOWN) {touchDown(p1,p2);return 1;}
        if(type==EVT_POINTERMOVE) {touchMove(p1,p2);return 1;}
        if(type==EVT_POINTERLONG||type==EVT_POINTERHOLD) {touchLongPress(p1,p2);return 1;}
        if(type==EVT_POINTERUP) {touchUp(p1,p2);return 1;}
        if(type==EVT_HIDE||type==EVT_BACKGROUND) {if(!home) save(false);hidden=true;leftShift=rightShift=leftCtrl=rightCtrl=altgr=false;dead.clear();return 1;}
        if(type==EVT_SHOW||type==EVT_FOREGROUND) {hidden=false;paint(true);return 1;}
        if(type==EVT_ORIENTATION) {if(p1>=0&&p1<=3) SetOrientation(p1);metrics();paint(true);return 1;}
        return 0;
    }
};
WorkspaceApp *WorkspaceApp::app=nullptr;
static int handle(int type,int p1,int p2) {
    if(type==EVT_INIT) {if(!WorkspaceApp::app) new WorkspaceApp();return 1;}
    auto *a=WorkspaceApp::app;if(!a) return 0;
    if(type==EVT_EXIT) {delete a;return 1;}
    if(type==EVT_KEYPRESS_EXT||type==EVT_KEYRELEASE_EXT||type==EVT_KEYREPEAT_EXT) {a->key(p1,type==EVT_KEYRELEASE_EXT?0:type==EVT_KEYREPEAT_EXT?2:1);return 1;}
    if(type==EVT_KEYPRESS) {if(p1==23||p1==26) a->showMenu(0);else if(p1==24||p1==28) a->page(-1);else if(p1==25||p1==29) a->page(1);return 1;}
    return a->event(type,p1,p2);
}
int main() {OpenScreen();InkViewMain(handle);return 0;}
