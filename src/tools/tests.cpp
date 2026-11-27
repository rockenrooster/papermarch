// papermarch_tests (TECH_SPEC 9.4): the battle's rules, the bot on every level of the table, the text form, the palette
// lint, the save and the PNG writer. Exit 0 = all passed. Argument: an output folder.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/png.h"
#include "game/layout.h"
#include "game/save.h"
#include "gfx/palette.h"
#include "sim/battle.h"
#include "sim/bot.h"

using namespace pm;

namespace {

int g_failures = 0, g_checks = 0;
void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL %s\n", what);
    }
}
#define CHECK(cond) check((cond), #cond)

sim::Battle g_b;

// Runs the battle with the squad held at `x` until `seconds` pass or it ends.
void run(sim::Battle& b, float x, float seconds, sim::BattleEvents* sum = nullptr) {
    sim::BattleEvents ev;
    for (int t = 0; t < int(seconds * 60.0f) && !b.won && !b.lost; ++t) {
        b.tick(x, true, ev);
        if (sum) {
            sum->kills += ev.kills;
            sum->lost += ev.lost;
            sum->shots += ev.shots;
            if (ev.gate >= 0) sum->gate = ev.gate, sum->gateAfter = ev.gateAfter;
            if (ev.barrel >= 0) sum->barrel = ev.barrel, sum->barrelReward = ev.barrelReward;
        }
    }
}

void testBattle() {
    // Gates: the squad passes the one it stands in; an add gate adds, a multiplier multiplies.
    sim::LevelDef d;
    d.start = 10;
    d.length = 60;
    d.gates[d.gateCount++] = {-4.6f, -0.2f, 10.0f, 5, sim::kGateAdd, 0, 5};
    d.gates[d.gateCount++] = {0.2f, 4.6f, 10.0f, 2, sim::kGateMul, 0, 2};
    g_b.init(d);
    CHECK(g_b.squad == 10 && !g_b.started);
    sim::BattleEvents ev;
    g_b.tick(2.5f, false, ev);
    CHECK(!g_b.started && g_b.z == 0.0f);  // nothing moves before the player starts
    run(g_b, 2.5f, 4.0f);
    CHECK(g_b.squad == 20 && g_b.gates[1].used && g_b.gates[0].passed);
    g_b.init(d);
    run(g_b, -2.5f, 4.0f);
    CHECK(g_b.squad == 15 && g_b.gates[0].used);
    // A barricade the squad shoots grows toward its cap before it is passed.
    sim::LevelDef r;
    r.start = 20;
    r.length = 60;
    r.gates[r.gateCount++] = {-4.9f, 4.9f, 25.0f, -30, sim::kGateAdd, 1, 5};
    g_b.init(r);
    run(g_b, 0.0f, 8.0f);
    CHECK(g_b.gates[0].used && g_b.squad >= 20);  // shot from -30 up past zero before the squad reached it
    // A barrel shot open frees its recruits; one run into costs soldiers.
    sim::LevelDef br;
    br.start = 10;
    br.length = 80;
    br.barrels[br.barrelCount++] = {0.0f, 30.0f, 20, 15};
    g_b.init(br);
    sim::BattleEvents sum;
    run(g_b, 0.0f, 8.0f, &sum);
    CHECK(g_b.barrels[0].broken && sum.barrel == 0 && g_b.squad == 25);
    br.barrels[0].hp = 100000;
    g_b.init(br);
    run(g_b, 0.0f, 8.0f);
    CHECK(g_b.barrels[0].broken && g_b.squad < 10);
    // A horde too big for the squad's fire reaches it and wins.
    sim::LevelDef h;
    h.start = 5;
    h.length = 200;
    h.waves[h.waveCount++] = {30.0f, -4.0f, 4.0f, 300, sim::kFoot};
    g_b.init(h);
    run(g_b, 0.0f, 30.0f);
    CHECK(g_b.lost && g_b.squad == 0);
    // A small one is shot down before it arrives.
    h.waves[0].count = 6;
    h.start = 40;
    g_b.init(h);
    sum = sim::BattleEvents();
    run(g_b, 0.0f, 12.0f, &sum);
    CHECK(sum.kills >= 5 && g_b.squad >= 39);
    // The boss: the squad halts before it; it falls to enough fire, and the level is won.
    sim::LevelDef bo;
    bo.start = 60;
    bo.length = 80;
    bo.bossHp = 400;
    g_b.init(bo);
    run(g_b, 0.0f, 60.0f);
    CHECK(g_b.won && g_b.squad > 0);
    bo.start = 3;
    bo.bossHp = 100000;
    g_b.init(bo);
    run(g_b, 0.0f, 120.0f);
    CHECK(g_b.lost);
    // The pause holds: the bot's run of level 20 (hordes run at it) stands now and then, never for less than kHoldBack.
    g_b.init(sim::level(20));
    sim::BattleEvents ev2;
    int stops = 0;
    float standing = 0, shortest = 99;
    for (int t = 0; t < 60 * 300 && !g_b.won && !g_b.lost; ++t) {
        g_b.tick(sim::botSteer(g_b), true, ev2);
        if (g_b.halted) {
            stops += standing == 0.0f;
            standing += sim::kDt;
        } else if (standing > 0.0f) {
            shortest = minv(shortest, standing);
            standing = 0;
        }
    }
    std::printf("level 20: %d stands, the shortest %.2f s\n", stops, double(shortest));
    CHECK(stops > 0 && shortest >= sim::Battle::kHoldBack - 0.02f);
}

void testEncode() {
    const sim::LevelDef& d = sim::level(1);
    static char a[16384], b[16384];
    CHECK(sim::encodeLevel(d, a, sizeof a) > 0);
    sim::LevelDef e;
    CHECK(sim::decodeLevel(a, e));
    sim::encodeLevel(e, b, sizeof b);
    CHECK(!std::strcmp(a, b));
}

void testTable() {
    const int count = sim::levelCount();
    int lost = 0, differs = 0;
    for (int n = 1; n <= count; ++n) {
        const sim::LevelDef& d = sim::level(n);
        i32 left = 0;
        if (!sim::botPlay(d, &left)) {
            ++lost;
            if (lost < 5) std::printf("level %d: the bot loses\n", n);
        } else if (d.botEnd && left != d.botEnd) {
            ++differs;
            if (differs < 5) std::printf("level %d: the bot ends with %d, the table says %d\n", n, left, d.botEnd);
        }
    }
    char what[96];
    std::snprintf(what, sizeof what, "the bot wins every level of the table (%d levels, %d lost, %d differ)", count, lost, differs);
    check(lost == 0 && differs == 0, what);
}

void testPalettes() {
    for (int w = 1; w <= kWorldCount; ++w) {
        const WorldPalette& p = worldPalette(w);
        char what[96];
        std::snprintf(what, sizeof what, "%s: ivory on the terrace %.2f >= 1.25", p.name, double(contrast(col::kIvory, p.terrace1)));
        check(contrast(col::kIvory, p.terrace1) >= 1.25f, what);
    }
}

void testSave(const char* dir) {
    char path[512];
    std::snprintf(path, sizeof path, "%s/test_save.txt", dir);
    SaveData s;
    s.level = 37;
    s.stars[0] = 3;
    s.stars[36] = 2;
    s.fast = true;
    s.finished = true;
    s.hints = 0x55;
    CHECK(writeSave(path, s));
    SaveData t;
    CHECK(loadSave(path, t));
    CHECK(t.level == 37 && t.stars[0] == 3 && t.stars[36] == 2 && t.fast && t.finished && t.hints == 0x55 && t.totalStars() == 5);
}

void testFormat() {
    char buf[32];
    formatThousands(1234567, buf, sizeof buf);
    CHECK(!std::strcmp(buf, "1,234,567"));
    formatHost(1240000, buf, sizeof buf);
    CHECK(!std::strcmp(buf, "1.24M"));
}

}  // namespace

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    {
        u8 px[3 * 2 * 4] = {};
        px[(1 * 3 + 2) * 4] = 200;
        for (int i = 0; i < 6; ++i) px[i * 4 + 3] = 255;
        char path[512];
        std::snprintf(path, sizeof path, "%s/test_png.png", dir);
        CHECK(writePng(path, px, 3, 2, false));
    }
    testBattle();
    testEncode();
    testTable();
    testPalettes();
    testSave(dir);
    testFormat();
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
