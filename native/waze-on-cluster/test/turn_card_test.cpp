// SPDX-License-Identifier: GPL-3.0-or-later
/* Host test of the turn card (turn_card.h) and its setting
 * (turn_card_setting.h): the kinds by name; the text folded to ASCII and
 * cut; a card with a plate, an arrow and text, a plain mark for an arrow
 * not known, and none for no manoeuvre;
 * a change of the distance alone touching only the text's rows; the
 * setting off by default, the app's choice kept across a restart, other
 * values refused or read as off. */
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "turn_card.h"
#include "turn_card_setting.h"

using namespace woc;

static int failures;

static void ok(const char *name, bool cond)
{
    std::printf("%-4s %s\n", cond ? "ok" : "FAIL", name);
    if (!cond) failures++;
}

static const uint8_t *at(const std::vector<uint8_t> &c, unsigned x, unsigned y)
{
    return &c[((size_t)y * kTurnCardW + x) * 4];
}

/* white pixels in a rectangle */
static unsigned white(const std::vector<uint8_t> &c, unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
    unsigned n = 0;
    for (unsigned y = y0; y < y1; y++)
        for (unsigned x = x0; x < x1; x++)
            n += at(c, x, y)[0] > 240 && at(c, x, y)[3] > 240;
    return n;
}

int main()
{
    Maneuver::Kind k = Maneuver::None;
    ok("kinds by name", maneuver_kind("left", &k) && k == Maneuver::Left && maneuver_kind("uturn", &k) &&
                            k == Maneuver::UTurn && maneuver_kind("none", &k) && k == Maneuver::None &&
                            maneuver_kind("other", &k) && k == Maneuver::Other);
    ok("an unknown kind: refused", !maneuver_kind("sideways", &k) && !maneuver_kind("", &k));

    ok("text: accents folded", card_text("Rua Jo\xC3\xA3o das Flores, \xC3\x81rvore, S\xC3\xA3o Jos\xC3\xA9 \xC3\xA7", 99) ==
                                   "Rua Joao das Flores, Arvore, Sao Jose c");
    ok("text: other characters as ?", card_text("\xE2\x82\xAC 5 \xF0\x9F\x9A\x97", 99) == "? 5 ?");
    ok("text: a sequence cut short as ?", card_text("ab\xC3", 99) == "ab?");
    ok("text: cut with ...", card_text("Avenida Longa Ficticia", 10) == "Avenida...");
    ok("text: one that fits is whole", card_text("Av. Brasil", 10) == "Av. Brasil");

    std::vector<uint8_t> none, left, right, far;
    Maneuver m;
    render_turn_card(m, &none);
    bool empty = none.size() == (size_t)kTurnCardW * kTurnCardH * 4;
    for (uint8_t b : none)
        empty = empty && b == 0;
    ok("no manoeuvre: a clear card", empty);

    m.kind = Maneuver::Left;
    m.distance = "200 m";
    m.road = "Av. Exemplo Comprida";
    render_turn_card(m, &left);
    ok("the plate: dark and nearly opaque in the middle, clear at the corners",
       at(left, 200, 30)[3] > 200 && at(left, 200, 30)[0] < 40 && at(left, 0, 0)[3] == 0 &&
           at(left, kTurnCardW - 1, kTurnCardH - 1)[3] == 0);
    ok("the arrow: white in the left square", white(left, 6, 4, 54, 52) > 150);
    ok("the distance and the road: text right of the arrow", white(left, 62, 7, 200, 28) > 100);
    m.kind = Maneuver::Right;
    render_turn_card(m, &right);
    ok("left and right: mirrored arrows",
       white(left, 6, 4, 18, 52) > 20 && white(left, 42, 4, 54, 52) == 0 && white(right, 42, 4, 54, 52) > 20 &&
           white(right, 6, 4, 18, 52) == 0);
    m.distance = "150 m";
    render_turn_card(m, &far);
    bool rows = true, changed = false;
    for (unsigned y = 0; y < kTurnCardH; y++)
        for (unsigned x = 0; x < kTurnCardW; x++)
            if (std::memcmp(at(right, x, y), at(far, x, y), 4)) {
                changed = true;
                rows = rows && y >= 7 && y < 28 && x >= 62;
            }
    ok("another distance: only the distance's rows change", changed && rows);
    std::vector<uint8_t> other;
    m.kind = Maneuver::Other;
    render_turn_card(m, &other);
    ok("an arrow not known: a dot in the middle of the square, no stroke",
       white(other, 22, 20, 38, 36) > 60 && white(other, 6, 42, 54, 52) == 0 && white(other, 6, 4, 54, 14) == 0 &&
           white(other, 62, 7, 200, 28) > 100);

    const std::string path = "/tmp/woc-turn-card-test-" + std::to_string(getpid()) + ".conf";
    unlink(path.c_str());
    ok("default: off", TurnCardSetting("off", path).mode() == TurnCardSetting::Off);
    ok("the configuration's bottom and top",
       TurnCardSetting("bottom", path).mode() == TurnCardSetting::Bottom &&
           TurnCardSetting("top", path).mode() == TurnCardSetting::Top);
    ok("the configuration's other values: off", TurnCardSetting("1", path).mode() == TurnCardSetting::Off);
    {
        TurnCardSetting s("off", path);
        ok("the app's bottom", s.set("bottom") && s.mode() == TurnCardSetting::Bottom &&
                                   std::string(s.name()) == "bottom");
        ok("another choice: refused, nothing changed", !s.set("middle") && !s.set("") &&
                                                           s.mode() == TurnCardSetting::Bottom);
    }
    ok("after a restart: still bottom, over the configuration's off",
       TurnCardSetting("off", path).mode() == TurnCardSetting::Bottom);
    {
        TurnCardSetting s("bottom", path);
        ok("the app's off", s.set("off") && s.mode() == TurnCardSetting::Off);
    }
    ok("after a restart: off, over the configuration's top", TurnCardSetting("top", path).mode() == TurnCardSetting::Off);
    std::ofstream(path) << "turn_card = sideways\n";
    ok("a saved value that is none of them: off", TurnCardSetting("top", path).mode() == TurnCardSetting::Off);
    std::ofstream(path) << "garbage\n";
    ok("a state file without the setting: off", TurnCardSetting("top", path).mode() == TurnCardSetting::Off);
    unlink(path.c_str());

    std::printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
