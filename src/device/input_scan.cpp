//------------------------------------------------------------------
// input_scan.cpp
//-------------------------------------------------------------------

#include "input_scan.h"

#include <cctype>
#include <sstream>

namespace
{

std::string afterEquals(const std::string &line)
{
    const auto pos = line.find('=');
    if (pos == std::string::npos)
        return {};
    std::string value = line.substr(pos + 1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && (value.back() == '\r' || std::isspace(static_cast<unsigned char>(value.back()))))
        value.pop_back();
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
    return value;
}

std::string lowerCopy(std::string value)
{
    for (char &c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool contains(const std::string &haystack, const char *needle)
{
    return haystack.find(needle) != std::string::npos;
}

int parseEventId(const std::string &handlers)
{
    const auto pos = handlers.find("event");
    if (pos == std::string::npos)
        return -1;
    std::string digits;
    for (size_t i = pos + 5; i < handlers.size() && std::isdigit(static_cast<unsigned char>(handlers[i])); ++i)
        digits.push_back(handlers[i]);
    if (digits.empty())
        return -1;
    try
    {
        return std::stoi(digits);
    }
    catch (...)
    {
        return -1;
    }
}

int parseBus(const std::string &line)
{
    const auto pos = line.find("Bus=");
    if (pos == std::string::npos)
        return -1;
    const std::string digits = line.substr(pos + 4, 4);
    if (digits.size() != 4)
        return -1;
    try
    {
        return std::stoi(digits, nullptr, 16);
    }
    catch (...)
    {
        return -1;
    }
}

bool deniedName(const std::string &name)
{
    return contains(name, "gpio")
        || contains(name, "cyttsp")
        || contains(name, "goodix")
        || contains(name, "zforce")
        || contains(name, "wacom")
        || contains(name, "digitizer")
        || contains(name, "touch")
        || contains(name, "hall")
        || contains(name, "accel")
        || contains(name, "mma8")
        || contains(name, "light")
        || contains(name, "pmic")
        || contains(name, "onkey")
        || contains(name, "axp")
        || contains(name, "sensor");
}

}

std::vector<ScannedDevice> scanInputDevices(std::istream &input)
{
    std::vector<ScannedDevice> devices;
    ScannedDevice current;
    std::string line;
    bool seen = false;

    const auto flush = [&]() {
        if (!seen)
            return;
        if (current.eventID < 0)
            current.eventID = parseEventId(current.handlers);
        if (!current.name.empty() && current.eventID >= 0)
            devices.push_back(current);
        current = {};
        seen = false;
    };

    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
        {
            flush();
            continue;
        }

        seen = true;
        switch (line.front())
        {
            case 'I':
                current.bus = parseBus(line);
                break;
            case 'N':
                current.name = afterEquals(line);
                break;
            case 'S':
                current.sysfs = afterEquals(line);
                break;
            case 'U':
                current.uniq = afterEquals(line);
                break;
            case 'H':
                current.handlers = afterEquals(line);
                current.eventID = parseEventId(current.handlers);
                break;
            default:
                break;
        }
    }
    flush();
    return devices;
}

bool isKeyboardCandidate(const ScannedDevice &device)
{
    if (device.eventID < 0 || device.name.empty())
        return false;

    const std::string name = lowerCopy(device.name);
    if (deniedName(name))
        return false;

    const std::string handlers = lowerCopy(device.handlers);
    const bool bluetooth = device.bus == 0x0005;
    const bool usbHid = device.bus == 0x0003;
    const bool namedKeyboard = contains(name, "keyboard") || contains(name, "kbd");
    const bool hasKbdHandler = contains(handlers, "kbd");

    if (bluetooth || usbHid)
        return true;
    if (namedKeyboard && device.bus != 0x0019)
        return true;
    if (hasKbdHandler && device.bus != 0x0019 && device.bus != 0x0018)
        return true;
    return false;
}
