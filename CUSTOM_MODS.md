# Custom fork modifications — full reconstruction reference

Everything needed to rebuild this state from a clean checkout of Meshtastic firmware.
The goal: LoRa **RX/TX activity indicators** (NeoPixel + discrete LEDs) that mirror node
status, an **audible/visual ACK ping test**, and a few per-board fixes — across three boards.

This document is self-contained: the verbatim variant blocks and the key shared-code snippets
below are enough to recreate the feature even if the branch is lost.

---

## 1. Git / repo state

| Item            | Value                                                          |
| --------------- | -------------------------------------------------------------- |
| Base branch     | `develop` (upstream `origin` = `meshtastic/firmware`)          |
| Work branch     | `feat/neopixel-lora-activity`                                  |
| Fork remote     | `fork` = `https://github.com/Timon-sys/firmware.git` (public)  |
| Commits on top  | `9dcdd9632` (XIAO NeoPixel) → `b8546118c` (T1000-E + ACK ping) |

> Hashes change on any rebase/amend; identify commits by subject if they don't match.
> The RAK3401 board, the notification-pixel colour, the single-press log, and this doc were the
> most recent work — commit them as a third commit on the branch.

Remotes are split deliberately: `origin` stays upstream (pull/rebase from it), `fork` is push
target. Update flow: `git fetch origin && git rebase origin/develop && git push --force-with-lease fork`.

Commit-message trailers (`Co-Authored-By`, `Claude-Session`) were stripped from history at the
user's request — keep them out of future commits on this branch.

---

## 2. Signal / colour legend

| Signal        | Colour               | Meaning                                       | Priority |
| ------------- | -------------------- | --------------------------------------------- | -------- |
| RX flash      | green `0x008000` @50% | Heard a valid LoRa packet (200 ms)           | highest  |
| TX flash      | amber `0x803000` @50% | Transmitting — true on-air window            | highest  |
| Heartbeat     | green `0x000A00` @~4% | Idle / charge state (dim pulse)              | middle   |
| Notification  | blue  `0x000080` @50% | Unread message nagging                       | lowest   |

Priority on a shared pixel is **RX/TX flash > heartbeat/charge > notification**: the heartbeat
pulse always shows; a pending message only tints the heartbeat's off-gaps blue (single pixel,
so a lower-priority colour can only appear when a higher one isn't lit).

> XIAO/T1000-E use the 50% values above. **RAK3401 runs RX/TX at full brightness**
> (`0x00FF00` green, `0xFF6000` amber) — the APA106 at 3.3V is dimmer, and full makes the
> quick flashes pop against the dim heartbeat.

On the T1000-E the RX/TX use two separate discrete LEDs instead of one pixel.

Hex is `0xRRGGBB`; `Adafruit_NeoPixel::setPixelColor` remaps to the strip's byte order, so the
same hex gives the right colour on both `NEO_GRB` (WS2812) and `NEO_RGB` (WS2811) strips.

---

## 3. Per-board variant.h blocks (verbatim)

### 3a. Seeed XIAO ESP32-S3 — `variants/esp32s3/seeed_xiao_s3/variant.h`

WS2812B on **GPIO 2 (pad D1)**. Also: the `BATTERY_PIN -1` line was **commented out** (see §6).

```c
#define LED_POWER 48
#define LED_STATE_ON 1 // State when LED is lit

// Optional external WS2812B status pixel on D1 (GPIO2), mirroring LED_POWER:
// 1s heartbeat, blink while charging, solid when charged, fast blink on
// critical battery. Driven as a 1-pixel strand by StatusLEDModule.
#define NEOPIXEL_STATUS_POWER_PIN 2
// Very dim green for the idle heartbeat: "all is well" should not look like a fault.
// Kept far below the 50% RX green so the two are easy to tell apart.
#define NEOPIXEL_STATUS_POWER_COLOR 0x000A00 // green @ ~4% brightness
// LoRa activity flashes at 50% so they stand out against the dimmer heartbeat.
// Radio convention: amber = we are transmitting, green = we heard someone.
#define NEOPIXEL_STATUS_LORA_TX_COLOR 0x803000 // amber @ 50%
#define NEOPIXEL_STATUS_LORA_RX_COLOR 0x008000 // green @ 50%
```

And the battery fix (originally `#define BATTERY_PIN -1` + `ADC_CHANNEL` + resolution):

```c
// This board has no battery sense hardware. BATTERY_PIN must be left *undefined*
// rather than set to a -1 sentinel: PowerStatus.h keys off defined(BATTERY_PIN),
// so defining it selects the "0% when no battery" branch, which StatusLEDModule
// reads as a critical battery and answers with its 30s fast-blink alarm.
// #define BATTERY_PIN -1
// #define ADC_CHANNEL ADC_CHANNEL_0
// #define BATTERY_SENSE_RESOLUTION_BITS 12
```

Default pixel type is `NEO_GRB` (from StatusLEDModule.h), correct for a WS2812B.

### 3b. Seeed T1000-E — `variants/nrf52840/tracker-t1000-e/variant.h`

Two **discrete onboard LEDs**: green `PIN_LED1` (P0.24), red `PIN_LED2` (P0.03 — undocumented
by Seeed but wired). RX→green, TX→red.

```c
#define PIN_LED1 (0 + 24) // P0.24, green
#define PIN_LED2 (0 + 3)  // P0.03, red - undocumented by Seeed, but wired
#define LED_POWER PIN_LED1
#define LED_BLUE -1    // Actually green
#define LED_STATE_ON 1 // State when LED is lit

// LoRa activity: green flashes on each received packet, red while transmitting.
// The green LED is shared with the heartbeat; the flash wins and the heartbeat
// resumes when it expires.
#define LED_LORA_RX PIN_LED1
#define LED_LORA_TX PIN_LED2
```

Existing button is P0.06, active-high + pull-down (`BUTTON_SENSE_TYPE 0x5`) — this is the proven
active-high/pull-down pattern later reused on the RAK3401.

### 3c. RAK3401 1W — `variants/nrf52840/rak3401_1watt/variant.h`

**APA106** pixel on **WB_IO1 / P0.17** (GRB order, 800 kHz) + **button on AIN1 / P0.31**.
(Originally a WS2811 (RGB) inside a NeoPixel-button unit; later swapped for a discrete APA106
which turned out to be **GRB** — red/green came out swapped under NEO_RGB, blue fine, so the
type was changed to NEO_GRB. APA106 colour order varies by batch; verify visually on any swap.)

```c
// External APA106 status pixel on WB_IO1 / P0.17. Mirrors the onboard status LEDs
// (heartbeat, charge, critical-battery fast-blink) and flashes green on LoRa RX / amber
// on TX, which take priority over the status colour. Reuses StatusLEDModule's NeoPixel path.
// NOTE: this APA106 is wired GRB (red/green came out swapped under NEO_RGB, blue fine - the
// classic GRB signature), so NEO_GRB is correct here. The WS2811 it replaced was RGB; APA106
// colour order varies by batch, so verify visually if the pixel is ever changed again.
#define NEOPIXEL_STATUS_POWER_PIN 17
#define NEOPIXEL_STATUS_TYPE (NEO_GRB + NEO_KHZ800) // APA106 (this unit): GRB order, 800 kHz
#define NEOPIXEL_STATUS_POWER_COLOR 0x000A00   // dim green idle heartbeat
#define NEOPIXEL_STATUS_LORA_RX_COLOR 0x008000 // green @ 50% - heard something over RF
#define NEOPIXEL_STATUS_LORA_TX_COLOR 0x803000 // amber @ 50% - transmitting

// User button on AIN1 / P0.31 - a clean analog GPIO with no external pull-up (unlike the I2C
// pads, whose hardware pull-up pinned the pin high and defeated the pull-down). The button
// circuit's ~2.5V active-high output reads correctly here with the internal pull-down:
// floating -> LOW, pressed -> HIGH.
#define BUTTON_PIN 31
#define BUTTON_ACTIVE_LOW false
#define BUTTON_ACTIVE_PULLUP false
#define BUTTON_SENSE_TYPE 0x5 // nRF input pull-down (+ sense for wake)
```

The RAK3401 has **no buzzer** (`PIN_BUZZER` commented out), so its ACK test is silent — the
double-press ping fires but produces no beep. `BATTERY_PIN` is real here (`PIN_A0`), so charge
mirroring reflects the actual battery.

---

## 4. Shared code (`src/`) — what to add and where

All gated so boards without the defines are unaffected. Already in commits `9dcdd9632` /
`b8546118c`; snippets below are the load-bearing pieces.

### 4a. `configuration.h` — feature gate (after `#include "variant.h"`)

```c
// Any of these ask RadioLibInterface to publish LoRa RX/TX events so a status
// LED (discrete GPIO or WS2812) can flash on radio activity.
#if defined(LED_LORA) || defined(LED_LORA_RX) || defined(LED_LORA_TX) || defined(NEOPIXEL_STATUS_POWER_PIN)
#define HAS_LORA_ACTIVITY_INDICATOR 1
#endif
```

### 4b. `mesh/RadioInterface.{h,cpp}` — observables

Add two `static Observable<uint32_t>` next to the existing `loraRxPacketObservable`:
`loraTxPacketObservable` (TX start) and `loraTxDoneObservable` (TX complete). Define both in
the `.cpp` alongside the RX one.

### 4c. `mesh/RadioLibInterface.cpp` — fire them

- In the RX path (after `printPacket("Lora RX", ...)`), under `#ifdef HAS_LORA_ACTIVITY_INDICATOR`:
  `loraRxPacketObservable.notifyObservers(mp->from);`
- In `startSend`/transmit-start (next to the existing `LED_LORA` `digitalWrite`), under the same
  guard: `loraTxPacketObservable.notifyObservers(txp->to);`
- In `completeSending()` (next to the `LED_LORA` off), under the same guard:
  `loraTxDoneObservable.notifyObservers(0);`  ← this is the **true off-air** edge.

### 4d. `modules/StatusLEDModule.{h,cpp}` — the indicator logic

Colour defaults (header, inside the NeoPixel `#if` block):
```c
#ifndef NEOPIXEL_STATUS_LORA_TX_COLOR
#define NEOPIXEL_STATUS_LORA_TX_COLOR 0x000040 // blue @ 25% (default; boards override)
#endif
#ifndef NEOPIXEL_STATUS_LORA_RX_COLOR
#define NEOPIXEL_STATUS_LORA_RX_COLOR 0x400020 // pink @ 25%
#endif
#ifndef NEOPIXEL_STATUS_POWER_NOTIFY_COLOR
#define NEOPIXEL_STATUS_POWER_NOTIFY_COLOR 0x000080 // blue @ 50% - unread message
#endif
```

Behaviour:
- Observers registered under `HAS_LORA_ACTIVITY_INDICATOR`; also `pinMode`/init any
  `LED_LORA_RX` / `LED_LORA_TX` discrete pins.
- `handleLoRaRx()` — flash RX colour on the pixel and/or drive `LED_LORA_RX`. **Suppressed while
  TX is active** (`if (!LORA_TX_LED_active)`) so a half-duplex radio never shows green+red at once.
  Pixel RX is a `RF_PIXEL_FLASH_MS` = 120 ms timed flash; the discrete `LED_LORA_RX` uses
  `LORA_ACTIVITY_LED_FLASH_MS` = 200 ms. Both are cleared by `runOnce()` (see §7 flash-duration note).
- `handleLoRaTx()` — light TX colour / `LED_LORA_TX` immediately (edge on, no timer).
- `handleLoRaTxDone()` — clear TX immediately (edge off). Done here, **not** in `runOnce()`,
  because a long send starves the OSThread and would strand the LED on past the transmission.
  `runOnce()` keeps only a 5 s watchdog (`LORA_TX_LED_MAX_MS`).
- `runOnce()` pixel priority (the tiered write):
```c
    if (RF_pixel_color) {
        writeStatusPixel(powerPixel, RF_pixel_color, true);
    }
#if !MESHTASTIC_EXCLUDE_EXTERNALNOTIFICATION
    else if (externalNotificationModule && externalNotificationModule->nagging()) {
        // Unread-message notification: steady colour, below the RF flash but above the heartbeat.
        writeStatusPixel(powerPixel, NEOPIXEL_STATUS_POWER_NOTIFY_COLOR, true);
        if ((uint32_t)my_interval > 250)
            my_interval = 250; // poll faster so the pixel reverts promptly once the nag ends
    }
#endif
    else {
        writeStatusPixel(powerPixel, NEOPIXEL_STATUS_POWER_COLOR, CHARGE_LED_state == LED_STATE_ON);
    }
```
  (`#include "modules/ExternalNotificationModule.h"` guarded by the same exclude macro.)

### 4e. `input/ButtonThread.cpp` — visible single press

In the `BUTTON_EVENT_PRESSED` case, add `LOG_INFO("Single press!");` (previously only
double/multi-press logged, so a working single press looked silent).

### 4f. `modules/SystemCommandsModule.cpp` — double-press = "testack" ping

Replace the `INPUT_BROKER_SEND_PING` position/nodeinfo ping with a real text packet that
requests an ACK (`#include "Router.h"` + `#include "modules/RoutingModule.h"`):

```c
        meshtastic_MeshPacket *p = router->allocForSending();
        if (p) {
            p->to = NODENUM_BROADCAST;
            p->channel = 0; // primary channel
            p->want_ack = true;
            p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
            const char *msg = "testack";
            p->decoded.payload.size = strlen(msg);
            memcpy(p->decoded.payload.bytes, msg, p->decoded.payload.size);
            // Arm the ACK beep for this packet only, so chat traffic doesn't trigger it.
            ackBeepPendingId = p->id;
            playPingSentBeep(); // medium beep: sent - listen for the high ACK beep that follows
            service->sendToMesh(p, RX_SRC_LOCAL, true);
            IF_SCREEN(screen->showSimpleBanner("testack\nSent", 3000));
        }
        return true;
```

### 4g. `modules/RoutingModule.{h,cpp}` — ACK/timeout detection

Global `PacketId ackBeepPendingId = 0;` (declared `extern` in the header). In
`handleReceivedProtobuf`, after `router->sniffReceived(...)`:

```c
    // Audible outcome for a button-initiated ACK test only: high beep on ACK, low beep when the
    // router gives up. Matching on request_id keeps ordinary chat ACKs silent.
    if (ackBeepPendingId && isToUs(&mp) && r && mp.which_payload_variant == meshtastic_MeshPacket_decoded_tag &&
        mp.decoded.request_id == ackBeepPendingId) {
        if (r->error_reason == meshtastic_Routing_Error_NONE) {
            playAckBeep();
            ackBeepPendingId = 0;
        } else if (r->error_reason == meshtastic_Routing_Error_MAX_RETRANSMIT) {
            playAckFailBeep();
            ackBeepPendingId = 0;
        }
    }
```
`request_id` is set on every ACK/NAK by `MeshModule::allocAckNak` (incl. implicit ACKs from
`ReliableRouter`), so matching it isolates *our* ping's result from ordinary chat ACKs.

### 4h. `buzz/buzz.{h,cpp}` — three tones

```c
void playPingSentBeep() { ToneDuration m[] = {{NOTE_B4, DURATION_1_8}}; playTones(m, 1); } // medium
void playAckBeep()      { ToneDuration m[] = {{NOTE_E7, DURATION_1_8}}; playTones(m, 1); } // high
void playAckFailBeep()  { ToneDuration m[] = {{NOTE_C3, DURATION_1_4}}; playTones(m, 1); } // low, longer
```
(Notes chosen from the I2S lookup table so speaker boards map them correctly.)

### 4i. `modules/NodeInfoModule.cpp` — throttle removal (LOCAL ONLY)

In `allocReply()`, the 10-minute / 60-second NodeInfo resend throttles were deleted so a button
ping is never suppressed. Marked with `// LOCAL MODIFICATION:`. The `>40%` channel-utilisation
gate above it still applies. **Drop this for any upstream PR.**

---

## 5. Build & flash

```
pio run -e seeed-xiao-s3   -t upload   # ESP32-S3: requires_dfu — hold BOOT, tap RESET at "Connecting..."
pio run -e tracker-t1000-e -t upload   # nRF52: serial DFU, 1200bps touch (automatic, no buttons)
pio run -e rak3401-1watt   -t upload   # nRF52: serial DFU, 1200bps touch (automatic)
pio device monitor -b 115200           # serial logs
```

- ESP32-S3 build ≈ 3–7 min; nRF52 ≈ 1–2 min.
- nRF52 upload fails if a serial monitor holds the port — close it first.
- Fallback for nRF52: double-tap RESET → UF2 drive appears → drag `.pio/build/<env>/firmware-*.uf2`.
- After a regression on shared code, rebuild **all three** envs before committing.

---

## 6. Hardware wiring

### XIAO ESP32-S3 (WS2812B)
- DATA → **GPIO 2 / pad D1**; VCC → 5V pad (USB only) or 3V3; GND common.
- 470 Ω inline on DATA + 1000 µF across VCC/GND recommended. 3.3V data into a 5V WS2812B is
  marginal — use a level shifter or 3V3 power if it flickers.

### T1000-E (discrete LEDs)
- No wiring — uses the two onboard LEDs (green P0.24, red P0.03).

### RAK3401 1W — final wiring (APA106-F5 + button, all at 3.3V, no boost)
- **APA106-F5** (5mm through-hole, pins VDD / DOUT / VSS / DIN): **VDD → 3V3**, **VSS → GND**,
  **DIN → WB_IO1 / P0.17**. Runs directly at 3.3V — no 5V step-up. Because the LED's VDD is
  3.3V, its logic-high threshold (~0.7×VDD ≈ 2.3V) is below the nRF's 3.3V data level, so the
  data line is a solid high with no level shifter (slightly dimmer than at 5V, fully reliable).
  This unit is **GRB** order (see §3c).
- **Button:** plain switch **VDD (3.3V) → AIN1 / P0.31**. Pressed = HIGH (3.3V), released =
  internal pull-down holds LOW. Active-high, no external parts. Lots of margin vs the old ~2.5V.
- **Do not use SDA/SCL (P0.13/P0.14) for the button:** the WisBlock base has a hardware I2C
  pull-up that idles the pin ~2.5V and defeats the internal pull-down (reads permanently HIGH).
  AIN1 is a bare GPIO with no such pull-up. (If a button ever must sit on I2C, wire it as a
  switch-to-GND and configure it **active-low**, letting the I2C pull-up be the idle-high.)
- IO2–IO6 are taken by the 1W LoRa module; IO1 is the one free IO-slot pin.
- History: started as a WS2811 (RGB) inside a NeoPixel-button unit with a 3V3→5V boost and a
  diode/resistor active-high button circuit; simplified to a plain APA106-F5 + switch, all 3.3V.

---

## 7. Debugging lessons (so they aren't rediscovered the hard way)

- **XIAO false low-battery alarm:** `BATTERY_PIN -1` still satisfies `defined(BATTERY_PIN)`, so
  `PowerStatus.h` returns 0% instead of the "no battery = 101%" branch → `StatusLEDModule` sees
  critical battery → 4×500 ms fast blink every 30 s. Fix = leave `BATTERY_PIN` undefined.
- **TX LED stuck on / "orange":** a fixed-timer TX flash cleared in `runOnce()` stayed lit for
  seconds on slow presets (thread starved during long TX) and could overlap an RX flash. Fix =
  edge-drive TX from `loraTxDoneObservable` (`completeSending`) and suppress RX during TX.
- **No ACK ever:** `wantReplies` sets `want_response` (a reply request), **not** `want_ack`; and
  tracker/sensor roles strip even that. Fix = send a text packet with `want_ack = true`.
- **Every chat ACK beeped:** gate on the ping's `request_id` (`ackBeepPendingId`).
- **RAK button dead on SDA:** hardware I2C pull-up (~2.47 V measured, nothing connected) beats
  the internal pull-down. Moved to AIN1/P0.31.
- **RX flash lasts ~300 ms on the RAK, not the nominal 120 ms** (verified on serial, COM29, two
  packets): the pixel is *lit* synchronously in `handleLoRaRx()`, but it's *cleared* by `runOnce()`
  on the single main loop. Receiving a **PKC-signed** packet (XEdDSA / Ed25519) blocks that loop
  ~296 ms on the nRF52840 (Cortex-M4 @ 64 MHz, no crypto accel) while it verifies the signature, so
  `runOnce()` can't clear the flash until the crypto finishes. Evidence — consecutive log lines with
  a ~296 ms gap at the crypto step, both packets:
  `Lora RX 21:26:58.065` → `Verified XEdDSA signature 21:26:58.078` → `decoded message 21:26:58.374`.
  Not a bug/hang: it self-clears once the loop frees, and a plain *unsigned* broadcast clears near
  120 ms. Less visible on the XIAO (ESP32-S3 is much faster at this crypto); the T1000-E is the same
  nRF52840 so it is subject to the same stretch — it just went unnoticed there. The amber seen with
  these packets is the RAK **relaying** the testack (a genuine ~283 ms rebroadcast TX). A real fix
  would clear the pixel from a hardware timer (nRF app_timer) instead of `runOnce()`, decoupling the
  120 ms off-edge from main-loop blocking — deferred for now.

### Arduino-IDE pin test sketch (RAK4631 core)
Board = **WisBlock RAK4631**, USB Stack = **TinyUSB**. The include is required or `Serial`
(USB CDC) won't link:
```cpp
#include <Adafruit_TinyUSB.h>   // required on RAK/Adafruit nRF52 or Serial won't link
#define BTN SDA                 // or PIN_WIRE_SDA / any pad under test
void setup(){ Serial.begin(115200); while(!Serial) delay(10); }
void loop(){
  pinMode(BTN, INPUT);          int raw = digitalRead(BTN);
  pinMode(BTN, INPUT_PULLDOWN); delayMicroseconds(200); int pd = digitalRead(BTN);
  Serial.printf("raw=%d pulldown=%d\n", raw, pd); delay(150);
}
```
Reading: raw 0→1 but pulldown stuck 0 = pull-down beaten (external pull-up / weak source);
raw stuck 0 = circuit not driving high; both 1 = pin fine, look at peripheral ownership (I2C/UART).

---

## 8. Caveats before any upstream PR

- **NodeInfo throttle removal** (§4i) removes an airtime protection — local only, drop for PR.
- **`"testack"` double-press** sends a visible text to every node on the primary channel and is
  global (any board with a button), not scoped to one variant.
- **Per-board pixel pins / colours** are personal hardware mods, not sensible defaults for every
  owner — split from the generic feature for a PR.
- **The XIAO `BATTERY_PIN` fix** is a genuine standalone bug fix and PRs cleanly on its own.
- A cleaner upstream form of the battery gate would test `BATTERY_PIN >= 0` rather than
  `defined(BATTERY_PIN)`, fixing the sentinel footgun for all variants at once.

---

## 9. Possible next steps (discussed, not yet done)

- **Pixel-based ACK feedback** for buzzer-less boards (e.g. RAK3401): white flash on ACK, dim-red
  on timeout, below RX/TX priority — the visual equivalent of the ACK/timeout beeps.
- Scope the `"testack"` double-press to specific variants if it shouldn't be global.
