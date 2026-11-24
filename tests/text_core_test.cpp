#include "text_core.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

static int failures = 0;

static void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << "\n";
        ++failures;
    }
}

int main()
{
    std::string text = "Bonjour été €";
    expect(TextCore::lastGlyphStart(text) == text.size() - 3, "euro boundary");
    expect(TextCore::popGlyph(text) == "€", "pop euro");
    expect(text == "Bonjour été ", "euro removed");
    expect(TextCore::popGlyph(text) == " ", "pop space");
    expect(TextCore::popGlyph(text) == "é", "pop accented glyph");
    expect(text == "Bonjour ét", "accent deletion remains valid");

    const std::string path = "/tmp/pbtypewriter-core-test.txt";
    std::string error;
    expect(TextCore::writeAtomicFile(path, "Bonjour été €\n", &error), "atomic save");
    std::ifstream input(path, std::ifstream::binary);
    const std::string saved((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    expect(saved == "Bonjour été €\n", "atomic roundtrip");
    std::remove(path.c_str());
    expect(!TextCore::writeAtomicFile("/missing-pbtypewriter-dir/file.txt", "x", &error), "save failure");

    if (failures)
    {
        std::cerr << failures << " failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "ok\n";
    return EXIT_SUCCESS;
}
