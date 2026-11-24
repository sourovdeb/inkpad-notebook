#ifndef KEYMAP_PARSE_H
#define KEYMAP_PARSE_H

#include <cctype>
#include <istream>
#include <map>
#include <sstream>
#include <string>

// Glyphs are UTF-8. A dead key is stored as a string whose first byte is 0x01
// and whose second byte is ^ " ` or ~.

struct ParsedKeymap
{
    std::map<int, std::string> plain;
    std::map<int, std::string> shift;
    std::map<int, std::string> altgr;
};

inline bool isDeadGlyph(const std::string &glyph)
{
    return glyph.size() == 2 && static_cast<unsigned char>(glyph[0]) == 0x01;
}

inline std::string deadGlyph(char mark)
{
    std::string glyph;
    glyph.push_back('\x01');
    glyph.push_back(mark);
    return glyph;
}

inline std::string trimCopy(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
        ++begin;
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(begin, end - begin);
}

inline std::string glyphFromToken(const std::string &token)
{
    if (token.size() == 2 && token[0] == '\\')
    {
        const char mark = token[1];
        if (mark == '^' || mark == '"' || mark == '`' || mark == '~')
            return deadGlyph(mark);
    }
    return token;
}

inline void parseKeymap(std::istream &in, ParsedKeymap &out)
{
    std::string line;
    bool shift = false;
    bool altGr = false;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.find("shift") != std::string::npos)
        {
            shift = true;
            altGr = false;
            continue;
        }
        if (line.find("altgr") != std::string::npos)
        {
            shift = false;
            altGr = true;
            continue;
        }
        std::stringstream row(line);
        int key = 0;
        if (!(row >> key))
            continue;
        std::string rest;
        std::getline(row, rest);
        rest = trimCopy(rest);
        if (rest.empty())
            continue;
        const std::string glyph = glyphFromToken(rest);
        if (shift)
            out.shift[key] = glyph;
        else if (altGr)
            out.altgr[key] = glyph;
        else
            out.plain[key] = glyph;
    }
}

inline std::string composeAccent(const std::string &dead, const std::string &base)
{
    if (!isDeadGlyph(dead) || base.empty())
        return base;
    const char mark = dead[1];
    struct Pair
    {
        char mark;
        const char *base;
        const char *out;
    };
    static const Pair table[] = {
        {'^', "a", "â"}, {'^', "A", "Â"}, {'^', "e", "ê"}, {'^', "E", "Ê"},
        {'^', "i", "î"}, {'^', "I", "Î"}, {'^', "o", "ô"}, {'^', "O", "Ô"},
        {'^', "u", "û"}, {'^', "U", "Û"},
        {'"', "a", "ä"}, {'"', "A", "Ä"}, {'"', "e", "ë"}, {'"', "E", "Ë"},
        {'"', "i", "ï"}, {'"', "I", "Ï"}, {'"', "o", "ö"}, {'"', "O", "Ö"},
        {'"', "u", "ü"}, {'"', "U", "Ü"}, {'"', "y", "ÿ"},
        {'`', "a", "à"}, {'`', "A", "À"}, {'`', "e", "è"}, {'`', "E", "È"},
        {'`', "u", "ù"}, {'`', "U", "Ù"},
        {'~', "a", "ã"}, {'~', "A", "Ã"}, {'~', "n", "ñ"}, {'~', "N", "Ñ"},
        {'~', "o", "õ"}, {'~', "O", "Õ"},
    };
    for (const Pair &item : table)
    {
        if (item.mark == mark && base == item.base)
            return item.out;
    }
    std::string visible(1, mark);
    if (mark == '"')
        visible = "¨";
    else if (mark == '`')
        visible = "`";
    return visible + base;
}

#endif
