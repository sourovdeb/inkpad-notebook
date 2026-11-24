#ifndef TEXT_CORE_H
#define TEXT_CORE_H

#include <cstddef>
#include <string>

namespace TextCore
{
bool isUtf8Continuation(unsigned char value);
std::size_t glyphStart(const std::string &text, std::size_t offset);
std::size_t lastGlyphStart(const std::string &text);
std::string popGlyph(std::string &text);
bool writeAtomicFile(const std::string &path, const std::string &contents, std::string *error = nullptr);
}

#endif
