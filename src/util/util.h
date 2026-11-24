//------------------------------------------------------------------
// util.h
//
// Author:           JuanJakobo
// Date:             04.08.2020
// Description:      Various utility methods
//-------------------------------------------------------------------

#ifndef UTIL
#define UTIL

#include "inkview.h"

#include "eventHandler.h"

#include <string>

enum class Action
{
    IWriteSecret,
    IReadSecret,
    IWriteString,
    IReadString,
    IWriteInt,
    IReadInt
};

const std::string CONFIG_PATH = CONFIG_FOLDER + "/config.cfg";

class Util
{
public:

    static int accessConfig(const Action &action, const std::string &name, int value)
    {
        iconfigedit *temp = nullptr;
        iconfig *config = OpenConfig(CONFIG_PATH.c_str(), temp);
        int returnValue = value;
        switch(action)
        {
            case Action::IWriteInt:
                WriteInt(config, name.c_str(), value);
                returnValue = 0;
                break;
            case Action::IReadInt:
                returnValue = ReadInt(config, name.c_str(), value);
                break;
            default:
                break;
        }
        CloseConfig(config);
        return returnValue;
    }

private:
    Util() {}
};
#endif
