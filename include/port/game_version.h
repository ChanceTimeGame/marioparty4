#pragma once

// Mario Party 4's disc revisions.
//
// The values are indices into the same 0-5 ordering as VERSIONS in
// configure.py, which is also how the per-version address tables in
// dolassets.cpp are indexed -- so these must not be renumbered independently.

#include <types.h>
#include "version.h"

namespace partyboard::version {

enum class GameVersion : u8 {
    UsaRev0 = VERSION_NO_ENG0,
    UsaRev1 = VERSION_NO_ENG1,
    PalRev0 = VERSION_NO_PAL0,
    PalRev1 = VERSION_NO_PAL1,
    PalRev2 = VERSION_NO_PAL2,
    Jpn = VERSION_NO_JP,
};

}  // namespace partyboard::version
