#ifndef PORT_GAME_SETTINGS_H_
#define PORT_GAME_SETTINGS_H_

// Mario Party 4's own port settings, as game code sees them.
//
// The framework's shared accessors live in port/settings.h; these two are this
// title's alone, so they live here rather than adding Mario Party 4 names to a
// header every game includes. The CVars behind them are declared and registered
// in games/mp4/src/port/settings.cpp, and each name has to appear in
// games/mp4/dol.def to be callable from the DOL's game code or from a REL.

#ifdef __cplusplus
extern "C" {
#endif

// Cheats: make every minigame, and Bowser's Gnarly Party, available without
// having earned them. Read by mentDll and by option/record.c.
//
// Both used to be declared in port/settings.h, which meant the framework's own
// header named a Mario Party 4 board -- and Mario Party 5 had to define both,
// export them, and return false, for an unlockable and a board it does not
// have. See the note on per-title settings in CLAUDE.md.
bool partyboard_settings_unlock_all_minigames(void);
bool partyboard_settings_unlock_bowsers_gnarly_party(void);

// Development aids for scripted board runs (minigame sweeps). Every one defaults
// to off (-1 / 0); restore them afterwards. Read only by DOL board code.
//
// game.devAllCom = 1: a new game's first board entry (BoardObjectSetup) makes all four players CPUs, so a party
// started through the real setup menus then plays itself with no input (and
// no window focus).
int partyboard_settings_dev_all_com(void);
// game.devForceTurnMax: the party's turn count, overriding the setup menu
// (BoardPartyConfigSet). -1 keeps the menu's choice.
int partyboard_settings_dev_force_turn_max(void);
// game.devForceMinigame: a minigame number (401 = m401) that the turn-end
// minigame setup plays instead of its roulette pick, if it is a 4P, 1vs3, 2vs2,
// Bowser or Battle minigame. The space colours are rewritten to a legal layout
// for that type first, as no colour roll produced it; Bowser and Battle ones get
// the 4P layout, which tests the overlay but not Bowser's or the Battle space's
// board event (no coin stakes). -1 = off.
int partyboard_settings_dev_force_minigame(void);
// game.devMinigameSweep = 1: devForceMinigame is the first of a sweep; each
// later turn-end plays the next such minigame in mgInfoTbl order, of the same
// class as the first (4P/1vs3/2vs2, or Bowser/Battle), until
// the table ends (then the board picks normally again).
int partyboard_settings_dev_minigame_sweep(void);
// game.devTurbo = 1: run with the frame limiter off from boot, as holding the
// turbo key does, but with no key (no window focus needed). Pair it with
// video.enableVsync=false.
int partyboard_settings_dev_turbo(void);
// game.devAutoMenu = 1: pad 1 presses Start and A in turn (game/pad.c) from
// boot until a new game first enters its board, so a scripted run gets
// through the title and setup menus with retail's defaults and no real input
// or window focus. Pair with devAllCom, or the board waits for a human.
int partyboard_settings_dev_auto_menu(void);

#ifdef __cplusplus
}
#endif

#endif // PORT_GAME_SETTINGS_H_
