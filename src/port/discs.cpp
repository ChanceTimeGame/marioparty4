// Mario Party 4's half of disc validation: which disc IDs this port accepts,
// and the XXH128 hashes of the images known to be good.
//
// The validation logic lives in port/src/platform/gc/iso_validate.cpp and is
// the same for any GameCube or Wii title; only this table is per game.
//
// This build is compiled for GMPE01 revision 1. The disc header's revision is
// checked by the shared validator, preventing a same-title but wrong-address
// boot of a different regional or revisioned image.

#include <port/iso_validate.hpp>

#include <array>

namespace partyboard::iso {

namespace {
const auto KNOWN_DISCS = std::to_array<KnownDisc>({
    {"GMPE01", Platform::GameCube, Region::NorthAmerica, 1, {"0f45c2365f5812d970188b7df0b1e2ff"}},
});
}  // namespace

std::span<const KnownDisc> known_discs() { return KNOWN_DISCS; }

}  // namespace partyboard::iso
