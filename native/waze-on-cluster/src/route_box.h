// SPDX-License-Identifier: GPL-3.0-or-later
/* The BMW navigation's route box: the next manoeuvres it shows on each
 * screen, as GuidanceEventList (SOME/IP 0xFB1B, major 5, instance 0x0080)
 * sends them in its event 0x8002, and the turn card's manoeuvre made of the
 * cluster's (turn_card.h). guidance_link.h receives the event.
 *
 * The payload is CommonAPI's big-endian serialisation, its strings a u32
 * length, a UTF-8 BOM, the text and a NUL. A screen's name ("IC.RB.Main" for
 * the cluster's box, "CID.WidgetRB.Big" for the central display's) comes
 * before its manoeuvres, the next one first, each ending in:
 *
 *   string distance  "300"; "" when none is shown
 *   u32    unit      4 with a distance (metres); 0 without
 *   f64    1.0 for the next manoeuvre, 500.0 for the one after (not used)
 *   u32    icon      the arrow (kind_of_icon)
 *   u32    icon2     icon - 8 so far (not used)
 *   u32    flag      1 so far (not used)
 *   string text      the road it leads to, or an instruction ("towards ...")
 *
 * This is how a car (iDrive 7, Google Maps over CarPlay, a drive in 2026-11)
 * sent it, not an interface definition: a manoeuvre laid out otherwise is
 * not found, and a screen whose name is in the payload with none found has
 * none to show. The icons: kind_of_icon. */
#ifndef WOC_ROUTE_BOX_H
#define WOC_ROUTE_BOX_H

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "turn_card.h"

namespace woc {

/* The cluster's route box. */
extern const char kClusterRouteBox[];   /* "IC.RB.Main" */

struct RouteBoxEntry {
    std::string distance;
    uint32_t unit = 0;
    double weight = 0;
    uint32_t icon = 0;
    uint32_t icon2 = 0;
    uint32_t flag = 0;
    std::string text;
};

/* The manoeuvres of the screen named screen in an event 0x8002 payload,
 * the next first. false: the payload does not name that screen (it may name
 * others), so it says nothing of it; true with none: that screen shows no
 * manoeuvre. */
bool parse_route_box(const uint8_t *p, size_t n, const std::string &screen, std::vector<RouteBoxEntry> *out);

/* The card's arrow for a route box icon, Maneuver::Other for one not known
 * yet. Known, from drives with the turn's direction noted:
 *   20 the destination; 30 a sharp right; 31 a right;
 *   42 "start on <road>" and 45 "towards <road>", drawn straight. */
Maneuver::Kind kind_of_icon(uint32_t icon);
/* Whether kind_of_icon knows icon. */
bool icon_known(uint32_t icon);

/* distance as the card shows it: "300 m" for unit 4, distance alone for a
 * unit not known yet, "" without one. */
std::string route_box_distance(const std::string &distance, uint32_t unit);
/* Whether route_box_distance knows unit (0 or 4). */
bool unit_known(uint32_t unit);

/* The turn card's manoeuvre for a screen's entries: the first one, or none. */
Maneuver route_box_maneuver(const std::vector<RouteBoxEntry> &entries);

}  // namespace woc

#endif
