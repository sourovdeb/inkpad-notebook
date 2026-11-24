//------------------------------------------------------------------
// main.h
//
// Author:           JuanJakobo
// Date:             14.06.2020
// Description:      sets the inkview main handler
//-------------------------------------------------------------------

#include "inkview.h"
#include "eventHandler.h"
#include "log.h"
#include "device_match.h"

#include <string>

EventHandler *events = nullptr;

// main() is the only owner. Null the pointer before delete so a second
// EVT_EXIT cannot free the same object. EVT_HIDE does not delete: the
// process stays alive and a later key would otherwise hit a null handler,
// or a second delete if EXIT follows HIDE.
static void releaseEvents()
{
    EventHandler *dying = events;
    events = nullptr;
    delete dying;
}

/**
* Handles events and redirects them
*
* @param type event type
* @param par1 first argument of the event
* @param par2 second argument of the event
* @return int returns if the event was handled
*/
int Inkview_handler(int type, int par1, int par2)
{
    switch (type)
    {
        case EVT_INIT:
            {
                releaseEvents();
                events = new EventHandler();
                return 1;
            }
        case EVT_EXIT:
            {
                releaseEvents();
                return 1;
            }
        case EVT_HIDE:
            return 1;
        default:
            if (!events)
                return 0;
            return events->eventDistributor(type, par1, par2);
    }
}

int main()
{
    OpenScreen();
    SetOrientation(0);


    //draw startscreen
    auto textHeight = ScreenHeight() / 30;
    auto startscreenFont = OpenFont("LiberationMono", textHeight, FONT_BOLD);
    SetFont(startscreenFont, BLACK);
    const char *model = GetDeviceModel();
    const DeviceProfile *profile = matchProfile(model ? model : "");
    const char *title = profile ? profile->name : "Typewriter";
    DrawTextRect(0, (ScreenHeight() / 3) * 2, ScreenWidth(), textHeight, title, ALIGN_CENTER);
    CloseFont(startscreenFont);
    FullUpdate();

    InkViewMain(Inkview_handler);
    return 0;
}
