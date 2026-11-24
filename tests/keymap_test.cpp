#include "keymap_parse.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

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
    std::ifstream file("kbLayouts/fr.keys");
    expect(file.is_open(), "fr.keys opens");
    ParsedKeymap map;
    parseKeymap(file, map);

    expect(map.plain[16] == "a", "azerty a is on the q key");
    expect(map.plain[30] == "q", "azerty q is on the a key");
    expect(map.plain[3] == "é", "eacute");
    expect(map.plain[8] == "è", "egrave");
    expect(map.plain[10] == "ç", "ccedilla");
    expect(map.plain[11] == "à", "agrave");
    expect(map.plain[40] == "ù", "ugrave");
    expect(isDeadGlyph(map.plain[26]), "circumflex is dead");
    expect(isDeadGlyph(map.shift[26]), "diaeresis is dead");
    expect(map.altgr[18] == "€", "altgr e is euro");
    expect(composeAccent(map.plain[26], "e") == "ê", "dead circumflex e");
    expect(composeAccent(map.shift[26], "e") == "ë", "dead diaeresis e");
    expect(composeAccent(map.plain[26], "a") == "â", "dead circumflex a");
    expect(composeAccent(deadGlyph('`'), "u") == "ù", "grave u");

    std::stringstream english("16 q\n#shift\n16 Q\n#altgr\n16 @\n");
    ParsedKeymap en;
    parseKeymap(english, en);
    expect(en.plain[16] == "q" && en.shift[16] == "Q" && en.altgr[16] == "@", "english sections");

    if (failures)
    {
        std::cerr << failures << " failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "ok\n";
    return EXIT_SUCCESS;
}
