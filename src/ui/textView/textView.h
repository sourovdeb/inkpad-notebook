//------------------------------------------------------------------
// textView.h
//
// Author:           JuanJakobo
// Date:             21.09.2021
// Description:      An UI class to display text items
//-------------------------------------------------------------------

#ifndef TEXTVIEW
#define TEXTVIEW

#include "view.h"
#include "inkview.h"
#include "deviceModel.h"
#include "device_match.h"
#include "keymap_parse.h"

#include <string>
#include <map>

class TextView final : public View
{
public:

    /**
        * Displays a text view
        *
        * @param ContentRect area of the screen where the list view is placed
        * @param Items items that shall be shown in the listview
        */
    TextView(const irect &contentRect, Device device, const std::string &filePath, const DeviceProfile *profile, int shownPage = 1);

    ~TextView();

    /**
        * Linux key code from either the raw event node or InkView
        * EVT_KEY*_EXT (firmware 6 InkPads). value: 0 release, 1 press, 2 repeat.
        */
    bool onLinuxKey(int code, int value);

    void pollRaw();
    bool rawOpen() const { return _rawFd >= 0; }

    /**
        * Checkes if the buttons on the bottom have been clicked, if they have been, executes function
        *
        * @param x x-coordinate
        * @param y y-coordinate
        * @return always false as changing cursor is currently not supported
        */
    bool checkIfEntryClicked(int x, int y) override;

    void draw() override;
    void drawPage() override;

private:
    int _lineCount;
    int _currentX;
    int _currentY;
    int _cursorThickness = 4;
    int _cursorPositionStr;
    std::map<int,int> _pageCharPos;
    std::map<int,int> _lastPageLineCount;
    std::string _currentText;
    std::map<int,int> _lineWidth;
    std::map<int,std::string> _keyBindings;
    std::map<int,std::string> _keyBindingsShift;
    std::map<int,std::string> _keyBindingsAltGr;
    int _textNextLineY;
    int _textBeginX;
    int _textEndX;
    int _textBeginY;
    int _textEndY;
    std::string _filePath;

    int _textHeight;
    ifont *_textFont = nullptr;
    const DeviceProfile *_profile = nullptr;
    int _rawFd = -1;
    bool _extMode = false;
    bool _shiftPressed = false;
    bool _altGrPressed = false;
    bool _capsLock = false;
    std::string _pendingDead;
    unsigned int _editsSinceSave = 0;
    unsigned int _sinceFullRefresh = 0;
    int _fullRefreshEvery = 0;

    bool saveDraft();
    void layoutMetrics();
    bool noteEdit(bool forceSave);
    void paintGlyph(int x, int y, int w, int h);
    std::string lookupKey(int code) const;
    int glyphWidth(const std::string &glyph) const;
    void appendGlyph(const std::string &glyph);
    void ensureDefaultKeymap(const std::string &keyBindingsPath) const;

    //TODO documention and different methods in one

    /**
        * Draws a Char to the screen, if there is no more space, either calls addPage or jumps to next line
        *
        * @param char that shall be drawn
        * @return width of the char
        */
    int drawChar(const std::string &glyph);

    /**
        * loads a Keymap into the application to replicate keys
        *
        * @return true on sucess, false on error
        */
    bool loadKeyMaps();

    void openRawInput(const std::string &inputNodePath);

    void addPage();

    /**
        * removes a page
        *
        */
    void removePage();

};
#endif
