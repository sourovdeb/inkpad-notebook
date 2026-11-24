// Host test double. Not an emulator or PocketBook SDK replacement.
#pragma once
#include <string>
#include <vector>
#include <cstring>
struct ifont {int size;};
struct irect {int x,y,w,h;};
struct imenu {short type,index;char *text;imenu *submenu;};
typedef void (*iv_menuhandler)(int);
typedef void (*iv_keyboardhandler)(char*);
typedef void (*iv_timerproc)();
typedef void (*iv_itemchangehandler)(char*);
enum {BLACK=0,WHITE=0xffffff,FONT_STD=0,FONT_BOLD=1,ALIGN_LEFT=0,ALIGN_CENTER=1,ICON_INFORMATION=0,ICON_ERROR=1,KBD_NORMAL=0,ITEM_ACTIVE=2,ITEM_INACTIVE=3};
enum {EVT_INIT=21,EVT_EXIT=22,EVT_SHOW=23,EVT_HIDE=24,EVT_POINTERUP=29,EVT_POINTERDOWN=30,EVT_POINTERMOVE=31,EVT_ORIENTATION=32,EVT_POINTERLONG=34,EVT_POINTERHOLD=35,EVT_KEYPRESS_EXT=40,EVT_KEYRELEASE_EXT=41,EVT_KEYREPEAT_EXT=42,EVT_FOREGROUND=151,EVT_BACKGROUND=152,EVT_KEYPRESS=1001};
namespace Mock {
struct Draw {int x,y,w,h,size;std::string text;};
static int width=1404,height=1872,orientation=0;
static ifont *font=nullptr;
static std::string book,message;
static std::vector<Draw> draws;
static iv_menuhandler menu=nullptr;
static iv_keyboardhandler keyboard=nullptr;
}
inline int ScreenWidth(){return Mock::width;}
inline int ScreenHeight(){return Mock::height;}
inline void SetPanelType(int){}
inline int IsBluetoothEnabled(){return 1;}
inline int IsBluetoothAwake(){return 1;}
inline void SetBluetoothOn(){}
inline void BluetoothWakeUp(){}
inline void SetOrientation(int v){Mock::orientation=v;Mock::width=v==0||v==3?1404:1872;Mock::height=v==0||v==3?1872:1404;}
inline ifont *OpenFont(const char*,int n,int){return new ifont{n};}
inline void CloseFont(ifont *f){delete f;}
inline void SetFont(ifont*f,int){Mock::font=f;}
inline int StringWidth(const char*s){int n=0;for(size_t i=0;i<std::strlen(s);++i) if((static_cast<unsigned char>(s[i])&0xc0)!=0x80) ++n;return n*Mock::font->size*3/5;}
inline void DrawTextRect(int x,int y,int w,int h,const char*s,int){Mock::draws.push_back({x,y,w,h,Mock::font->size,s});}
inline void DrawRect(int,int,int,int,int){}
inline void DrawLine(int,int,int,int,int){}
inline void FillArea(int x,int y,int w,int h,int){if(x==0&&y==0&&w==Mock::width&&h==Mock::height) Mock::draws.clear();}
inline void FullUpdate(){}
inline void PartialUpdateBW(int,int,int,int){}
inline void SetWeakTimer(const char*,iv_timerproc,int){}
inline void ClearTimer(iv_timerproc){}
inline void OpenKeyboard(const char*,char*,int,int,iv_keyboardhandler cb){Mock::keyboard=cb;}
inline irect GetMenuRect(imenu*){return {0,0,600,700};}
inline void OpenMenu(imenu*,int,int,int,iv_menuhandler cb){Mock::menu=cb;}
inline int OpenBook(const char *p,const char*,int){Mock::book=p;return 1;}
inline void Message(int,const char*,const char*s,int){Mock::message=s;}
inline void CloseApp(){}
inline void OpenScreen(){}
inline void InkViewMain(int(*)(int,int,int)){}
