//
// Author:           JuanJakobo
// Date:             16.09.2021
//
//-------------------------------------------------------------------

#include "inkview.h"
#include "textView.h"
#include "eventHandler.h"

#include "deviceModel.h"
#include "device_match.h"
#include "keymap_default.h"
#include "text_core.h"
#include "log.h"

#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <cctype>

#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

using std::string;

namespace
{
TextView *g_activeText = nullptr;

string nextGlyph(const string &text, size_t &index)
{
    if (index >= text.size())
        return {};
    const size_t start = index++;
    while (index < text.size() && TextCore::isUtf8Continuation(static_cast<unsigned char>(text[index])))
        ++index;
    return text.substr(start, index - start);
}

void pollRawKeys()
{
    if (!g_activeText || !g_activeText->rawOpen())
        return;
    g_activeText->pollRaw();
    if (g_activeText && g_activeText->rawOpen())
        SetHardTimer("tw_poll", &pollRawKeys, 40);
}
}

TextView::TextView(const irect &contentRect, Device device, const string &filePath, const DeviceProfile *profile, int shownPage)
    : View(contentRect, shownPage), _filePath(filePath), _profile(profile)
{
    _lineCount = 0;
    _currentX = 0;
    _currentY = 0;
    _cursorPositionStr = 0;
    draw();
    if (!loadKeyMaps())
        return;

    if (!device.inputNodePath.empty())
        openRawInput(device.inputNodePath);
    else
    {
        _extMode = true;
        Message(ICON_INFORMATION, "Information", "InkPad keyboard is live. Root is not used. Esc saves and closes.", 1800);
    }
}

TextView::~TextView()
{
    if (_rawFd >= 0)
    {
        ::close(_rawFd);
        _rawFd = -1;
    }
    ClearTimer(&pollRawKeys);
    if (g_activeText == this)
        g_activeText = nullptr;
    if (_textFont)
        CloseFont(_textFont);
}

bool TextView::checkIfEntryClicked(int x, int y)
{
    if (IsInRect(x, y, &_firstPageButton))
        firstPage();
    else if (IsInRect(x, y, &_nextPageButton))
        nextPage();
    else if (IsInRect(x, y, &_prevPageButton))
        prevPage();
    else if (IsInRect(x, y, &_lastPageButton))
        actualizePage(_page);

    return false;
}

void TextView::layoutMetrics()
{
    if (_textFont)
    {
        CloseFont(_textFont);
        _textFont = nullptr;
    }

    const PageMetrics metrics = metricsFor(_profile, ScreenWidth(), ScreenHeight());
    _textHeight = metrics.textHeight;
    _textNextLineY = metrics.lineGap;
    _textBeginX = metrics.margin;
    _textEndX = _contentRect.w - metrics.margin;
    if (_textEndX <= _textBeginX)
        _textEndX = _textBeginX + _textHeight;
    _textBeginY = _contentRect.y;
    _textEndY = _contentRect.h - _footerHeight - _textHeight;
    _cursorThickness = std::max(3, _textHeight / 14);
    _fullRefreshEvery = metrics.fullRefreshEvery;
    _textFont = OpenFont("Roboto", _textHeight, FONT_STD);
}

void TextView::draw()
{
    FillAreaRect(&_contentRect, WHITE);
    layoutMetrics();

    _currentX = _textBeginX;
    _currentY = _textBeginY;
    _lineCount = 0;
    _currentText.clear();

    _lastPageLineCount.insert(std::pair<int, int>(_page, 0));
    _pageCharPos.insert(std::pair<int, int>(_page, 0));

    SetFont(_textFont, BLACK);
    if (iv_access(_filePath.c_str(), W_OK) == 0)
    {
        std::ifstream inFile(_filePath);
        string line;
        bool multipleLines = false;

        if (inFile.is_open())
        {
            while (getline(inFile, line))
            {
                if (multipleLines)
                {
                    _lineCount++;
                    _lineWidth.insert(std::pair<int, int>(_lineCount, _currentX));
                    _currentX = _textBeginX;
                    _currentY += _textNextLineY;
                    _currentText += "\n";
                }

                for (size_t index = 0; index < line.size();)
                {
                    const string glyph = nextGlyph(line, index);
                    multipleLines = true;
                    _currentText += glyph;

                    int charWidth = glyphWidth(glyph);

                    if (_currentX + charWidth > _textEndX)
                    {
                        _lineCount++;
                        _lineWidth.insert(std::pair<int, int>(_lineCount, _currentX));
                        _currentX = _textBeginX;
                        _currentY += _textNextLineY;
                    }

                    if (_currentY >= _textEndY)
                    {
                        _shownPage++;
                        _page = _shownPage;
                        _lastPageLineCount.insert(std::pair<int, int>(_page, _lineCount));
                        _pageCharPos.insert(std::pair<int, int>(_page, static_cast<int>(TextCore::lastGlyphStart(_currentText))));
                        _currentX = _textBeginX;
                        _currentY = _textBeginY;
                    }
                    _currentX += charWidth;
                }
            }
            inFile.close();
        }
        drawPage();
    }
    else
    {
        Message(ICON_ERROR, "Error", "File not found.", 1000);
    }
}

int TextView::glyphWidth(const string &glyph) const
{
    if (glyph.empty() || isDeadGlyph(glyph))
        return 0;
    if (glyph.size() == 1)
        return CharWidth(glyph[0]);
    const int width = StringWidth(glyph.c_str());
    return width > 0 ? width : _textHeight;
}

int TextView::drawChar(const string &glyph)
{
    int charWidth = glyphWidth(glyph);
    if (charWidth < 1)
        charWidth = _textHeight / 2;

    if (_currentX + charWidth > _textEndX)
    {
        FillArea(_currentX, _currentY, _cursorThickness, _textHeight, WHITE);
        PartialUpdate(_currentX, _currentY, _cursorThickness, _textHeight);
        _lineCount++;
        _lineWidth.insert(std::pair<int, int>(_lineCount, _currentX));
        _currentX = _textBeginX;
        _currentY += _textNextLineY;
    }

    if (_currentY >= _textEndY)
    {
        addPage();
        FillArea(_currentX, _currentY, _cursorThickness, _textHeight, WHITE);
        PartialUpdate(_currentX, _currentY, _cursorThickness, _textHeight);
    }

    _cursorPositionStr = static_cast<int>(_currentText.size());
    SetFont(_textFont, BLACK);
    DrawTextRect(_currentX, _currentY, charWidth, _textHeight, glyph.c_str(), ALIGN_CENTER);
    _currentX += charWidth;

    return charWidth;
}

void TextView::ensureDefaultKeymap(const string &keyBindingsPath) const
{
    if (iv_access(keyBindingsPath.c_str(), R_OK) == 0)
        return;

    std::ofstream out(keyBindingsPath, std::ofstream::trunc);
    if (!out.is_open())
        return;
    out << defaultEnglishKeymap();
}

bool TextView::loadKeyMaps()
{
    const string keyBindingsPath = CONFIG_FOLDER + "/map.keys";
    ensureDefaultKeymap(keyBindingsPath);

    std::ifstream inFile(keyBindingsPath);
    if (!inFile.is_open())
    {
        Message(ICON_ERROR, "Error", "Could not write the default keymap.", 2000);
        return false;
    }

    ParsedKeymap parsed;
    parseKeymap(inFile, parsed);
    _keyBindings = std::move(parsed.plain);
    _keyBindingsShift = std::move(parsed.shift);
    _keyBindingsAltGr = std::move(parsed.altgr);
    return !_keyBindings.empty();
}

bool TextView::saveDraft()
{
    string error;
    const bool saved = TextCore::writeAtomicFile(_filePath, _currentText, &error);
    if (!saved)
        Log::writeErrorLog("save failed: " + error);
    return saved;
}

bool TextView::noteEdit(bool forceSave)
{
    if (!forceSave)
        ++_editsSinceSave;

    if (forceSave || _editsSinceSave >= 24)
    {
        if (!saveDraft())
        {
            Message(ICON_ERROR, "Error", "Draft save failed. Text remains open.", 2000);
            return false;
        }
        _editsSinceSave = 0;
        if (_fullRefreshEvery > 0)
        {
            _sinceFullRefresh += 24;
            if (_sinceFullRefresh >= static_cast<unsigned int>(_fullRefreshEvery))
            {
                _sinceFullRefresh = 0;
                FullUpdate();
            }
        }
    }
    return true;
}

void TextView::paintGlyph(int x, int y, int w, int h)
{
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    PartialUpdate(x, y, w, h);
}

string TextView::lookupKey(int code) const
{
    const auto plain = _keyBindings.find(code);
    const bool capsLetter = _capsLock && plain != _keyBindings.end() && plain->second.size() == 1 && std::isalpha(static_cast<unsigned char>(plain->second[0]));
    if (_shiftPressed || capsLetter)
    {
        const auto it = _keyBindingsShift.find(code);
        if (it != _keyBindingsShift.end())
            return it->second;
    }
    else if (_altGrPressed)
    {
        const auto it = _keyBindingsAltGr.find(code);
        if (it != _keyBindingsAltGr.end())
            return it->second;
    }
    if (plain != _keyBindings.end())
        return plain->second;
    return {};
}

void TextView::openRawInput(const string &inputNodePath)
{
    _rawFd = ::open(inputNodePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (_rawFd < 0)
    {
        DrawTextRect(0, (_contentRect.h / 3) * 2, _contentRect.w, _textHeight, strerror(errno), ALIGN_CENTER);
        PartialUpdate(_contentRect.x, _contentRect.y, _contentRect.w, _contentRect.h);
        return;
    }

    g_activeText = this;
    Message(ICON_INFORMATION, "Information", "Typing through a temporary input node. Esc saves and closes.", 1600);
    SetHardTimer("tw_poll", &pollRawKeys, 40);
}

void TextView::pollRaw()
{
    if (_rawFd < 0)
        return;

    while (true)
    {
        input_event event{};
        const ssize_t n = ::read(_rawFd, &event, sizeof(event));
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            ::close(_rawFd);
            _rawFd = -1;
            break;
        }
        if (n != static_cast<ssize_t>(sizeof(event)))
            break;
        if (event.type == EV_KEY)
            onLinuxKey(event.code, event.value);
    }
}

bool TextView::onLinuxKey(int code, int value)
{
    if (_rawFd < 0 && !_extMode)
        return false;

    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT)
    {
        if (value == 0)
            _shiftPressed = false;
        else if (value == 1)
            _shiftPressed = true;
        return true;
    }
    if (code == KEY_RIGHTALT)
    {
        if (value == 0)
            _altGrPressed = false;
        else if (value == 1)
            _altGrPressed = true;
        return true;
    }
    if (code == KEY_CAPSLOCK)
    {
        if (value == 1)
            _capsLock = !_capsLock;
        return true;
    }

    if (value != 1 && value != 2)
        return true;

    SetFont(_textFont, BLACK);

    if (code == KEY_ESC)
    {
        if (_rawFd >= 0)
        {
            ::close(_rawFd);
            _rawFd = -1;
        }
        _extMode = false;
        if (!_currentText.empty())
        {
            if (saveDraft())
                Message(ICON_INFORMATION, "Information", "Input closed. File saved.", 1000);
            else
                Message(ICON_ERROR, "Error", "Draft save failed.", 2000);
        }
        else
        {
            remove(_filePath.c_str());
            Message(ICON_INFORMATION, "Information", "Input closed. Empty file removed.", 1000);
        }
        return true;
    }

    if (code == KEY_BACKSPACE)
    {
        if (!_pendingDead.empty())
        {
            _pendingDead.clear();
            return true;
        }
        if (_currentText.empty())
            return true;

        const string removed = TextCore::popGlyph(_currentText);
        if (!noteEdit(true))
        {
            _currentText += removed;
            return true;
        }
        draw();
        return true;
    }

    if (code == KEY_ENTER)
    {
        _lineCount++;
        _lineWidth.insert(std::pair<int, int>(_lineCount, _currentX));
        _currentText += "\n";
        if (!noteEdit(true))
        {
            _currentText.pop_back();
            --_lineCount;
            return true;
        }
        if ((_currentY + _textNextLineY) >= _textEndY)
        {
            _currentY += _textNextLineY;
            addPage();
        }
        else
        {
            FillArea(_currentX, _currentY, _cursorThickness, _textHeight, WHITE);
            PartialUpdate(_currentX, _currentY, _cursorThickness, _textHeight);
            _currentY += _textNextLineY;
            _currentX = _textBeginX;
            FillArea(_currentX, _currentY, _cursorThickness, _textHeight, BLACK);
            PartialUpdate(_currentX, _currentY, _cursorThickness, _textHeight);
        }
        _cursorPositionStr = _currentText.size();
        return true;
    }

    if (code == KEY_TAB || code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL || code == KEY_LEFTALT || code == KEY_UP || code == KEY_DOWN || code == KEY_LEFT || code == KEY_RIGHT)
        return true;

    const string key = (code == KEY_SPACE) ? string(" ") : lookupKey(code);
    if (key.empty())
        return true;
    if (isDeadGlyph(key))
    {
        _pendingDead = key;
        return true;
    }

    string glyph = key;
    if (!_pendingDead.empty())
    {
        glyph = composeAccent(_pendingDead, key);
        _pendingDead.clear();
    }

    FillArea(_currentX, _currentY, _cursorThickness, _textHeight, WHITE);
    _currentText += glyph;
    noteEdit(false);
    const int textWidth = drawChar(glyph);
    PartialUpdate(_currentX - textWidth, _currentY, textWidth, _textHeight);
    return true;
}

void TextView::removePage()
{
    _shownPage--;
    _page = _shownPage;
    drawPage();
    _lineCount++;
}

void TextView::drawPage()
{
    FillAreaRect(&_contentRect, WHITE);

    if (!_currentText.empty())
    {
        _currentX = _textBeginX;
        _currentY = _textBeginY;

        const auto lineIt = _lastPageLineCount.find(_shownPage);
        if (lineIt != _lastPageLineCount.end())
            _lineCount = lineIt->second;

        int lastPageCharPosition = 0;
        const auto pageIt = _pageCharPos.find(_shownPage);
        if (pageIt != _pageCharPos.end())
            lastPageCharPosition = pageIt->second;

        for (size_t i = static_cast<size_t>(lastPageCharPosition); i < _currentText.size();)
        {
            if (_currentY >= _textEndY)
                break;

            if (TextCore::isUtf8Continuation(static_cast<unsigned char>(_currentText[i])))
            {
                ++i;
                continue;
            }

            if (_currentText.at(i) == '\n')
            {
                ++i;
                _lineCount++;
                _lineWidth.insert(std::pair<int, int>(_lineCount, _currentX));
                _currentX = _textBeginX;
                _currentY += _textNextLineY;
                continue;
            }

            const string glyph = nextGlyph(_currentText, i);
            int charWidth = glyphWidth(glyph);

            if (_currentX + charWidth > _textEndX)
            {
                _lineCount++;
                _lineWidth.insert(std::pair<int, int>(_lineCount, _currentX));
                _currentX = _textBeginX;
                _currentY += _textNextLineY;
            }

            _cursorPositionStr = static_cast<int>(_currentText.size());
            SetFont(_textFont, BLACK);
            DrawTextRect(_currentX, _currentY, charWidth, _textHeight, glyph.c_str(), ALIGN_CENTER);
            _currentX += charWidth;
        }
        _cursorPositionStr = static_cast<int>(_currentText.size());
    }
    drawFooter();
    PartialUpdate(_contentRect.x, _contentRect.y, _contentRect.w, _contentRect.h);
}

void TextView::addPage()
{
    _shownPage++;
    _page = _shownPage;
    _lastPageLineCount.insert(std::pair<int, int>(_page, _lineCount));
    _pageCharPos.insert(std::pair<int, int>(_page, static_cast<int>(TextCore::lastGlyphStart(_currentText))));
    _currentX = _textBeginX;
    _currentY = _textBeginY;
    _cursorPositionStr = static_cast<int>(_currentText.size());

    FillAreaRect(&_contentRect, WHITE);
    drawFooter();
    FillArea(_currentX, _currentY, _cursorThickness, _textHeight, BLACK);
    if (_fullRefreshEvery > 0)
        FullUpdate();
    else
        PartialUpdate(_contentRect.x, _contentRect.y, _contentRect.w, _contentRect.h);
}
