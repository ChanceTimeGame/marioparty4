// Mario Party 4's side of the framework/game contract in port/game.h.
//
// This is the only place the port layer's view of the game is turned into
// actual Hudson engine symbols; everything above it goes through the four
// functions and two dimensions declared there.

#include "port/game.h"

extern "C" {
#include "game/card.h"
#include "game/disp.h"
#include "game/pad.h"
#include "game/process.h"
#include "game/object.h"
}

// Unpacks the data that lived inside the original DOL; see dolassets.cpp.
extern "C" void InitializeDol();
extern "C" void ShutdownDol();
// musyx (extern/musyx): its PC teardown frees sample memory, voices and streams.
extern "C" bool sndIsInstalled(void);
extern "C" void sndQuit(void);
extern "C" void msmSysShutdownAux(void);
extern "C" int game_main();

extern "C" {

int partyboard_game_fb_width(void) { return HU_FB_WIDTH; }
int partyboard_game_fb_height(void) { return HU_FB_HEIGHT; }

void partyboard_game_init_pad(void) { HuPadInit(); }
void partyboard_game_init_card(void) { HuCardInit(); }
void partyboard_game_init_assets(void) { InitializeDol(); }

int partyboard_game_main(void) { return game_main(); }

void partyboard_game_park_process(void)
{
    if (HuPrcCurrentGet() == NULL) {
        return;
    }
    for (;;) {
        HuPrcVSleep();
    }
}

void partyboard_game_shutdown(void)
{
    omDLLFreeAll();
    HuPrcFreeAll();
    ShutdownDol();
    msmSysShutdownAux();
    if (sndIsInstalled()) {
        sndQuit();
    }
}

}
