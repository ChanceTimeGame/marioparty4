# Mario Party 4 (US) decompilation

A complete matching decompilation of Mario Party 4 (GameCube, US revision 1, `GMPE01_01`). Built
with the original compilers, it produces a `main.dol` and RELs byte-identical to the retail game.

**Status: 100% matched.** All 10,985 functions and all 616 units match, code and data, and every
output passes `config/GMPE01_01/build.sha1`. Nothing is left to decompile.

This repository started as [mariopartyrd/marioparty4](https://github.com/mariopartyrd/marioparty4),
by way of the [mariopartyrd/partyboard](https://github.com/mariopartyrd/partyboard) port; its names
and labels follow that fork point, not later upstream relabelling. Thanks to everyone who worked on
both.

## Why this fork exists: Chance Time

This fork is the source of truth for Mario Party 4 in **Chance Time**, a native PC port of the N64
and GameCube Mario Party games. Chance Time ships no game data: players supply their own ROM or
disc.

How the two fit together:

- **This repository is a pure matching decompilation.** It holds decompiled game code only, with no
  PC-port code, no `#ifdef` for a PC build and no host types. Every commit passes the `build.sha1`
  check against the retail disc.
- **Chance Time uses it as a submodule** and keeps its own changes (64-bit fixes, byte swapping,
  rendering and audio adapters) as a patch series applied on top. So the port is always built from
  code that is provably the original game logic, and the decompilation never carries port changes.
- **Mario Party 4 was the first.** Chance Time's other titles follow the same model as they reach
  100%. Mario Party 5 and 6 share this game's engine and SDK, and their decompilations
  ([ChanceTimeGame/marioparty5](https://github.com/ChanceTimeGame/marioparty5),
  [ChanceTimeGame/marioparty6](https://github.com/ChanceTimeGame/marioparty6)) start many
  functions from the matched code here. If you want to help with matching, that is where the work
  is.

## Contributing

The decompilation is finished, so this repository takes genuine decompilation fixes only: better
names, correct types and prototypes, comments, and a fakematch replaced by the real source. Every
change must still pass `build.sha1`. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Building

This repository contains no game assets or assembly. You need your own copy of the game.

1. Install [Python 3](https://www.python.org/downloads/) and
   [ninja](https://github.com/ninja-build/ninja/releases) (`pip install ninja` works). On Linux
   x86-64, [wibo](https://github.com/decompals/wibo) is downloaded for the Windows compilers; on
   other Linux platforms and on macOS, install Wine. On Windows, use native tools: WSL is not
   needed.
2. `git clone --recursive https://github.com/ChanceTimeGame/marioparty4`.
3. Put your extracted disc (`sys/` and `files/`) in `orig/GMPE01_01/`.
4. `python configure.py --version GMPE01_01`, then `ninja`. The build checks every output against
   `config/GMPE01_01/build.sha1`.

Only `GMPE01_01` is verified at 100%; the other versions in `config/` are configured but not
checked.

Licence: CC0 1.0 (`LICENSE`), as upstream.
