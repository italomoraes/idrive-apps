// SPDX-License-Identifier: GPL-3.0-or-later
/* Host test of the navigation's route box (route_box.h), on payloads laid
 * out byte for byte as a car sent them (made-up roads): a screen's events
 * in order, empty slots included, and the other screens' left out; a
 * payload without the cluster's screen saying nothing of it, and one with
 * it and empty slots only saying there is no manoeuvre; each turn's
 * direction and the destination's side as the card's arrow, and the plain
 * mark for a kind or direction not known; the distance's text and unit; a
 * text's length never taken for its direction (the mistake of the first
 * reading); the list mid-rebuild keeping the card; cut, garbled and
 * implausible input read without a fault and without inventing events. */
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

    Payload &u8(uint8_t v)
    {
        b.push_back(v);
        return *this;
    }
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
    Payload &raw(const Payload &p)
    {
        b.insert(b.end(), p.b.begin(), p.b.end());
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
    /* a screen: its name, its header and its events */
    Payload &screen(const std::string &name, const std::vector<Payload> &events)
    {
        str(name);
        size_t bytes = 0;
        uint8_t shown = 0;
        for (const Payload &e : events) {
            bytes += e.b.size();
            shown += e.b.size() >= 8 && e.b[7] != kRouteBoxEmpty;
        }
        u32(2).u32(0).u32(0).u8(shown).u32((uint32_t)bytes);
        for (const Payload &e : events)
            raw(e);
        return *this;
    }
};

/* a union: its length, its tag (the event's kind), its bytes */
Payload event(uint32_t kind, const Payload &data)
{
    Payload p;
    p.u32((uint32_t)data.b.size()).u32(kind).raw(data);
    return p;
}

/* the bytes of a turn or a destination up to its direction, as a car sent
 * them: lead is 2 for the first event, 1 for the ones after */
Payload head(const std::string &distance, uint32_t unit, const std::string &text, uint32_t lead)
{
    Payload d;
    d.u32(0).u32(1).f64(600).u8(0).u32(8).u32(1).zeros(8);
    d.str(distance).u32(unit).f64(1.0);
    const uint32_t s = (uint32_t)text.size() + 8;   /* the text string's bytes */
    d.u32(s + 8).u32(s).u32(1).str(text);
    d.zeros(16).u32(lead).u32(4).u32(1).zeros(12);
    return d;
}

Payload turn(const std::string &distance, uint32_t unit, const std::string &text, uint32_t direction,
             uint32_t lead = 2)
{
    Payload d = head(distance, unit, text, lead);
    d.u32(direction).u32(4).u32(1).zeros(8);
    return event(kRouteBoxTurn, d);
}

Payload destination(const std::string &distance, uint32_t unit, const std::string &text, uint32_t side,
                    uint32_t lead = 2)
{
    Payload d = head(distance, unit, text, lead);
    d.u32(side).u8(1).zeros(4);
    for (int i = 0; i < 3; i++)
        d.u32(4).u32(1).u32(0);
    return event(kRouteBoxDestination, d);
}

Payload empty_slot()
{
    Payload d;
    d.u32(0).u32(0).f64(0).u8(0).u32(8).u32(1).zeros(8).u32(0);
    return event(kRouteBoxEmpty, d);
}

bool parse(const Payload &p, const std::string &screen, std::vector<RouteBoxEvent> *e)
{
    return parse_route_box(p.b.data(), p.b.size(), screen, e);
}

}  // namespace

int main()
{
    std::vector<RouteBoxEvent> e;
    Maneuver m;

    ok("an empty slot is the 37 bytes a car sends, a turn's head its 60 after the text",
       empty_slot().b.size() == 8 + 37 && turn("0", 4, "R. Exemplo", 7).b.size() == 8 + 144);
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox, {turn("300", 4, "Rua Exemplo", 5), turn(" ", 0, "Rua Seguinte", 2, 1),
                                           empty_slot()});
        const bool named = parse(p, kClusterRouteBox, &e);
        ok("the cluster's screen: named, its three slots in order", named && e.size() == 3 &&
                                                                       e[0].kind == kRouteBoxTurn &&
                                                                       e[2].kind == kRouteBoxEmpty);
        ok("the next turn: its distance, unit, text and direction",
           e.size() == 3 && e[0].distance == "300" && e[0].unit == 4 && e[0].text == "Rua Exemplo" && e[0].direction == 5);
        ok("the one after it: a blank distance, its own direction",
           e.size() == 3 && e[1].distance == " " && e[1].unit == 0 && e[1].text == "Rua Seguinte" && e[1].direction == 2);
        ok("the card: a right, 300 m, the road",
           route_box_maneuver(e, &m) && m.kind == Maneuver::Right && m.distance == "300 m" && m.road == "Rua Exemplo");
    }
    {
        /* 15 characters: the first reading took the text's length (31) for
         * a right turn */
        Payload p;
        p.u32(0).screen(kClusterRouteBox, {turn("80", 4, "Rua Quinze Ltrs", 2)});
        parse(p, kClusterRouteBox, &e);
        ok("a text's length is not its direction: a 15-letter road on the left is a left",
           route_box_maneuver(e, &m) && m.kind == Maneuver::Left);
    }
    {
        const Maneuver::Kind want[] = {Maneuver::Straight, Maneuver::Left, Maneuver::SlightLeft, Maneuver::SharpLeft,
                                       Maneuver::Right, Maneuver::SlightRight, Maneuver::SharpRight, Maneuver::UTurn};
        bool all = true;
        for (uint32_t d = 1; d <= 8; d++) {
            Payload p;
            p.u32(0).screen(kClusterRouteBox, {turn("100", 4, "Rua Exemplo", d)});
            parse(p, kClusterRouteBox, &e);
            all = all && route_box_maneuver(e, &m) && m.kind == want[d - 1] && event_known(kRouteBoxTurn, d);
        }
        ok("turn directions 1 to 8: straight, left, slight left, sharp left, right, slight right, sharp right, "
           "U-turn",
           all);
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox, {destination("100", 4, "Casa de Teste", 2), empty_slot(), empty_slot()});
        parse(p, kClusterRouteBox, &e);
        ok("the destination ahead: the flag, its distance and name",
           route_box_maneuver(e, &m) && m.kind == Maneuver::Arrive && m.distance == "100 m" && m.road == "Casa de Teste");
        ok("the destination's sides",
           kind_of_event(kRouteBoxDestination, 1) == Maneuver::ArriveLeft &&
               kind_of_event(kRouteBoxDestination, 3) == Maneuver::ArriveRight &&
               event_known(kRouteBoxDestination, 3) && !event_known(kRouteBoxDestination, 9) &&
               kind_of_event(kRouteBoxDestination, 9) == Maneuver::Arrive);
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox, {turn("", 0, "rumo a Rua Exemplo", 1), turn(" ", 0, "Av. Seguinte", 5, 1)});
        parse(p, kClusterRouteBox, &e);
        ok("\"towards\" with no distance: straight on, no distance",
           route_box_maneuver(e, &m) && m.kind == Maneuver::Straight && m.distance.empty());
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox, {event(3, head("200", 4, "Rotat\xC3\xB3ria Exemplo", 2))});
        parse(p, kClusterRouteBox, &e);
        ok("a kind not known yet: the plain mark, its text as sent, accents and all",
           route_box_maneuver(e, &m) && m.kind == Maneuver::Other && m.road == "Rotat\xC3\xB3ria Exemplo" &&
               m.distance == "200 m" && !event_known(3, 0));
        ok("a direction not known yet: the plain mark",
           kind_of_event(kRouteBoxTurn, 9) == Maneuver::Other && !event_known(kRouteBoxTurn, 0));
    }
    {
        Payload p;
        p.u32(0)
            .screen("CID.WidgetRB.Big", {turn("", 0, "Rumo a Rua Exemplo", 1)})
            .screen(kClusterRouteBox, {turn("60", 4, "Rua Diagonal", 7)})
            .screen("HUD.RB.All", {});
        parse(p, kClusterRouteBox, &e);
        ok("three screens: only the cluster's event", e.size() == 1 && e[0].text == "Rua Diagonal" && e[0].direction == 7);
        parse(p, "CID.WidgetRB.Big", &e);
        ok("and the central display's, asked for, only its own", e.size() == 1 && e[0].text == "Rumo a Rua Exemplo");
        const bool hud = parse(p, "HUD.RB.All", &e);
        ok("a screen named with no events: none, no card", hud && e.empty() && route_box_maneuver(e, &m) &&
                                                               m.kind == Maneuver::None);
    }
    {
        Payload p;
        p.u32(0).screen("CID.WidgetRB.Big", {turn("200", 4, "Rua Exemplo", 5)});
        ok("no cluster screen in the payload: it says nothing of it", !parse(p, kClusterRouteBox, &e) && e.empty());
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox, {empty_slot(), empty_slot(), empty_slot()});
        ok("empty slots only: no manoeuvre",
           parse(p, kClusterRouteBox, &e) && e.size() == 3 && route_box_maneuver(e, &m) && m.kind == Maneuver::None);
        Payload q;
        q.u32(0).screen(kClusterRouteBox, {empty_slot(), turn(" ", 0, "Rua Seguinte", 5, 1), empty_slot()});
        parse(q, kClusterRouteBox, &e);
        Maneuver kept;
        kept.kind = Maneuver::Left;
        ok("the first slot empty and a later one not (the list mid-rebuild): the card keeps its own",
           !route_box_maneuver(e, &kept) && kept.kind == Maneuver::Left);
    }
    {
        /* the group after the text missing: no direction read */
        Payload d = head("300", 4, "Rua Exemplo", 2);
        d.b.resize(d.b.size() - 28);
        d.zeros(28);
        Payload p;
        p.u32(0).screen(kClusterRouteBox, {event(kRouteBoxTurn, d.u32(5).u32(4).u32(1).zeros(8))});
        parse(p, kClusterRouteBox, &e);
        ok("a turn without its marks after the text: no direction, the plain mark",
           e.size() == 1 && e[0].direction == 0 && route_box_maneuver(e, &m) && m.kind == Maneuver::Other);
    }
    {
        Payload p;
        p.u32(0).screen(kClusterRouteBox, {turn("300", 4, "Rua Exemplo", 5), turn("90", 4, "Rua Seguinte", 2, 1)});
        /* cut in the middle of the second event */
        Payload cut = p;
        cut.b.resize(p.b.size() - 40);
        parse(cut, kClusterRouteBox, &e);
        ok("cut short: the event before the cut, nothing invented",
           e.size() == 1 && e[0].text == "Rua Exemplo" && e[0].direction == 5);
        bool fine = true;
        for (size_t n = 0; n <= p.b.size(); n++) {
            parse_route_box(p.b.data(), n, kClusterRouteBox, &e);
            fine = fine && e.size() <= 2 && (e.empty() || e[0].text.empty() || e[0].text == "Rua Exemplo");
        }
        ok("every length of it read without a fault", fine);
        /* an event's length past the screen's bytes */
        Payload bad;
        bad.u32(0).str(kClusterRouteBox).u32(2).u32(0).u32(0).u8(1).u32(20).u32(0x7FFFFFF0).u32(2).zeros(12);
        ok("an event longer than its screen: none", parse(bad, kClusterRouteBox, &e) && e.empty());
        std::vector<uint8_t> noise(4096);
        uint32_t x = 12345;
        for (uint8_t &c : noise) {
            x = x * 1103515245u + 12345u;
            c = (uint8_t)(x >> 16);
        }
        ok("noise: nothing", !parse_route_box(noise.data(), noise.size(), kClusterRouteBox, &e) && e.empty());
        ok("nothing at all: nothing", !parse_route_box(nullptr, 0, kClusterRouteBox, &e) && e.empty());
    }

    ok("distance: metres for unit 4, none when blank",
       route_box_distance("300", 4) == "300 m" && route_box_distance(" ", 0).empty() &&
           route_box_distance("", 4).empty() && route_box_distance("1,2", 7) == "1,2");
    ok("units 0 and 4 known, 7 not", unit_known(0) && unit_known(4) && !unit_known(7));
    ok("an empty slot: no arrow, known", kind_of_event(kRouteBoxEmpty, 0) == Maneuver::None &&
                                             event_known(kRouteBoxEmpty, 0));

    std::printf("%s\n", failures ? "FAILED" : "all ok");
    return failures ? 1 : 0;
}
