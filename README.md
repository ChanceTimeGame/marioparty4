# Mario Party 4 decompilation (Chance Time)

The Mario Party 4 decompilation as the [Chance Time](https://github.com/ChanceTimeGame) PC port
builds it. It started as [mariopartyrd/marioparty4](https://github.com/mariopartyrd/marioparty4),
by way of the [mariopartyrd/partyboard](https://github.com/mariopartyrd/partyboard) port, and
carries the port's host changes (`TARGET_PC`, 64-bit and byte-order fixes) while still building
a byte-identical game: every unit matches, and the DOL and every REL pass the SHA-1 check for
`GMPE01_01` (USA revision 1).

Chance Time keeps its working copy of this tree in its own repository (`games/mp4/`); this
repository is a snapshot of it, refreshed from there. Its names and labels follow the port's
fork point and not later upstream relabelling.

This repository contains no game data. To build, place your own extracted disc in
`orig/GMPE01_01/` (`sys/` and `files/`), then:

```sh
python configure.py --version GMPE01_01
ninja
```

The configure step downloads the compilers and tools. Licence: CC0 1.0 (`LICENSE`), as upstream.
