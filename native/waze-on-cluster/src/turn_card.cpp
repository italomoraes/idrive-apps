// SPDX-License-Identifier: GPL-3.0-or-later
#include "turn_card.h"

#include <cmath>
#include <cstring>

namespace woc {

namespace {

/* The classic 5x7 font, ASCII 32-126: five columns a glyph, bit 0 the top
 * row. */
const uint8_t kFont[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00}, {0x00, 0x1C, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1C, 0x00}, {0x08, 0x2A, 0x1C, 0x2A, 0x08}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}, {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00}, {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06}, {0x32, 0x49, 0x79, 0x41, 0x3E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00}, {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40}, {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
    {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3D, 0x00},
    {0x7F, 0x10, 0x28, 0x44, 0x00}, {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00}, {0x00, 0x00, 0x7F, 0x00, 0x00},
    {0x00, 0x41, 0x36, 0x08, 0x00}, {0x08, 0x04, 0x08, 0x10, 0x08},
};

/* Latin-1 0xC0-0xFF as base letters ('?' for what has none). */
const char kLatin1[] = "AAAAAAACEEEEIIIIDNOOOOO*OUUUUY?s"
                       "aaaaaaaceeeeiiiidnooooo/ouuuuy?y";

struct Canvas {
    std::vector<uint8_t> *px;

    /* colour over what is there, at coverage a (0-1). */
    void blend(int x, int y, uint8_t r, uint8_t g, uint8_t b, float a)
    {
        if (x < 0 || y < 0 || x >= (int)kTurnCardW || y >= (int)kTurnCardH || a <= 0)
            return;
        if (a > 1)
            a = 1;
        uint8_t *p = &(*px)[((size_t)y * kTurnCardW + x) * 4];
        const float da = p[3] / 255.0f, oa = a + da * (1 - a);
        const uint8_t c[3] = {r, g, b};
        for (int i = 0; i < 3; i++)
            p[i] = (uint8_t)std::lround((c[i] * a + p[i] * da * (1 - a)) / oa);
        p[3] = (uint8_t)std::lround(oa * 255);
    }
};

float seg_dist(float px, float py, float ax, float ay, float bx, float by)
{
    const float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    const float len2 = vx * vx + vy * vy;
    float t = len2 > 0 ? (wx * vx + wy * vy) / len2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    const float dx = px - (ax + t * vx), dy = py - (ay + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}

struct Pt {
    float x, y;
};

/* The arrow of a kind in a 48x48 square: a stroked path, the head at its
 * last point along its last segment. */
struct Arrow {
    std::vector<Pt> path;
    bool head = true;
    bool ring = false;   /* a roundabout's circle */
    bool dot = false;    /* the destination */
};

Arrow arrow_of(Maneuver::Kind k)
{
    Arrow a;
    switch (k) {
    case Maneuver::Straight: a.path = {{24, 42}, {24, 10}}; break;
    case Maneuver::SlightLeft: a.path = {{28, 42}, {28, 26}, {15, 12}}; break;
    case Maneuver::Left: a.path = {{30, 42}, {30, 20}, {9, 20}}; break;
    case Maneuver::SharpLeft: a.path = {{28, 42}, {28, 16}, {13, 32}}; break;
    case Maneuver::SlightRight: a.path = {{20, 42}, {20, 26}, {33, 12}}; break;
    case Maneuver::Right: a.path = {{18, 42}, {18, 20}, {39, 20}}; break;
    case Maneuver::SharpRight: a.path = {{20, 42}, {20, 16}, {35, 32}}; break;
    case Maneuver::UTurn: a.path = {{32, 42}, {32, 18}, {26, 12}, {20, 12}, {14, 18}, {14, 36}}; break;
    case Maneuver::Roundabout:
        a.path = {{24, 44}, {24, 34}};
        a.head = false;
        a.ring = true;
        break;
    case Maneuver::Arrive:
        a.path = {{24, 42}, {24, 24}};
        a.head = false;
        a.dot = true;
        break;
    case Maneuver::None: break;
    }
    return a;
}

/* Coverage of an edge at signed distance d inside (positive) it. */
float edge(float d)
{
    return d <= -0.5f ? 0 : d >= 0.5f ? 1 : d + 0.5f;
}

void draw_arrow(Canvas *c, Maneuver::Kind k, int ox, int oy)
{
    const Arrow a = arrow_of(k);
    if (a.path.size() < 2)
        return;
    const float half = 3.0f;
    const Pt tip = a.path.back(), prev = a.path[a.path.size() - 2];
    float ux = tip.x - prev.x, uy = tip.y - prev.y;
    const float ul = std::sqrt(ux * ux + uy * uy);
    ux /= ul;
    uy /= ul;
    /* the head: a triangle whose point is 5 px past the path's end */
    const Pt h0 = {tip.x + ux * 5, tip.y + uy * 5};
    const Pt h1 = {tip.x - ux * 5 - uy * 9, tip.y - uy * 5 + ux * 9};
    const Pt h2 = {tip.x - ux * 5 + uy * 9, tip.y - uy * 5 - ux * 9};
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            float d = 1e9f;
            for (size_t i = 0; i + 1 < a.path.size(); i++) {
                const float s = seg_dist(fx, fy, a.path[i].x, a.path[i].y, a.path[i + 1].x, a.path[i + 1].y);
                d = s < d ? s : d;
            }
            float cov = edge(half - d);
            if (a.head) {
                /* inside the triangle: the least of the distances to its edges */
                const Pt t[3] = {h0, h2, h1};
                float in = 1e9f;
                for (int i = 0; i < 3; i++) {
                    const Pt p = t[i], q = t[(i + 1) % 3];
                    const float ex = q.x - p.x, ey = q.y - p.y, el = std::sqrt(ex * ex + ey * ey);
                    const float s = ((fx - p.x) * ey - (fy - p.y) * ex) / el;
                    in = s < in ? s : in;
                }
                const float hc = edge(in);
                cov = hc > cov ? hc : cov;
            }
            if (a.ring) {
                const float r = std::sqrt((fx - 24) * (fx - 24) + (fy - 22) * (fy - 22));
                const float rc = edge(half - std::fabs(r - 10));
                cov = rc > cov ? rc : cov;
            }
            if (a.dot) {
                const float r = std::sqrt((fx - 24) * (fx - 24) + (fy - 16) * (fy - 16));
                const float rc = edge(9 - r);
                cov = rc > cov ? rc : cov;
            }
            c->blend(ox + x, oy + y, 255, 255, 255, cov);
        }
    }
}

void draw_text(Canvas *c, const std::string &s, int x, int y, int scale, uint8_t r, uint8_t g, uint8_t b)
{
    for (char ch : s) {
        const unsigned char u = (unsigned char)ch;
        const uint8_t *gl = kFont[(u >= 32 && u <= 126 ? u : '?') - 32];
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if (gl[col] >> row & 1)
                    for (int dy = 0; dy < scale; dy++)
                        for (int dx = 0; dx < scale; dx++)
                            c->blend(x + col * scale + dx, y + row * scale + dy, r, g, b, 1);
        x += 6 * scale;
    }
}

}  // namespace

bool maneuver_kind(const std::string &name, Maneuver::Kind *out)
{
    static const struct {
        const char *name;
        Maneuver::Kind kind;
    } kinds[] = {{"none", Maneuver::None},
                 {"straight", Maneuver::Straight},
                 {"slight_left", Maneuver::SlightLeft},
                 {"left", Maneuver::Left},
                 {"sharp_left", Maneuver::SharpLeft},
                 {"slight_right", Maneuver::SlightRight},
                 {"right", Maneuver::Right},
                 {"sharp_right", Maneuver::SharpRight},
                 {"uturn", Maneuver::UTurn},
                 {"roundabout", Maneuver::Roundabout},
                 {"arrive", Maneuver::Arrive}};
    for (const auto &k : kinds)
        if (name == k.name) {
            *out = k.kind;
            return true;
        }
    return false;
}

std::string card_text(const std::string &utf8, size_t max_chars)
{
    std::string out;
    for (size_t i = 0; i < utf8.size();) {
        const unsigned char c = (unsigned char)utf8[i];
        if (c < 0x80) {
            out += c >= 32 && c <= 126 ? (char)c : ' ';
            i++;
            continue;
        }
        /* a UTF-8 sequence: its code point, or '?' for one cut short */
        const int n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        uint32_t cp = n == 1 ? 0xFFFD : c & (0xFF >> (n + 1));
        bool whole = n > 1 && i + n <= utf8.size();
        for (int k = 1; whole && k < n; k++) {
            const unsigned char t = (unsigned char)utf8[i + k];
            if ((t & 0xC0) != 0x80)
                whole = false;
            cp = cp << 6 | (t & 0x3F);
        }
        if (!whole) {
            out += '?';
            i++;
            continue;
        }
        out += cp >= 0xC0 && cp <= 0xFF ? kLatin1[cp - 0xC0] : cp == 0xA0 ? ' ' : cp == 0xBA || cp == 0xB0 ? 'o' : cp == 0xAA ? 'a' : '?';
        i += n;
    }
    if (out.size() > max_chars && max_chars >= 3)
        out = out.substr(0, max_chars - 3) + "...";
    else if (out.size() > max_chars)
        out.resize(max_chars);
    return out;
}

void render_turn_card(const Maneuver &m, std::vector<uint8_t> *rgba)
{
    rgba->assign((size_t)kTurnCardW * kTurnCardH * 4, 0);
    if (m.kind == Maneuver::None)
        return;
    Canvas c{rgba};
    /* the plate: dark, a little see-through, its corners rounded */
    const float rad = 12;
    for (int y = 0; y < (int)kTurnCardH; y++)
        for (int x = 0; x < (int)kTurnCardW; x++) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            const float cx = fx < rad ? rad : fx > kTurnCardW - rad ? kTurnCardW - rad : fx;
            const float cy = fy < rad ? rad : fy > kTurnCardH - rad ? kTurnCardH - rad : fy;
            const float d = std::sqrt((fx - cx) * (fx - cx) + (fy - cy) * (fy - cy));
            c.blend(x, y, 16, 20, 28, edge(rad - d) * 0.88f);
        }
    draw_arrow(&c, m.kind, 6, 4);
    const int tx = 62, room = (int)kTurnCardW - tx - 10;
    const std::string road = card_text(m.road, (size_t)(room / 12));
    const std::string dist = card_text(m.distance, (size_t)(room / 18));
    if (road.empty()) {
        draw_text(&c, dist, tx, 18, 3, 255, 255, 255);
    } else {
        draw_text(&c, dist, tx, 7, 3, 255, 255, 255);
        draw_text(&c, road, tx, 35, 2, 200, 208, 220);
    }
}

}  // namespace woc
