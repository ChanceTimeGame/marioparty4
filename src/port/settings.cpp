// Mario Party 4's own settings: the parts of the settings tree that only make
// sense for this title. Everything a port of any Mario Party would want --
// video, audio, backend, and the general game preferences -- lives in
// port/src/core/settings.cpp instead.
//
// This file implements the four hooks port/settings.h declares.

#include "port/settings.h"
#include "port/game_settings.h"

#include "port/config.hpp"

#include <array>

namespace partyboard {

namespace {

struct Mp4Settings {
    // Quality of life
    ConfigVar<bool> enableQuickTransform{"game.enableQuickTransform", false};

    // Cheats
    ConfigVar<bool> infiniteHearts{"game.infiniteHearts", false};
    ConfigVar<bool> unlockAllMinigames{"game.unlockAllMinigames", false};
    ConfigVar<bool> unlockBowsersGnarlyParty{"game.unlockBowsersGnarlyParty", false};

    // Development aids (include/port/game_settings.h)
    ConfigVar<int> devAllCom{"game.devAllCom", 0};
    ConfigVar<int> devForceTurnMax{"game.devForceTurnMax", -1};
    ConfigVar<int> devForceMinigame{"game.devForceMinigame", -1};
    ConfigVar<int> devMinigameSweep{"game.devMinigameSweep", 0};
    ConfigVar<int> devTurbo{"game.devTurbo", 0};
    ConfigVar<int> devAutoMenu{"game.devAutoMenu", 0};
};

Mp4Settings g_mp4Settings;

// Keys are unchanged from when these lived in UserSettings, so existing
// settings files keep loading.
const auto CHEATS = std::to_array<GameCheat>({
    {"Minigames", "Unlock All Minigames",
     "Treat every minigame as unlocked when the game checks minigame availability.",
     &g_mp4Settings.unlockAllMinigames},
    {"Boards", "Unlock Bowser's Gnarly Party",
     "Treat Bowser's Gnarly Party as unlocked when the game checks board availability.",
     &g_mp4Settings.unlockBowsersGnarlyParty},
    // Infinite Hearts is registered and wired up but not offered yet; it was
    // commented out of the Cheats tab before this file existed.
});

}  // namespace

void registerGameSettings() {
    Register(g_mp4Settings.enableQuickTransform);
    Register(g_mp4Settings.infiniteHearts);
    Register(g_mp4Settings.unlockAllMinigames);
    Register(g_mp4Settings.unlockBowsersGnarlyParty);
    Register(g_mp4Settings.devAllCom);
    Register(g_mp4Settings.devForceTurnMax);
    Register(g_mp4Settings.devForceMinigame);
    Register(g_mp4Settings.devMinigameSweep);
    Register(g_mp4Settings.devTurbo);
    Register(g_mp4Settings.devAutoMenu);
}

void resetGameSettingsForSpeedrunMode() {
    g_mp4Settings.infiniteHearts.setValue(false);
}

void applyGamePreset(Preset preset) {
    // Classic deliberately leaves this alone rather than forcing it off, which
    // is what the preset did before it moved here.
    if (preset == Preset::Enhanced) {
        g_mp4Settings.enableQuickTransform.setValue(true);
    }
}

std::span<const GameCheat> gameCheats() { return CHEATS; }

}  // namespace partyboard

extern "C" {

bool partyboard_settings_unlock_all_minigames(void)
{
    return partyboard::g_mp4Settings.unlockAllMinigames.getValue();
}

bool partyboard_settings_unlock_bowsers_gnarly_party(void)
{
    return partyboard::g_mp4Settings.unlockBowsersGnarlyParty.getValue();
}

int partyboard_settings_dev_all_com(void)
{
    return partyboard::g_mp4Settings.devAllCom.getValue();
}

int partyboard_settings_dev_force_turn_max(void)
{
    return partyboard::g_mp4Settings.devForceTurnMax.getValue();
}

int partyboard_settings_dev_force_minigame(void)
{
    return partyboard::g_mp4Settings.devForceMinigame.getValue();
}

int partyboard_settings_dev_minigame_sweep(void)
{
    return partyboard::g_mp4Settings.devMinigameSweep.getValue();
}

int partyboard_settings_dev_turbo(void)
{
    return partyboard::g_mp4Settings.devTurbo.getValue();
}

int partyboard_settings_dev_auto_menu(void)
{
    return partyboard::g_mp4Settings.devAutoMenu.getValue();
}
}
