// SPDX-License-Identifier: GPL-3.0-or-later
/* The BMW navigation's route box: the next manoeuvres it shows on each
 * screen, as GuidanceEventList (SOME/IP 0xFB1B, major 5, instance 0x0080)
 * sends them in its event 0x8002, and the turn card's manoeuvre made of the
 * cluster's (turn_card.h). guidance_link.h receives the event.
 *
 * The payload is CommonAPI's big-endian serialisation: strings a u32 length
 * over a UTF-8 BOM, the text and a NUL; a union a u32 length, a u32 tag and
 * that many bytes. Each screen ("IC.RB.Main" the cluster's box,
 * "CID.WidgetRB.Big" the central display's) is its name, then
 *
 *   u32 (1 or 2), u32 0, u32 0, u8 (the events shown), u32 the events' bytes
 *
 * and its events, the next manoeuvre first, each a union:
 *
 *   u32 length, u32 kind  2 a turn, 6 the destination, 7 an empty slot;
 *                         others (roundabouts, forks, ferries...) not seen yet
 *   ...                   (33 bytes)
 *   string distance       "300"; " " or "" when none is shown
 *   u32    unit           4 metres; 0 without a distance
 *   f64    (1.0)
 *   u32, u32, u32         the text's union: lengths and tag 1
 *   string text           the road it leads to, or an instruction ("towards ...")
 *   ...                   (40 bytes)
 *   u32    direction      a turn: 1 straight on, 2 left, 3 slight left,
 *                         4 sharp left, 5 right, 6 slight right, 7 sharp
 *                         right, 8 U-turn; the destination: 1 on the left,
 *                         2 ahead, 3 on the right
 *   u32 4, u32 1, ...
 *
 * The turn's directions are BMW's GuidingTypes ones that carplay converts
 * CarPlay's manoeuvres to (DOCUMENTACAO), and a drive's turns had the
 * directions noted on the road. This is how a car sent it (iDrive 7, Google
 * Maps over CarPlay, 2026-11), not an interface definition: a payload laid
 * out otherwise yields no manoeuvre. */
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

/* Event kinds (the union's tags) and turn directions, as the navigation
 * sends them. */
const uint32_t kRouteBoxTurn = 2, kRouteBoxDestination = 6, kRouteBoxEmpty = 7;

/* One event of a screen's route box. */
struct RouteBoxEvent {
    uint32_t kind = 0;        /* kRouteBoxTurn, kRouteBoxDestination, kRouteBoxEmpty or one not known */
    std::string distance;     /* "300"; blank when none is shown */
    uint32_t unit = 0;        /* 4 metres */
    std::string text;         /* the road, or an instruction */
    uint32_t direction = 0;   /* 0: not found */
};

/* The events of the screen named screen in an event 0x8002 payload, in
 * order, empty slots included. false: the payload does not name that screen
 * (it may name others), so it says nothing of it; true: what that screen
 * shows (no events, or empty slots only: no manoeuvre). */
bool parse_route_box(const uint8_t *p, size_t n, const std::string &screen, std::vector<RouteBoxEvent> *out);

/* The card's arrow for an event; Maneuver::Other for a kind or direction
 * not known yet, Maneuver::None for an empty slot. */
Maneuver::Kind kind_of_event(uint32_t kind, uint32_t direction);
/* Whether kind_of_event knows that event (an empty slot counts as known). */
bool event_known(uint32_t kind, uint32_t direction);

/* distance as the card shows it: "300 m" for unit 4, the number alone for a
 * unit not known yet, "" when it is blank. */
std::string route_box_distance(const std::string &distance, uint32_t unit);
/* Whether route_box_distance knows unit (0 or 4). */
bool unit_known(uint32_t unit);

/* The turn card's manoeuvre for a screen's events: the first one's; none
 * when every slot is empty. false: the first slot is empty and a later one
 * is not, as the navigation sends while it rebuilds the list: the card keeps
 * what it shows. */
bool route_box_maneuver(const std::vector<RouteBoxEvent> &events, Maneuver *out);

}  // namespace woc

#endif
