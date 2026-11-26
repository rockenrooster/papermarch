#pragma once
// Colours (GAME_DESIGN 9.2): the constant ones (the host, gold, the enemy's ink, the shrine) and ten worlds. 0xRRGGBB.

#include "core/types.h"

namespace pm {

// Constant in every world: the game's identity.
namespace col {
constexpr u32 kIvory = 0xFFFBEE, kIvoryShade = 0xE9DCC2;
constexpr u32 kScarlet = 0xF03A4C, kScarletDark = 0xB81F3A;
constexpr u32 kGold = 0xFFC83A, kGoldDark = 0xE09010;
constexpr u32 kInk = 0x292C55, kInkLight = 0x474D8C, kInkDeep = 0x14152C;
constexpr u32 kText = 0x33221A;
constexpr u32 kViolet = 0x8B5CF2, kVioletLight = 0xB79AFF;
constexpr u32 kMoss = 0x6E8C3A, kMossLight = 0x9AB45A;
constexpr u32 kShadow = 0x26140A;
constexpr u32 kWood = 0x8A5A36, kWoodLight = 0xD9A862, kWoodDark = 0xA8743A;
constexpr u32 kWhite = 0xFFFFFF;
constexpr u32 kCream = 0xFFF4D6;
}  // namespace col

struct WorldPalette {
    const char* name;
    u32 page;                     // the paper the land is glued on (Tidereach: the sea; Skyhold: the sky)
    u32 land, terrace1, terrace2;
    u32 treeDark, treeLight;      // the world's tree family
    u32 tree2Dark, tree2Light;    // its accent trees
    u32 water, waterLight, waterDark;
    u32 road, stitch;
    u32 accent, accentDark;       // the enemy's accent: flags, roofs
    u32 tableA, tableB, tableLine;
    int trees;                    // the tree family (game/dressing): 0 pines, 1 palms, 2 snow pines, 3 maples, 4 beach palms,
                                  // 5 blossoms, 6 mushrooms, 7 bushes, 8 ember trees, 9 lantern plums
};

constexpr int kWorldCount = 10;
const WorldPalette& worldPalette(int world);  // 1..10 (wraps)
const char* bossName(int world);              // the boss Keep of a world (GAME_DESIGN 15)

// WCAG relative luminance and contrast ratio (the palette lint), and the hue in degrees.
float luminance(u32 rgb);
float contrast(u32 a, u32 b);
float hueOf(u32 rgb);

}  // namespace pm
