#include "text_core.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace TextCore
{

bool isUtf8Continuation(unsigned char value)
{
    return (value & 0xC0) == 0x80;
}

std::size_t glyphStart(const std::string &text, std::size_t offset)
{
    if (text.empty())
        return 0;
    if (offset >= text.size())
        offset = text.size() - 1;
    while (offset > 0 && isUtf8Continuation(static_cast<unsigned char>(text[offset])))
        --offset;
    return offset;
}

std::size_t lastGlyphStart(const std::string &text)
{
    return text.empty() ? 0 : glyphStart(text, text.size() - 1);
}

std::string popGlyph(std::string &text)
{
    if (text.empty())
        return {};
    const std::size_t index = lastGlyphStart(text);
    const std::string glyph = text.substr(index);
    text.erase(index);
    return glyph;
}

bool writeAtomicFile(const std::string &path, const std::string &contents, std::string *error)
{
    const std::string temporaryPath = path + ".tmp";
    std::ofstream output(temporaryPath, std::ofstream::binary | std::ofstream::trunc);
    if (!output.is_open())
    {
        if (error)
            *error = std::strerror(errno);
        return false;
    }

    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.flush();
    const bool good = output.good();
    output.close();
    if (!good)
    {
        if (error)
            *error = "write failed";
        std::remove(temporaryPath.c_str());
        return false;
    }

    if (std::rename(temporaryPath.c_str(), path.c_str()) != 0)
    {
        if (error)
            *error = std::strerror(errno);
        std::remove(temporaryPath.c_str());
        return false;
    }
    return true;
}

}
