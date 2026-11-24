//------------------------------------------------------------------
// device_match.cpp
//-------------------------------------------------------------------

#include "device_match.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace
{

std::string upperCopy(const std::string &raw)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw)
        out.push_back(static_cast<char>(std::toupper(c)));
    return out;
}

std::string trimCopy(const std::string &raw)
{
    size_t begin = 0;
    while (begin < raw.size() && std::isspace(static_cast<unsigned char>(raw[begin])))
        ++begin;
    size_t end = raw.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(raw[end - 1])))
        --end;
    return raw.substr(begin, end - begin);
}

const DeviceProfile kProfiles[] = {
    {"743K3", "InkPad Color 3", 1404, 1872, 300, true, true, false, InputPath::ExtKeys, "B300"},
    {"743C", "InkPad Color 2", 1404, 1872, 300, true, true, false, InputPath::ExtKeys, "B300"},
    {"743G", "InkPad 4", 1404, 1872, 300, false, true, false, InputPath::ExtKeys, "B288"},
    {"741", "InkPad Color", 1404, 1872, 300, true, true, false, InputPath::RawEvdev, "B288"},
    {"740-3", "InkPad 3 Pro", 1404, 1872, 300, false, true, true, InputPath::RawEvdev, "B288"},
    {"740-2", "InkPad 3 Pro", 1404, 1872, 300, false, true, true, InputPath::RawEvdev, "B288"},
    {"740", "InkPad 3", 1404, 1872, 300, false, true, true, InputPath::RawEvdev, "i.MX6"},
    {"1040", "InkPad X", 1404, 1872, 227, false, true, true, InputPath::RawEvdev, "i.MX6"},
    {"970", "InkPad Lite", 825, 1200, 150, false, false, false, InputPath::NoBluetooth, "B288"},
    {"840", "InkPad", 1200, 1600, 250, false, false, false, InputPath::NoBluetooth, "i.MX6"},
    {"633", "PocketBook Color", 1072, 1448, 300, true, true, false, InputPath::RawEvdev, "B288"},
    {"632", "Touch HD 3", 1072, 1448, 300, false, true, false, InputPath::RawEvdev, "B288"},
};

}

std::string normalizeModelId(const std::string &raw)
{
    std::string text = trimCopy(raw);
    const auto open = text.rfind('(');
    const auto close = text.rfind(')');
    if (open != std::string::npos && close != std::string::npos && close > open + 1)
        text = text.substr(open + 1, close - open - 1);

    text = upperCopy(trimCopy(text));
    const std::string pocket = "POCKETBOOK";
    if (text.rfind(pocket, 0) == 0)
        text = trimCopy(text.substr(pocket.size()));

    if (text.rfind("PB", 0) == 0)
        text = text.substr(2);

    if (text == "READER INKPAD" || text == "INKPAD")
        return "840";
    if (text == "INKPAD 4")
        return "743G";
    if (text == "INKPAD COLOR 3")
        return "743K3";
    if (text == "INKPAD COLOR 2")
        return "743C";
    if (text == "INKPAD COLOR")
        return "741";
    if (text == "INKPAD 3 PRO")
        return "740-2";
    if (text == "INKPAD 3")
        return "740";
    if (text == "INKPAD X")
        return "1040";
    if (text == "INKPAD LITE")
        return "970";
    if (text == "PB633" || text == "COLOR")
        return "633";

    std::replace(text.begin(), text.end(), '_', '-');
    return text;
}

const DeviceProfile *matchProfile(const std::string &raw)
{
    if (raw.empty())
        return nullptr;

    const std::string id = normalizeModelId(raw);
    for (const auto &profile : kProfiles)
    {
        if (id == profile.id)
            return &profile;
    }

    const std::string folded = upperCopy(trimCopy(raw));
    if (folded.find("COLOR 3") != std::string::npos || folded.find("743K3") != std::string::npos)
        return profileById("743K3");
    if (folded.find("COLOR 2") != std::string::npos || folded.find("743C") != std::string::npos)
        return profileById("743C");
    if (folded.find("INKPAD 4") != std::string::npos || folded.find("743G") != std::string::npos)
        return profileById("743G");
    if (folded.find("INKPAD X") != std::string::npos || folded.find("1040") != std::string::npos)
        return profileById("1040");
    if (folded.find("INKPAD LITE") != std::string::npos || folded.find("970") != std::string::npos)
        return profileById("970");
    if (folded.find("3 PRO") != std::string::npos || folded.find("740-2") != std::string::npos || folded.find("740-3") != std::string::npos)
        return profileById("740-2");
    if (folded.find("INKPAD 3") != std::string::npos)
        return profileById("740");
    if (folded.find("INKPAD COLOR") != std::string::npos || folded.find("741") != std::string::npos)
        return profileById("741");
    if (folded.find("INKPAD") != std::string::npos)
        return profileById("840");
    return nullptr;
}

const DeviceProfile *profileById(const std::string &id)
{
    for (const auto &profile : kProfiles)
    {
        if (id == profile.id)
            return &profile;
    }
    return nullptr;
}

int fallbackDpi(int screenWidth, int screenHeight)
{
    const int longSide = std::max(screenWidth, screenHeight);
    const int shortSide = std::min(screenWidth, screenHeight);
    if (longSide >= 1800 && shortSide >= 1300)
        return 300;
    if (longSide <= 1300)
        return 150;
    if (shortSide >= 1000)
        return 212;
    return 250;
}

PageMetrics metricsFor(const DeviceProfile *profile, int screenWidth, int screenHeight)
{
    const int dpi = profile ? profile->dpi : fallbackDpi(screenWidth, screenHeight);
    PageMetrics metrics;
    metrics.textHeight = std::max(28, dpi * 14 / 72);
    metrics.margin = std::max(24, dpi / 6);
    metrics.lineGap = metrics.textHeight + metrics.textHeight / 5;
    metrics.fullRefreshEvery = (profile && profile->color) ? 96 : 0;
    return metrics;
}
