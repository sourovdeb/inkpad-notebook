#include "device_match.h"
#include "input_scan.h"

#include <iostream>
#include <sstream>
#include <cstdlib>

static int failures = 0;

static void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << "\n";
        ++failures;
    }
}

static const char *kInkpadProc = R"PROC(
I: Bus=0019 Vendor=0001 Product=0001 Version=0100
N: Name="gpio-keys"
P: Phys=gpio-keys/input0
S: Sysfs=/devices/platform/gpio-keys/input/input0
U: Uniq=
H: Handlers=kbd event0

I: Bus=0018 Vendor=0000 Product=0000 Version=0000
N: Name="cyttsp5_mt"
S: Sysfs=/devices/platform/cyttsp5/input/input1
H: Handlers=event1

I: Bus=0018 Vendor=056a Product=0000 Version=0000
N: Name="Wacom I2C Digitizer"
S: Sysfs=/devices/platform/wacom/input/input2
H: Handlers=mouse0 event7

I: Bus=0005 Vendor=046d Product=b342 Version=011b
N: Name="Logitech K380"
S: Sysfs=/devices/platform/bt/input/input3
U: Uniq=f4:73:35:00:11:22
H: Handlers=sysrq kbd event3

I: Bus=0003 Vendor=04f2 Product=0111 Version=0110
N: Name="USB HID Keyboard"
S: Sysfs=/devices/platform/usb/input/input4
H: Handlers=kbd event12
)PROC";

int main()
{
    expect(normalizeModelId("PB743G") == "743G", "pb743g");
    expect(normalizeModelId("PocketBook 743 (PB743G)") == "743G", "parens");
    expect(normalizeModelId("PocketBook 740") == "740", "740");
    expect(normalizeModelId("PB740-2") == "740-2", "740-2");
    expect(normalizeModelId("Reader InkPad") == "840", "legacy inkpad");
    expect(normalizeModelId("InkPad Color 3") == "743K3", "color 3 name");
    expect(normalizeModelId("PB743K3") == "743K3", "pb743k3 exact model");
    expect(normalizeModelId("InkPad Color") == "741", "color 1 not color 3");

    const auto *inkpad4 = matchProfile("PB743G");
    expect(inkpad4 && inkpad4->input == InputPath::ExtKeys, "inkpad 4 uses ext keys");
    expect(inkpad4 && !inkpad4->color && inkpad4->bluetooth, "inkpad 4 panel");
    expect(inkpad4 && inkpad4->width == 1404 && inkpad4->height == 1872, "inkpad 4 pixels");

    const auto *color3 = matchProfile("PocketBook InkPad Color 3 (PB743K3)");
    expect(color3 && color3->color && color3->input == InputPath::ExtKeys, "color 3");
    expect(color3 && std::string(color3->soc) == "B300", "color 3 soc");
    expect(color3 && color3->width == 1404 && color3->height == 1872, "color 3 pixels");

    const auto *inkpad3 = matchProfile("PB740");
    expect(inkpad3 && inkpad3->input == InputPath::RawEvdev && inkpad3->wacom, "inkpad 3 raw+wacom");

    const auto *lite = matchProfile("PB970");
    expect(lite && lite->input == InputPath::NoBluetooth && !lite->bluetooth, "lite has no radio");

    const auto *original = matchProfile("Reader InkPad");
    expect(original && original->id == std::string("840") && !original->bluetooth, "original inkpad");

    const auto *color = matchProfile("PB633");
    expect(color && color->input == InputPath::RawEvdev && color->width == 1072, "pb633 stays raw");

    const auto *pro = matchProfile("PB740-2");
    expect(pro && std::string(pro->name) == "InkPad 3 Pro", "3 pro");
    expect(matchProfile("InkPad 3") != matchProfile("InkPad"), "inkpad 3 is not the 2014 inkpad");

    const auto metrics = metricsFor(inkpad4, 1404, 1872);
    expect(metrics.textHeight == 300 * 14 / 72, "14pt at 300 dpi");
    expect(metrics.margin >= 24, "margin");
    expect(metricsFor(color3, 1404, 1872).fullRefreshEvery == 96, "kaleido refresh");
    expect(metricsFor(inkpad4, 1404, 1872).fullRefreshEvery == 0, "carta partial");
    expect(metricsFor(nullptr, 825, 1200).textHeight >= 28, "fallback dpi");

    std::stringstream sample(kInkpadProc);
    const auto devices = scanInputDevices(sample);
    expect(devices.size() == 5, "five nodes parsed");

    int keyboards = 0;
    bool sawLowEventKeyboard = false;
    bool sawWacom = false;
    for (const auto &device : devices)
    {
        if (device.name.find("Wacom") != std::string::npos)
        {
            sawWacom = true;
            expect(!isKeyboardCandidate(device), "wacom event 7 is not a keyboard");
        }
        if (isKeyboardCandidate(device))
        {
            ++keyboards;
            if (device.eventID == 3)
                sawLowEventKeyboard = true;
        }
    }
    expect(sawWacom, "wacom present");
    expect(keyboards == 2, "bt and usb keyboards only");
    expect(sawLowEventKeyboard, "event3 keyboard kept");

    if (failures)
    {
        std::cerr << failures << " failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "ok\n";
    return EXIT_SUCCESS;
}
