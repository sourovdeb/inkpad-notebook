//------------------------------------------------------------------
// eventHandler.cpp
//
// Author:           JuanJakobo
// Date:             22.04.2021
//
//-------------------------------------------------------------------

#include "eventHandler.h"
#include "inkview.h"

#include "mainMenu.h"

#include "log.h"
#include "util.h"

#include "view.h"

#include "textView.h"
#include "errorView.h"
#include "device_match.h"
#include "input_scan.h"

#include "fileBrowser.h"
#include "fileModel.h"

#include <string>
#include <fstream>
#include <sstream>
#include <climits>


using std::string;
using std::vector;

EventHandler *EventHandler::_eventHandlerStatic = nullptr;


EventHandler::EventHandler()
{
    _eventHandlerStatic = this;

    if (iv_access(CONFIG_FOLDER.c_str(), W_OK) != 0)
        iv_mkdir(CONFIG_FOLDER.c_str(), 0777);
    if (iv_access(ARTICLE_FOLDER.c_str(), W_OK) != 0)
        iv_mkdir(ARTICLE_FOLDER.c_str(), 0777);

    SetOrientation(Util::accessConfig(Action::IReadInt, "orientation",0));
    createInputEvent();
}

EventHandler::~EventHandler()
{
    if (_eventHandlerStatic == this)
        _eventHandlerStatic = nullptr;
    if (!_currentDevice.inputNodePath.empty())
        system(("/mnt/secure/su rm -f " + _currentDevice.inputNodePath).c_str());
}

int EventHandler::eventDistributor(const int type, const int par1, const int par2)
{
    // Firmware 6 InkPads deliver Bluetooth keys as EVT_KEY*_EXT (40/41/42).
    // PB743K3 firmware may populate par2, so it is diagnostic data only.
    if (_useExtKeys && _currentView == Views::TEXT
        && (type == EVT_KEYPRESS_EXT || type == EVT_KEYRELEASE_EXT || type == EVT_KEYREPEAT_EXT)
        && par1 > 0 && par1 < 240)
    {
        if (_extEventCount < 32)
        {
            Log::writeInfoLog("ext-key type=" + std::to_string(type)
                + " code=" + std::to_string(par1)
                + " par2=" + std::to_string(par2));
        }
        ++_extEventCount;
        if (auto view = dynamic_cast<TextView *>(_currentViews.get()))
        {
            const int value = (type == EVT_KEYRELEASE_EXT) ? 0 : (type == EVT_KEYREPEAT_EXT ? 2 : 1);
            return view->onLinuxKey(par1, value);
        }
    }

    if (ISPOINTEREVENT(type))
        return EventHandler::pointerHandler(type, par1, par2);
    else if (ISKEYEVENT(type))
        return EventHandler::keyHandler(type, par1, par2);
    return 0;
}

void EventHandler::mainMenuHandlerStatic(const int index)
{
    if (_eventHandlerStatic)
        _eventHandlerStatic->mainMenuHandler(index);
}

void EventHandler::mainMenuHandler(const int index)
{
    switch (index)
    {
        //start input mode
        case 101:
            {
                if (_currentDevice.name.empty()){
                    createInputEvent();
                }else{
                    getLocalFiles(ARTICLE_FOLDER);
                }
                break;
            }
            //set orientation
        case 102:
            {
                // Set screen orientation: 0=portrait, 1=landscape 90, 2=landscape 270, 3=portrait 180
                int orientation = []{
                    switch(DialogSynchro(ICON_QUESTION, "Action", "How to turn?", "Portrait", "Landscape 270", "Cancel"))
                    {
                        case 1:
                            return 0;
                            break;
                        case 2:
                            return 2;
                            break;
                        default:
                            return -1;
                            break;
                    }
                }();
                if (GetOrientation() != orientation && orientation != -1)
                {
                    SetOrientation(orientation);
                    Util::accessConfig(Action::IWriteInt, "orientation",orientation);
                    _currentViews->setContentRect(_menu.getContentRect());
                    _currentViews->draw();
                }
                break;
            }
            //create File
        case 103:
            {
                _tempKeyboard = "Filename";
                _tempKeyboard.resize(60);
                if (iv_access(_currentPath.c_str(), W_OK) == 0)
                    OpenKeyboard(_tempKeyboard.c_str(), &_tempKeyboard[0], 60 - 1, KBD_NORMAL, &keyboardHandlerStatic);
                else
                    Message(ICON_INFORMATION,"Error", "Can not write to current location",2000);
                break;
            }
            //Exit
        case 104:
            {
                if (IsBluetoothEnabled() == 1){
                    Message(ICON_INFORMATION,"Information","Disabling Bluetooth",2000);
                    SetBluetoothOff();
                }
                CloseApp();
                break;
            }
        default:
            break;
    }
}

int EventHandler::pointerHandler(const int type, const int par1, const int par2)
{
    if (type == EVT_POINTERUP)
    {
        if (_currentViews->checkIfEntryClicked(par1,par2))
        {

            switch(_currentView)
            {
                case Views::DEVICE:
                    {
                        if (auto view = dynamic_cast<ListView<DeviceViewEntry,Device>*>(_currentViews.get()))
                        {
                            if (createDevice(view->getCurrentEntry()))
                            {
                                getLocalFiles(ARTICLE_FOLDER);
                            }
                            else
                            {
                                view->invertCurrentEntryColor();
                            }
                        }
                        break;
                    }
                case Views::FIL:
                    {
                        if (auto view = dynamic_cast<ListView<FileViewEntry,FileItem>*>(_currentViews.get()))
                        {
                            if (view->getCurrentEntry().type == Type::FFOLDER)
                            {
                                auto path = view->getCurrentEntry().path;
                                getLocalFiles(path);
                            }
                            else
                            {
                                Device writing = _currentDevice;
                                if (_useExtKeys)
                                    writing.inputNodePath.clear();
                                _currentViews.reset(new TextView(_menu.getContentRect(), writing, view->getCurrentEntry().path, _profile));
                                _currentView = Views::TEXT;

                            }
                        }
                        break;
                    }
                default:
                    break;
            }
            return 0;
        }
    }
    return 1;
}

int EventHandler::keyHandler(const int type, const int par1, const int par2)
{
    //TODO lock
    if (type == EVT_KEYPRESS)
    {
        switch(par1)
        {
            case 23:
            case 26:
                return _menu.createMenu(EventHandler::mainMenuHandlerStatic, _currentView);
            case 24:
            case 28:
                (GetOrientation() == 0 || GetOrientation() == 3) ? _currentViews->prevPage() : _currentViews->nextPage();
                break;
            case 25:
            case 29:
                (GetOrientation() == 0 || GetOrientation() == 3) ? _currentViews->nextPage() : _currentViews->prevPage();
                break;
            default:
                return 1;
        }
    }

    return 0;
}

void EventHandler::enableBluetooth()
{
    if (IsBluetoothEnabled() == 0)
        SetBluetoothOn();
    if (IsBluetoothAwake() == 0)
        BluetoothWakeUp();
}

void EventHandler::createInputEvent()
{
    const char *model = GetDeviceModel();
    _profile = matchProfile(model ? model : "");
    _useExtKeys = _profile && _profile->input == InputPath::ExtKeys;
    Log::writeInfoLog("model=" + string(model ? model : "unknown"));
    Log::writeInfoLog("profile=" + string(_profile ? _profile->id : "unknown")
        + " input=" + (_useExtKeys ? "extended" : "raw"));

    if (_profile && _profile->input == InputPath::NoBluetooth)
    {
        _currentViews.reset(new ErrorView(_menu.getContentRect(), "This InkPad has no Bluetooth radio, so an external keyboard cannot be attached. InkPad Lite and the 2014 InkPad are in this group."));
        _currentView = Views::ERROR;
        return;
    }

    enableBluetooth();
    if (IsBluetoothEnabled() != 1)
    {
        Message(ICON_ERROR, "Error", "Could not enable Bluetooth", 2000);
        _currentViews.reset(new ErrorView(_menu.getContentRect(), "Bluetooth did not turn on. Enable it in Settings, then choose Start input mode from the menu."));
        _currentView = Views::ERROR;
        return;
    }

    if (_useExtKeys)
    {
        getLocalFiles(ARTICLE_FOLDER);
        return;
    }

    std::ifstream infile("/proc/bus/input/devices");
    const auto scanned = scanInputDevices(infile);
    vector<Device> devices;
    for (const auto &item : scanned)
    {
        if (!isKeyboardCandidate(item))
            continue;
        Device device;
        device.name = item.name;
        device.sysfs = item.sysfs;
        device.uniq = item.uniq;
        device.eventID = item.eventID;
        devices.push_back(device);
    }

    switch (devices.size())
    {
        case 1:
            if (createDevice(devices.at(0)))
            {
                getLocalFiles(ARTICLE_FOLDER);
                break;
            }
            [[fallthrough]];
        case 2 ... INT_MAX:
            {
                _currentViews.reset(new ListView<DeviceViewEntry, Device>(_menu.getContentRect(), devices));
                _currentView = Views::DEVICE;
                break;
            }
        default:
            {
                _currentViews.reset(new ErrorView(_menu.getContentRect(), "No external keyboard found. Pair one with bluetoothctl, then open the menu and choose Start input mode. Built-in buttons and the stylus are ignored."));
                _currentView = Views::ERROR;
            }
    }
}

bool EventHandler::createDevice(const Device &device)
{
    _currentDevice = device;
    if (_currentDevice.name.empty())
        createInputEvent();

    if (_useExtKeys)
        return true;

    if (iv_access("/mnt/secure/su", X_OK) != 0)
    {
        Message(ICON_ERROR, "Error", "No root access available.", 2000);
        Log::writeInfoLog("no root access");
        return false;
    }

    std::ifstream infile("/sys" + _currentDevice.sysfs + "/event" + std::to_string(_currentDevice.eventID) + "/uevent");
    string line;
    string major, minor;

    while (std::getline(infile, line))
    {
        if (line.find("MAJOR") != std::string::npos)
            major = line.substr(line.find('=') + 1);
        if (line.find("MINOR") != std::string::npos)
            minor = line.substr(line.find('=') + 1);
    }

    const auto isDigits = [](const string &value) {
        return !value.empty() && value.find_first_not_of("0123456789") == string::npos;
    };
    if (!isDigits(major) || !isDigits(minor))
    {
        Message(ICON_ERROR, "Error", "Invalid input device numbers.", 2000);
        return false;
    }

    _currentDevice.inputNodePath = "/tmp/pbtypewriter-event" + std::to_string(_currentDevice.eventID);
    const string su = "/mnt/secure/su";
    string systemCommand = su + " rm -f " + _currentDevice.inputNodePath;
    system(systemCommand.c_str());
    systemCommand = su + " mknod -m 666 " + _currentDevice.inputNodePath + " c " + major + " " + minor;
    const int created = system(systemCommand.c_str());
    if (created != 0)
    {
        Message(ICON_ERROR, "Error", "Could not create link to input.", 2000);
        Log::writeInfoLog("Could not create link to input. System return code " + std::to_string(created));
        return false;
    }

    return true;
}

void EventHandler::getLocalFiles(const string &path)
{

    _currentPath = path;
    vector<FileItem> files = FileBrowser::getFileStructure(path,true,true);

    _currentViews.reset(new ListView<FileViewEntry,FileItem>(_menu.getContentRect(),files));
    _currentView = Views::FIL;
}

void EventHandler::keyboardHandlerStatic(char *text)
{
    if (_eventHandlerStatic)
        _eventHandlerStatic->keyboardHandler(text);
}
void EventHandler::keyboardHandler(char *text)
{
    if (!text)
        return;

    string path(text);
    if (path.empty())
        return;

    path = _currentPath + '/' + path;
    std::ofstream output(path);
    getLocalFiles(_currentPath);
}
