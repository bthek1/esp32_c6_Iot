// WS2812B strip as a Matter Extended Color Light, commissioned into SmartThings
// (or any Matter controller) over Wi-Fi. On/Off, Level Control and Color Control
// attribute changes drive lib/strip's strip_fx engine. The richer animated
// patterns stay available via the webserver target; both go through strip_fx.

#include "strip.h"
#include "strip_fx.h"
#include "serial.h"

#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_system.h>
#include <esp_matter.h>
#include <esp_matter_endpoint.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/logging/LogV.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;

#define STRIP_PIN   2
#define STRIP_COUNT (60 * 5)    // 5 m x 60 LED/m = 300

// Last hue/saturation from the Color Control cluster (Matter units, 0..254).
static uint8_t s_hue = 0, s_sat = 0;

// Saturation-aware HSV -> RGB (h 0..359, s/v 0..255).
static void hsv_to_rgb(int h, int s, int v, uint8_t *r, uint8_t *g, uint8_t *b) {
    int c = v * s / 255;
    int x = c * (60 - abs((h % 120) - 60)) / 60;
    int m = v - c, rr = 0, gg = 0, bb = 0;
    switch ((h / 60) % 6) {
        case 0: rr = c; gg = x; break;
        case 1: rr = x; gg = c; break;
        case 2: gg = c; bb = x; break;
        case 3: gg = x; bb = c; break;
        case 4: rr = x; bb = c; break;
        default: rr = c; bb = x; break;
    }
    *r = (uint8_t)(rr + m);
    *g = (uint8_t)(gg + m);
    *b = (uint8_t)(bb + m);
}

// Recompute the strip's base colour from the cached hue/sat at full value;
// brightness is handled separately by the Level Control cluster.
static void apply_color(void) {
    uint8_t r, g, b;
    hsv_to_rgb((int)s_hue * 360 / 254, (int)s_sat * 255 / 254, 255, &r, &g, &b);
    strip_fx_set_color(r, g, b);    // does not change mode; tints solid + patterns
}

// Called by the Matter stack whenever an attribute changes.
static esp_err_t attribute_update_cb(attribute::callback_type_t type,
                                     uint16_t endpoint_id, uint32_t cluster_id,
                                     uint32_t attribute_id, esp_matter_attr_val_t *val,
                                     void *priv) {
    if (type != attribute::PRE_UPDATE) return ESP_OK;
    using namespace chip::app::Clusters;

    if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        strip_fx_set_mode(val->val.b ? STRIP_FX_SOLID : STRIP_FX_OFF);
        serial_println("matter on/off -> %d", val->val.b);
    } else if (cluster_id == LevelControl::Id &&
               attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
        strip_fx_set_brightness(val->val.u8);          // 0..254 ~ 0..255 scale
        serial_println("matter level -> %d", val->val.u8);
    } else if (cluster_id == ColorControl::Id) {
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id)              s_hue = val->val.u8;
        else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id)  s_sat = val->val.u8;
        apply_color();
        serial_println("matter color -> h=%d s=%d", s_hue, s_sat);
    }
    return ESP_OK;
}

// CHIP routes its logs through this redirect; we drop the high-volume Progress/
// Detail traffic from the message-layer modules (chip[EM] "Msg RX/TX/Ack",
// chip[DMG], chip[IM] status spam) so the console stays readable. Errors from
// those modules still pass through, and every other module logs as normal via
// the default ESP32 backend. Our own serial_println status lines are printf and
// bypass this entirely.
static void chip_log_filter(const char *module, uint8_t category,
                            const char *msg, va_list args) {
    if (category > chip::Logging::kLogCategory_Error &&
        (strcmp(module, "EM") == 0 || strcmp(module, "DMG") == 0 ||
         strcmp(module, "IM") == 0)) {
        return;
    }
    chip::Logging::Platform::LogV(module, category, msg, args);
}

// Report commissioning / connectivity milestones over the serial console so the
// boot log shows exactly where we are: advertising, paired, on Wi-Fi, etc.
static void matter_event_cb(const ChipDeviceEvent *event, intptr_t arg) {
    using namespace chip::DeviceLayer;
    switch (event->Type) {
        case DeviceEventType::kCommissioningSessionStarted:
            serial_println("matter: commissioning session started (controller connected)");
            break;
        case DeviceEventType::kCommissioningSessionStopped:
            serial_println("matter: commissioning session stopped");
            break;
        case DeviceEventType::kCommissioningComplete:
            serial_println("matter: COMMISSIONING COMPLETE — device is now paired");
            break;
        case DeviceEventType::kFailSafeTimerExpired:
            serial_println("matter: fail-safe timer expired (commissioning did not finish)");
            break;
        case DeviceEventType::kFabricRemoved:
            serial_println("matter: fabric removed — device de-commissioned");
            break;
        case DeviceEventType::kCHIPoBLEAdvertisingChange:
            serial_println("matter: BLE advertising %s",
                           event->CHIPoBLEAdvertisingChange.Result == kActivity_Started ? "started" : "stopped");
            break;
        case DeviceEventType::kInterfaceIpAddressChanged:
            if (event->InterfaceIpAddressChanged.Type == InterfaceIpChangeType::kIpV4_Assigned)
                serial_println("matter: Wi-Fi got an IPv4 address — reachable on the network");
            else if (event->InterfaceIpAddressChanged.Type == InterfaceIpChangeType::kIpV6_Assigned)
                serial_println("matter: Wi-Fi got an IPv6 address");
            break;
        case DeviceEventType::kServerReady:
            serial_println("matter: server ready");
            break;
        default:
            break;
    }
}

extern "C" void app_main(void) {
    serial_init();
    serial_println("==================================================");
    serial_println(" matter_strip — WS2812B Matter Extended Color Light");
    serial_println(" target: %s  idf: %s", "matter_strip", esp_get_idf_version());
    serial_println(" strip: gpio %d, %d leds", STRIP_PIN, STRIP_COUNT);
    serial_println("==================================================");

    // Quiet CHIP's chatty message-layer modules before the stack starts logging.
    chip::Logging::SetLogRedirectCallback(chip_log_filter);
    bool strip_ok = strip_init(STRIP_PIN, STRIP_COUNT);
    serial_println(strip_ok ? "strip init OK (gpio %d, %d leds)" : "strip init FAILED (gpio %d, %d leds)",
                   STRIP_PIN, STRIP_COUNT);

    strip_fx_start();                 // background render task (lib/strip); starts OFF
                                      // until the controller turns the light on.

    // Matter node + an Extended Color Light endpoint.
    node::config_t node_cfg;
    node_t *node = node::create(&node_cfg, attribute_update_cb, NULL);

    extended_color_light::config_t light_cfg;
    light_cfg.on_off.on_off = false;  // start OFF
    // Each device type has its own create() (not the generic endpoint::create).
    endpoint_t *ep = extended_color_light::create(node, &light_cfg, ENDPOINT_FLAG_NONE, NULL);

    // esp-matter's extended_color_light enables the colour-temperature + XY colour
    // features but NOT Hue/Saturation, so CurrentHue/CurrentSaturation come back
    // UNSUPPORTED and our HSV callback never fires. Add the Hue/Saturation feature
    // so controllers can drive colour the way strip_fx expects.
    cluster_t *cc = cluster::get(ep, chip::app::Clusters::ColorControl::Id);
    cluster::color_control::feature::hue_saturation::config_t hs_cfg;
    cluster::color_control::feature::hue_saturation::add(cc, &hs_cfg);

    // Start the Matter stack; matter_event_cb reports commissioning/Wi-Fi status.
    esp_matter::start(matter_event_cb);
    serial_println("matter: stack started");

    // Print the manual pairing code + QR payload explicitly so they're easy to
    // find in the boot log (also use for Home Assistant's "add Matter device").
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
}
