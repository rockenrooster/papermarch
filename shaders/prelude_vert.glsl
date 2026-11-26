precision highp float;
precision highp int;

// A unit's radius by team and tier (TECH_SPEC 5.4), by selects rather than a const array: the OnePlus 12R's Adreno 740
// driver (V@0676, Android 16) returns no usable value for a const float array indexed by a value read from an attribute,
// which left every wisp and gloom with no size there (2026-11-20).
float unitRadius(bool gloom, int tier) {
    vec4 r = gloom ? vec4(0.29, 0.40, 0.55, 0.72) : vec4(0.26, 0.36, 0.50, 0.65);
    // Tiers 4 and 5 are a cub (1.1 m) and the colossus (1.8 m), drawn as glooms (VARIETY_TECH 7).
    if (tier >= 4) return tier == 4 ? 1.1 : 1.8;
    return tier <= 0 ? r.x : (tier == 1 ? r.y : (tier == 2 ? r.z : r.w));
}
