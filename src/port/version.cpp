// Mario Party 4's disc identity: which disc header means which revision, and
// which region each revision belongs to. The framework asks; see
// port/include/port/port_version.h.

#include "port/port_version.h"

#include "aurora/lib/logging.hpp"

#include <string_view>

namespace partyboard::version {

namespace {
aurora::Module Mp4VersionLog("partyboard::version::mp4");
}

using namespace std::string_view_literals;

GameVersion detectVersion(const DVDDiskID& diskId) {
    const std::string_view game(diskId.gameName, sizeof(diskId.gameName));

    // This build is compiled for GMPE01 revision 1 only (docs/PRODUCT_VISION.md "Regions and
    // revisions"); discs.cpp rejects every other dump before boot.
    if (game == "GMPE"sv) {
        return GameVersion::UsaRev1;
    }

    Mp4VersionLog.fatal("Unknown/unsupported game version in disc: {}", game);
    return GameVersion::UsaRev1;
}

Region regionOf(GameVersion version) {
    switch (version) {
    case GameVersion::PalRev0:
    case GameVersion::PalRev1:
    case GameVersion::PalRev2:
        return Region::Pal;
    case GameVersion::Jpn:
        return Region::Jpn;
    case GameVersion::UsaRev0:
    case GameVersion::UsaRev1:
    default:
        return Region::Usa;
    }
}

}  // namespace partyboard::version
