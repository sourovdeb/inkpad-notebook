//------------------------------------------------------------------
// device_match.h
//
// InkPad / PocketBook model matching. No InkView dependency, so the
// same table can be checked on a desktop compiler.
//-------------------------------------------------------------------

#ifndef DEVICE_MATCH_H
#define DEVICE_MATCH_H

#include <string>

enum class InputPath
{
    ExtKeys,
    RawEvdev,
    NoBluetooth
};

struct DeviceProfile
{
    const char *id;
    const char *name;
    int width;
    int height;
    int dpi;
    bool color;
    bool bluetooth;
    bool wacom;
    InputPath input;
    const char *soc;
};

struct PageMetrics
{
    int textHeight;
    int margin;
    int lineGap;
    int fullRefreshEvery;
};

std::string normalizeModelId(const std::string &raw);
const DeviceProfile *matchProfile(const std::string &raw);
const DeviceProfile *profileById(const std::string &id);
int fallbackDpi(int screenWidth, int screenHeight);
PageMetrics metricsFor(const DeviceProfile *profile, int screenWidth, int screenHeight);

#endif
