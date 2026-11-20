// SPDX-License-Identifier: GPL-3.0-or-later
/* Host test of the navigation's route box (route_box.h), on payloads built
 * the way a car sent them (made-up roads): the cluster's manoeuvres found,
 * the next first, and the other screens' left out; a payload without the
 * cluster's screen saying nothing of it, and one with it and no manoeuvre
 * saying there is none; the distance's text and unit; the arrows known and
 * the plain mark for the others; a distance string never taken for a
 * manoeuvre's text; cut, garbled and implausible input read without a
 * fault and without inventing manoeuvres. */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "route_box.h"

using namespace woc;

static int failures;

static void ok(const char *name, bool cond)
{
    std::printf("%-4s %s\n", cond ? "ok" : "FAIL", name);
    if (!cond) failures++;
}

namespace {

/* A payload in CommonAPI's big-endian serialisation. */
struct Payload {
    std::vector<uint8_t> b;

    Payload &u32(uint32_t v)
    {
        for (int s = 24; s >= 0; s -= 8)
            b.push_back((uint8_t)(v >> s));
        return *this;
    }
    Payload &f64(double d)
    {
        uint64_t v;
        std::memcpy(&v, &d, sizeof v);
        for (int s = 56; s >= 0; s -= 8)
            b.push_back((uint8_t)(v >> s));
        return *this;
    }
    Payload &zeros(size_t n)
    {
        b.insert(b.end(), n, 0);
        return *this;
    }
    /* a u32 length over the BOM, the text and a NUL */
    Payload &str(const std::string &s)
    {
        u32((uint32_t)(s.size() + 4));
        b.push_back(0xEF);
        b.push_back(0xBB);
        b.push_back(0xBF);
        b.insert(b.end(), s.begin(), s.end());
        b.push_back(0);
        return *this;
    }
    /* a screen's name and the fields a car sent before its manoeuvres */
    Payload &screen(const std::string &name)
    {
        str(name);
        u32(2).zeros(9).u32(0x103).u32(0xa1).u32(2).zeros(8).f64(0).zeros(1);
        return *this;
    }
    /* a manoeuvre: the fields before it, then distance, unit, weight,
     * icon, icon2, flag, text, then the fields after it */
    Payload &maneuver(const std::string &distance, uint32_t unit, double weight, uint32_t icon,
                      const std::string &text, uint32_t flag = 1)
    {
        u32(8).u32(1).u32(0).u32(0);
        str(distance);
        u32(unit).f64(weight).u32(icon).u32(icon >= 8 ? icon - 8 : 0).u32(flag);
        str(text);
        zeros(16).u32(2).u32(4).u32(1).zeros(12).u32(1).u32(4).u32(1).zeros(12).u32(0x25).u32(7);
        return *this;
    }
};

bool parse(const Payload &p, const std::string &screen, std::vector<RouteBoxEntry> *e)
{
    return parse_route_box(p.b.data(), p.b.size(), screen, e);
}

}  // namespace

int main()
{
    std::vector<RouteBoxEntry> e;

    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox).maneuver("300", 4, 1.0, 31, "Rua Exemplo").maneuver("", 0, 500.0, 20,
                                                                                               "Casa de Teste");
        const bool named = parse(p, kClusterRouteBox, &e);
        ok("the cluster's screen: named, two manoeuvres", named && e.size() == 2);
        ok("the next one first: its distance, unit, weight, icons, flag and text",
           e.size() == 2 && e[0].distance == "300" && e[0].unit == 4 && e[0].weight == 1.0 && e[0].icon == 31 &&
               e[0].icon2 == 23 && e[0].flag == 1 && e[0].text == "Rua Exemplo");
        ok("the one after it, with no distance",
           e.size() == 2 && e[1].distance.empty() && e[1].unit == 0 && e[1].weight == 500.0 && e[1].icon == 20 &&
               e[1].text == "Casa de Teste");
        const Maneuver m = route_box_maneuver(e);
        ok("the card: a right, 300 m, the road", m.kind == Maneuver::Right && m.distance == "300 m" &&
                                                    m.road == "Rua Exemplo");
    }
    {
        Payload p;
        p.u32(0)
            .screen("CID.WidgetRB.Big")
            .maneuver("", 0, 1.0, 45, "Rumo a Rua Exemplo")
            .screen(kClusterRouteBox)
            .maneuver("60", 4, 1.0, 30, "Rua Diagonal")
            .screen("HUD.RB.All");
        parse(p, kClusterRouteBox, &e);
        ok("two screens: only the cluster's manoeuvre", e.size() == 1 && e[0].text == "Rua Diagonal");
        parse(p, "CID.WidgetRB.Big", &e);
        ok("and the central display's, asked for, only its own", e.size() == 1 && e[0].text == "Rumo a Rua Exemplo");
        const bool hud = parse(p, "HUD.RB.All", &e);
        ok("a screen named with no manoeuvre: none", hud && e.empty());
    }
    {
        Payload p;
        p.u32(0).screen("CID.WidgetRB.Big").maneuver("200", 4, 1.0, 31, "Rua Exemplo");
        ok("no cluster screen in the payload: it says nothing of it", !parse(p, kClusterRouteBox, &e) && e.empty());
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox).screen("CID.WidgetRB.Big");
        const bool named = parse(p, kClusterRouteBox, &e);
        ok("the cluster's screen with no manoeuvre: named, none, no card",
           named && e.empty() && route_box_maneuver(e).kind == Maneuver::None);
    }
    {
        /* the distance string right after the screen's name, 24 bytes on:
         * still the distance, not a manoeuvre's text */
        Payload p;
        p.u32(0).str(kClusterRouteBox).zeros(24).str("300").u32(4).f64(1.0).u32(31).u32(23).u32(1).str("Rua Exemplo");
        parse(p, kClusterRouteBox, &e);
        ok("a distance string never read as a manoeuvre's text",
           e.size() == 1 && e[0].distance == "300" && e[0].text == "Rua Exemplo");
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox).maneuver("1,2", 7, 1.0, 99, "Avenida S\xC3\xA3o Jo\xC3\xA3o");
        parse(p, kClusterRouteBox, &e);
        const Maneuver m = route_box_maneuver(e);
        ok("an icon not known yet: the plain mark", m.kind == Maneuver::Other);
        ok("a unit not known yet: the number alone", m.distance == "1,2");
        ok("the text as sent, accents and all (the card folds them)", m.road == "Avenida S\xC3\xA3o Jo\xC3\xA3o");
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox).maneuver("300", 4, 1.0, 0xFFFFFFFF, "Rua Exemplo");
        parse(p, kClusterRouteBox, &e);
        ok("an implausible icon: no manoeuvre", e.empty());
        Payload q;
        q.u32(0).screen(kClusterRouteBox).maneuver("300", 4, 1.0, 31, "Rua Exemplo", 0x7FFFFFFF);
        parse(q, kClusterRouteBox, &e);
        ok("an implausible flag: no manoeuvre", e.empty());
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox).maneuver("300", 4, 1.0, 31, "Rua Exemplo").maneuver("90", 4, 1.0, 31,
                                                                                              "Rua Seguinte");
        /* cut in the middle of the second manoeuvre's text: the 72 bytes
         * after it and 10 of its own */
        Payload cut = p;
        cut.b.resize(p.b.size() - 82);
        parse(cut, kClusterRouteBox, &e);
        ok("cut short: the manoeuvre before the cut, nothing invented", e.size() == 1 && e[0].text == "Rua Exemplo");
        bool fine = true;
        for (size_t n = 0; n <= p.b.size(); n++) {
            parse_route_box(p.b.data(), n, kClusterRouteBox, &e);
            fine = fine && e.size() <= 2;
        }
        ok("every length of it read without a fault", fine);
        /* a length field pointing past the end */
        Payload big;
        big.u32(0).u32(0x7FF).b.push_back(0xEF);
        ok("a string longer than the payload: ignored", !parse(big, kClusterRouteBox, &e) && e.empty());
        std::vector<uint8_t> noise(4096);
        uint32_t x = 12345;
        for (uint8_t &c : noise) {
            x = x * 1103515245u + 12345u;
            c = (uint8_t)(x >> 16);
        }
        ok("noise: nothing", !parse_route_box(noise.data(), noise.size(), kClusterRouteBox, &e) && e.empty());
        ok("nothing at all: nothing", !parse_route_box(nullptr, 0, kClusterRouteBox, &e) && e.empty());
    }

    ok("icon 20: the destination", kind_of_icon(20) == Maneuver::Arrive);
    ok("icon 30: a sharp right", kind_of_icon(30) == Maneuver::SharpRight);
    ok("icon 31: a right", kind_of_icon(31) == Maneuver::Right);
    ok("icons 42 and 45 (start on, towards): straight",
       kind_of_icon(42) == Maneuver::Straight && kind_of_icon(45) == Maneuver::Straight);
    ok("icon 38 not known yet", kind_of_icon(38) == Maneuver::Other && !icon_known(38) && icon_known(31));
    ok("distance: metres for unit 4, none without a number",
       route_box_distance("300", 4) == "300 m" && route_box_distance("", 4).empty() &&
           route_box_distance("", 0).empty());
    ok("units 0 and 4 known, 7 not", unit_known(0) && unit_known(4) && !unit_known(7));

    std::printf("%s\n", failures ? "FAILED" : "all ok");
    return failures ? 1 : 0;
}
