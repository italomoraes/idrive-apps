// SPDX-License-Identifier: GPL-3.0-or-later
#include "route_box.h"

#include <cmath>
#include <cstring>

namespace woc {

const char kClusterRouteBox[] = "IC.RB.Main";

namespace {

/* Longer strings are not the route box's: a length field read in the
 * middle of other data. */
const uint32_t kMaxString = 2048;

uint32_t u32_at(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

double f64_at(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v = v << 8 | p[i];
    double d;
    std::memcpy(&d, &v, sizeof d);
    return d;
}

struct Str {
    size_t start = 0;   /* its length field */
    size_t end = 0;     /* past its NUL */
    std::string text;   /* without the BOM and the NUL */
};

/* Each CommonAPI string in p, in order: a u32 length (4 to kMaxString)
 * over a BOM, the text and a NUL. Where none starts, the next byte is
 * tried. */
std::vector<Str> strings(const uint8_t *p, size_t n)
{
    std::vector<Str> out;
    size_t i = 0;
    while (n >= 8 && i <= n - 8) {
        const uint32_t len = u32_at(p + i);
        if (len >= 4 && len <= kMaxString && len <= n - i - 4 && p[i + 4] == 0xEF && p[i + 5] == 0xBB &&
            p[i + 6] == 0xBF && p[i + 3 + len] == 0) {
            Str s;
            s.start = i;
            s.end = i + 4 + len;
            s.text.assign((const char *)p + i + 7, len - 4);
            out.push_back(s);
            i = s.end;
        } else {
            i++;
        }
    }
    return out;
}

bool is_screen(const std::string &s)
{
    return s.compare(0, 3, "IC.") == 0 || s.compare(0, 4, "CID.") == 0 || s.compare(0, 4, "HUD.") == 0;
}

/* Between a manoeuvre's distance string and its text: unit u32, weight
 * f64, icon u32, icon2 u32, flag u32. */
const size_t kFieldsBytes = 24;

}  // namespace

bool parse_route_box(const uint8_t *p, size_t n, const std::string &screen, std::vector<RouteBoxEntry> *out)
{
    out->clear();
    const std::vector<Str> ss = strings(p, n);
    bool named = false, in_screen = false;
    for (size_t k = 0; k < ss.size(); k++) {
        const Str &s = ss[k];
        if (is_screen(s.text)) {
            in_screen = s.text == screen;
            named = named || in_screen;
            continue;
        }
        /* a manoeuvre's text: its distance string ends kFieldsBytes before it */
        if (!in_screen || s.text.empty() || k == 0 || s.start < kFieldsBytes + 4 ||
            ss[k - 1].end != s.start - kFieldsBytes || is_screen(ss[k - 1].text))
            continue;
        const uint8_t *q = p + s.start;
        RouteBoxEntry e;
        e.unit = u32_at(q - 24);
        e.weight = f64_at(q - 20);
        e.icon = u32_at(q - 12);
        e.icon2 = u32_at(q - 8);
        e.flag = u32_at(q - 4);
        if (e.unit >= 1000 || e.icon >= 100000 || e.icon2 >= 100000 || e.flag >= 1000 || !std::isfinite(e.weight))
            continue;
        e.distance = ss[k - 1].text;
        e.text = s.text;
        out->push_back(e);
    }
    return named;
}

Maneuver::Kind kind_of_icon(uint32_t icon)
{
    switch (icon) {
    case 20: return Maneuver::Arrive;
    case 30: return Maneuver::SharpRight;
    case 31: return Maneuver::Right;
    case 42:
    case 45: return Maneuver::Straight;
    default: return Maneuver::Other;
    }
}

bool icon_known(uint32_t icon)
{
    return kind_of_icon(icon) != Maneuver::Other;
}

std::string route_box_distance(const std::string &distance, uint32_t unit)
{
    if (distance.empty())
        return std::string();
    return unit == 4 ? distance + " m" : distance;
}

bool unit_known(uint32_t unit)
{
    return unit == 0 || unit == 4;
}

Maneuver route_box_maneuver(const std::vector<RouteBoxEntry> &entries)
{
    Maneuver m;
    if (entries.empty())
        return m;
    const RouteBoxEntry &e = entries.front();
    m.kind = kind_of_icon(e.icon);
    m.distance = route_box_distance(e.distance, e.unit);
    m.road = e.text;
    return m;
}

}  // namespace woc
