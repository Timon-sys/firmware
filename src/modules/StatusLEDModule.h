#pragma once

#include "BluetoothStatus.h"
#include "MeshModule.h"
#include "PowerStatus.h"
#include "concurrency/OSThread.h"
#include "configuration.h"
#include "main.h"
#include <Arduino.h>
#include <functional>

#if !MESHTASTIC_EXCLUDE_INPUTBROKER
#include "input/InputBroker.h"
#endif

// WS2812/NeoPixel status-LED support. A variant may define
//   NEOPIXEL_STATUS_POWER_PIN   (required to enable the power/charge pixel)
//   NEOPIXEL_STATUS_POWER_COLOR (optional, default red 0xFF0000)
//   NEOPIXEL_STATUS_PAIRING_PIN / _COLOR  (default blue 0x0000FF)
// Each pixel is a standalone 1-LED strand on its own GPIO - this mirrors how
// boards like the LilyGo T-Echo-Card expose three independent WS2812s.
#if defined(NEOPIXEL_STATUS_POWER_PIN) || defined(NEOPIXEL_STATUS_PAIRING_PIN)
#include <Adafruit_NeoPixel.h>
#ifndef NEOPIXEL_STATUS_TYPE
#define NEOPIXEL_STATUS_TYPE (NEO_GRB + NEO_KHZ800)
#endif
#ifndef NEOPIXEL_STATUS_POWER_COLOR
#define NEOPIXEL_STATUS_POWER_COLOR 0xFF0000 // red
#endif
#ifndef NEOPIXEL_STATUS_PAIRING_COLOR
#define NEOPIXEL_STATUS_PAIRING_COLOR 0x0000FF // blue
#endif
// LoRa activity colours flashed on the power pixel: TX blue, RX pink.
#ifndef NEOPIXEL_STATUS_LORA_TX_COLOR
#define NEOPIXEL_STATUS_LORA_TX_COLOR 0x000040 // blue @ 25%
#endif
#ifndef NEOPIXEL_STATUS_LORA_RX_COLOR
#define NEOPIXEL_STATUS_LORA_RX_COLOR 0x400020 // pink @ 25%
#endif
#endif

class StatusLEDModule : private concurrency::OSThread
{
    bool slowTrack = false;

  public:
    StatusLEDModule();

    int handleStatusUpdate(const meshtastic::Status *);
#if !MESHTASTIC_EXCLUDE_INPUTBROKER
    int handleInputEvent(const InputEvent *arg);
#endif
#ifdef HAS_LORA_ACTIVITY_INDICATOR
    int handleLoRaRx(uint32_t sender);
    int handleLoRaTx(uint32_t dest);
    int handleLoRaTxDone(uint32_t);
#endif

    void setPowerLED(bool);

#ifdef NEOPIXEL_STATUS_POWER_PIN
    Adafruit_NeoPixel powerPixel = Adafruit_NeoPixel(1, NEOPIXEL_STATUS_POWER_PIN, NEOPIXEL_STATUS_TYPE);
#endif
#ifdef NEOPIXEL_STATUS_PAIRING_PIN
    Adafruit_NeoPixel pairingPixel = Adafruit_NeoPixel(1, NEOPIXEL_STATUS_PAIRING_PIN, NEOPIXEL_STATUS_TYPE);
#endif

  protected:
    unsigned int my_interval = 1000; // interval in millisconds
    virtual int32_t runOnce() override;

    CallbackObserver<StatusLEDModule, const meshtastic::Status *> bluetoothStatusObserver =
        CallbackObserver<StatusLEDModule, const meshtastic::Status *>(this, &StatusLEDModule::handleStatusUpdate);
    CallbackObserver<StatusLEDModule, const meshtastic::Status *> powerStatusObserver =
        CallbackObserver<StatusLEDModule, const meshtastic::Status *>(this, &StatusLEDModule::handleStatusUpdate);
#if !MESHTASTIC_EXCLUDE_INPUTBROKER
    CallbackObserver<StatusLEDModule, const InputEvent *> inputObserver =
        CallbackObserver<StatusLEDModule, const InputEvent *>(this, &StatusLEDModule::handleInputEvent);
#endif
#ifdef HAS_LORA_ACTIVITY_INDICATOR
    CallbackObserver<StatusLEDModule, uint32_t> loraRxObserver =
        CallbackObserver<StatusLEDModule, uint32_t>(this, &StatusLEDModule::handleLoRaRx);
    CallbackObserver<StatusLEDModule, uint32_t> loraTxObserver =
        CallbackObserver<StatusLEDModule, uint32_t>(this, &StatusLEDModule::handleLoRaTx);
    CallbackObserver<StatusLEDModule, uint32_t> loraTxDoneObserver =
        CallbackObserver<StatusLEDModule, uint32_t>(this, &StatusLEDModule::handleLoRaTxDone);
#endif

  private:
    bool CHARGE_LED_state = LED_STATE_OFF;
    bool PAIRING_LED_state = LED_STATE_OFF;
#if defined(LED_HEARTBEAT)
    bool HEARTBEAT_LED_state = LED_STATE_OFF;
#endif

    uint32_t PAIRING_LED_starttime = 0;
    uint32_t lastUserbuttonTime = 0;
    uint32_t POWER_LED_starttime = 0;
    bool doing_fast_blink = false;
#ifdef LED_LORA
    static constexpr uint32_t LORA_RX_LED_FLASH_MS = 100;
    bool LORA_LED_state = LED_STATE_OFF;
    uint32_t LORA_LED_starttime = 0;
#endif
#ifdef NEOPIXEL_STATUS_POWER_PIN
    // Brief RF activity flash that overrides the heartbeat colour on the power pixel.
    static constexpr uint32_t RF_PIXEL_FLASH_MS = 120;
    uint32_t RF_pixel_color = 0; // 0 = no flash in progress
    uint32_t RF_pixel_starttime = 0;
#endif
#if defined(LED_LORA_RX) || defined(LED_LORA_TX)
    // Discrete-GPIO RF activity LEDs. Either pin may be shared with LED_POWER;
    // the heartbeat simply resumes once the flash expires.
    static constexpr uint32_t LORA_ACTIVITY_LED_FLASH_MS = 200;
#endif
#ifdef LED_LORA_RX
    bool LORA_RX_LED_active = false;
    uint32_t LORA_RX_LED_starttime = 0;
#endif
#ifdef LED_LORA_TX
    // TX is edge-driven: lit for the true on-air window (start -> completeSending), not a fixed
    // duration. The timestamp only backstops a watchdog in case the done event never arrives.
    bool LORA_TX_LED_active = false;
    uint32_t LORA_TX_LED_starttime = 0;
    static constexpr uint32_t LORA_TX_LED_MAX_MS = 5000;
#endif

    enum PowerState { discharging, charging, charged, critical };

    PowerState power_state = discharging;

    enum BLEState { unpaired, pairing, connected };

    BLEState ble_state = unpaired;
};

extern StatusLEDModule *statusLEDModule;
#ifdef RGB_LED_POWER
#include "AmbientLightingThread.h"
extern AmbientLightingThread *ambientLightingThread;
#endif
