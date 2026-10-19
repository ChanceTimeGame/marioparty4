# Mario Party 4 decompilation

A matching decompilation of Mario Party 4. It builds a byte-identical game: every unit matches,
and the DOL and every REL pass the SHA-1 check for `GMPE01_01` (USA revision 1).

It started as [mariopartyrd/marioparty4](https://github.com/mariopartyrd/marioparty4), by way of
the [mariopartyrd/partyboard](https://github.com/mariopartyrd/partyboard) port; its names and labels
follow that fork point, not later upstream relabelling. It contains decompiled code only: the
changes the [Chance Time](https://github.com/ChanceTimeGame) PC port needs are kept out of it.

This repository contains no game data. To build, place your own extracted disc in
`orig/GMPE01_01/` (`sys/` and `files/`), then:

```sh
python configure.py --version GMPE01_01
ninja
```

The configure step downloads the compilers and tools. Licence: CC0 1.0 (`LICENSE`), as upstream.
