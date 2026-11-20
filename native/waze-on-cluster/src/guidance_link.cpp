// SPDX-License-Identifier: GPL-3.0-or-later
#include "guidance_link.h"

#include <fcntl.h>
#include <unistd.h>

#include <vector>

#include <vsomeip/vsomeip.hpp>

#include "log.h"
#include "route_box.h"

namespace woc {

namespace {

const vsomeip::service_t kService = 0xFB1B;   /* GuidanceEventList */
const vsomeip::instance_t kInstance = 0x0080;
const vsomeip::major_version_t kMajor = 5;
const vsomeip::event_t kEvent = 0x8002;        /* the route box of each screen */
const vsomeip::eventgroup_t kGroup = 0x0002;
const char kAppName[] = "woc-guidance";

}  // namespace

const int64_t GuidanceLink::kSettleMs;

GuidanceLink::GuidanceLink()
{
    if (pipe2(wake_, O_NONBLOCK | O_CLOEXEC) < 0)
        wake_[0] = wake_[1] = -1;
}

GuidanceLink::~GuidanceLink()
{
    stop();
    if (wake_[0] >= 0) close(wake_[0]);
    if (wake_[1] >= 0) close(wake_[1]);
}

void GuidanceLink::wake()
{
    char c = 0;
    ssize_t n = write(wake_[1], &c, 1);
    (void)n;
}

bool GuidanceLink::start(std::string *err)
{
    if (wake_[0] < 0) {
        *err = "pipe failed";
        return false;
    }
    /* No VSOMEIP_CONFIGURATION: the unit's /etc/vsomeip, as the HMI and
     * cluster-render have it. */
    app_ = vsomeip::runtime::get()->create_application(kAppName);
    if (!app_ || !app_->init()) {
        app_.reset();
        *err = "vsomeip application did not initialise";
        return false;
    }
    app_->register_availability_handler(
        kService, kInstance, [this](vsomeip::service_t, vsomeip::instance_t, bool up) { on_availability(up); },
        vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);
    app_->register_message_handler(kService, kInstance, kEvent,
                                   [this](const std::shared_ptr<vsomeip::message> &m) { on_message(m); });
    app_->register_subscription_status_handler(
        kService, kInstance, kGroup, kEvent,
        [](const vsomeip::service_t, const vsomeip::instance_t, const vsomeip::eventgroup_t, const vsomeip::event_t,
           const uint16_t code) {
            if (code == 0)
                log_info("guidance: the navigation's route box subscribed");
            else
                log_warn("guidance: the route box's subscription refused (%u)", (unsigned)code);
        },
        true);
    app_->request_service(kService, kInstance, kMajor, vsomeip::ANY_MINOR);
    std::shared_ptr<vsomeip::application> app = app_;
    thread_ = std::thread([app] { app->start(); });
    log_info("guidance: vsomeip application %s, waiting for the navigation's route box (0x%04x.0x%04x)", kAppName,
             (unsigned)kService, (unsigned)kInstance);
    return true;
}

void GuidanceLink::stop()
{
    if (!app_)
        return;
    bool subscribed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribed = subscribed_;
    }
    if (subscribed) {
        app_->unsubscribe(kService, kInstance, kGroup);
        app_->release_event(kService, kInstance, kEvent);
    }
    app_->release_service(kService, kInstance);
    app_->clear_all_handler();
    app_->stop();
    if (thread_.joinable())
        thread_.join();
    app_.reset();
}

void GuidanceLink::on_availability(bool up)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (up == up_)
            return;
        up_ = up;
        if (up)
            up_since_ = std::chrono::steady_clock::now();
        if (!up && maneuver_.kind != Maneuver::None) {
            maneuver_ = Maneuver();
            changed_ = true;
        }
    }
    log_info("guidance: the navigation's route box %s", up ? "offered" : "gone");
    wake();
}

void GuidanceLink::on_message(const std::shared_ptr<vsomeip::message> &msg)
{
    const std::shared_ptr<vsomeip::payload> pl = msg ? msg->get_payload() : nullptr;
    if (!pl)
        return;
    std::vector<RouteBoxEntry> entries;
    if (!parse_route_box(pl->get_data(), pl->get_length(), kClusterRouteBox, &entries))
        return;   /* about other screens only */
    const Maneuver m = route_box_maneuver(entries);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        notifications_++;
        if (!entries.empty()) {
            const RouteBoxEntry &e = entries.front();
            if (!icon_known(e.icon) && !logged_icons_.count(e.icon))
                new_icons_.insert(e.icon);
            if (!unit_known(e.unit) && !logged_units_.count(e.unit))
                new_units_.insert(e.unit);
        }
        if (m == maneuver_ && new_icons_.empty() && new_units_.empty() && first_logged_)
            return;
        if (m != maneuver_) {
            maneuver_ = m;
            changed_ = true;
        }
    }
    wake();
}

bool GuidanceLink::take(Maneuver *m)
{
    if (!app_)
        return false;
    char buf[64];
    while (read(wake_[0], buf, sizeof buf) > 0) {
    }
    bool subscribe = false, changed = false, first = false;
    std::set<uint32_t> icons, units;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (up_ && !subscribed_ &&
            std::chrono::steady_clock::now() - up_since_ >= std::chrono::milliseconds(kSettleMs)) {
            subscribed_ = true;
            subscribe = true;
        }
        if (notifications_ && !first_logged_) {
            first_logged_ = true;
            first = true;
        }
        icons.swap(new_icons_);
        units.swap(new_units_);
        logged_icons_.insert(icons.begin(), icons.end());
        logged_units_.insert(units.begin(), units.end());
        if (changed_) {
            *m = maneuver_;
            changed_ = false;
            changed = true;
        }
    }
    if (subscribe) {
        /* outside the lock: vsomeip may call the handlers from here */
        app_->request_event(kService, kInstance, kEvent, std::set<vsomeip::eventgroup_t>{kGroup},
                            vsomeip::event_type_e::ET_UNKNOWN, vsomeip::reliability_type_e::RT_UNKNOWN);
        app_->subscribe(kService, kInstance, kGroup, kMajor);
        log_info("guidance: asking for the route box, its service offered %lld s ago",
                 (long long)(kSettleMs / 1000));
    }
    if (first)
        log_info("guidance: the first route box for the cluster came");
    for (uint32_t i : icons)
        log_info("guidance: route box icon %u not known yet: a plain mark on the card", (unsigned)i);
    for (uint32_t u : units)
        log_info("guidance: route box distance unit %u not known yet: the number alone", (unsigned)u);
    return changed;
}

}  // namespace woc
