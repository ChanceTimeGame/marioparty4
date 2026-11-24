# Contributing

Mario Party 4 is 100% matched, so there are no functions left to decompile. What this repository
still accepts are changes that make the source a better description of the original game without
changing the build.

## What is welcome

- **Names.** A function, variable, struct or field still called `fn_*`, `lbl_*` or `unk*` whose
  purpose you can show.
- **Types and prototypes.** A field with the wrong width or signedness, a pointer typed as an
  integer, a missing or wrong prototype. These matter beyond matching: ports compile this code,
  and a wrong type can match on the GameCube and still be a bug elsewhere.
- **Fakematches replaced by the real source.** A form that matches only because of what it does to
  the compiler (a stray `volatile`, a dummy local, a self-assignment, an empty `do {} while (0)`)
  usually hides a wrong type, a missing inline or a macro. Replacing it with a natural spelling
  that still matches is a real improvement.
- **Comments** that explain what code does, especially retail bugs and oddities.

## What is not

- **Port code.** No PC-port code, no `#ifdef` for other platforms, no host types. (Chance Time keeps
  its port changes in its own repository; see the README.)
- **Relabelling for its own sake.** Names follow this fork's history, not later upstream
  relabelling; rename only where the new name is better supported.
- **Unrelated reformatting.**

## Pull requests

Before opening one:

- `ninja` passes every `config/GMPE01_01/build.sha1` check from a clean build
  (`ninja -t clean && ninja`).
- `ninja progress` still reports 100%.

Small pull requests (one unit or one kind of change) are easiest to review.

If you want to help with matching, the work is in
[Mario Party 5](https://github.com/ChanceTimeGame/marioparty5) and
[Mario Party 6](https://github.com/ChanceTimeGame/marioparty6), which share this game's engine.
