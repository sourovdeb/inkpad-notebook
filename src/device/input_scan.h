//------------------------------------------------------------------
// input_scan.h
//
// Reads /proc/bus/input/devices text. Keeps Bluetooth and USB HID
// keyboards, and rejects the InkPad touchscreen, gpio keys and Wacom
// digitizer that the old "event id > 6" test both missed and invited.
//-------------------------------------------------------------------

#ifndef INPUT_SCAN_H
#define INPUT_SCAN_H

#include <iosfwd>
#include <string>
#include <vector>

struct ScannedDevice
{
    std::string name;
    std::string sysfs;
    std::string uniq;
    std::string handlers;
    int eventID = -1;
    int bus = -1;
};

std::vector<ScannedDevice> scanInputDevices(std::istream &input);
bool isKeyboardCandidate(const ScannedDevice &device);

#endif
