// SPDX-License-Identifier: GPL-3.0-or-later
/* The turn card: the next manoeuvre (an arrow, the distance to it and the
 * road it leads to) as a small RGBA picture drawn over the cluster map
 * (MapFeed::set_card), below the phone's own UI or over the top of it, as
 * turn_card_setting.h says.
 *
 * The card is kTurnCardW x kTurnCardH, straight alpha: a dark rounded plate,
 * the arrow in its left square, the distance on the first line and the road
 * on the second. The text is a built-in 5x7 font, scaled; letters with
 * accents are drawn as their base letter and anything else outside ASCII as
 * '?'. A line that does not fit ends in "...". */
#ifndef WOC_TURN_CARD_H
#define WOC_TURN_CARD_H

#include <stdint.h>
#include <string>
#include <vector>

namespace woc {

const unsigned kTurnCardW = 360, kTurnCardH = 56;
/* Where the card goes in the 1088x480 map, by its top-left corner: centred
 * on the part the cluster shows in full (x 306-782). Bottom is just below
 * the phone's position arrow and above where the cluster's frame ends the
 * map: on photos of a car's cluster (iDrive 7, Google Maps, 2026-11) the
 * arrow's circle spans about y 314-388 and the map shows down to about
 * y 450 in the middle (at 344 the card covered the arrow). Top is over the
 * phone's own turn card, at the top of the safe area (504x368 at 286,72). */
const int kTurnCardX = 364, kTurnCardBottomY = 392, kTurnCardTopY = 72;

struct Maneuver {
    enum Kind {
        None,   /* no manoeuvre: no card */
        Straight,
        SlightLeft,
        Left,
        SharpLeft,
        SlightRight,
        Right,
        SharpRight,
        UTurn,
        Roundabout,
        Arrive,
        Other,  /* a manoeuvre whose arrow is not known (route_box.h): a plain mark */
    };
    Kind kind = None;
    std::string distance;   /* as shown: "200 m" */
    std::string road;       /* the road after the manoeuvre, or its description */

    bool operator==(const Maneuver &o) const { return kind == o.kind && distance == o.distance && road == o.road; }
    bool operator!=(const Maneuver &o) const { return !(*this == o); }
};

/* The kind by its name on the control socket ("straight", "slight_left",
 * "left", "sharp_left", "slight_right", "right", "sharp_right", "uturn",
 * "roundabout", "arrive", "other", "none"); false for another name. */
bool maneuver_kind(const std::string &name, Maneuver::Kind *out);

/* The card for m as kTurnCardW * kTurnCardH * 4 bytes of RGBA. */
void render_turn_card(const Maneuver &m, std::vector<uint8_t> *rgba);

/* text as the card draws it: ASCII, accents folded, at most max_chars with
 * "..." in place of what is cut. */
std::string card_text(const std::string &utf8, size_t max_chars);

}  // namespace woc

#endif
