#include <Arduino.h>
#include "Config.h"
#include "RtosUtil.h"
#include "AppState.h"
#include "BleHandler.h"
#include "MpuHandler.h"
#include "TouchHandler.h"
#include "ActionMapper.h"

#if ENABLE_DEBUG_CONSOLE
#include "DebugConsole.h"
#endif

static BleHandler   bleHandler;
static MpuHandler   mpuHandler;
static TouchHandler touchHandler;
static ActionMapper actionMapper;

#if ENABLE_DEBUG_CONSOLE
static DebugConsole debugConsole;
#endif

static TaskHandle_t sensorTaskHandle = nullptr;

static const char *mpuStateLabel(MpuState state) {
    switch (state) {
        case MpuState::SEARCHING:   return "mencari";
        case MpuState::CALIBRATING: return "kalibrasi";
        case MpuState::RUNNING:     return "aktif";
    }
    return "?";
}

static void sensorTaskEntry(void *pvParameters) {
    (void)pvParameters;

    touchHandler.begin();
    mpuHandler.begin();
    actionMapper.begin(&bleHandler, &mpuHandler);

#if ENABLE_DEBUG_CONSOLE
    debugConsole.begin(&actionMapper, &bleHandler, &mpuHandler);
#endif

    Serial.println(F("[SensorTask] Aktif di Core 1. Mode awal: MODE 1 (Navigasi Kursor)"));

    unsigned long lastStatusPrint = millis();
    unsigned long motionFreezeUntil = 0;

    for (;;) {
#if ENABLE_DEBUG_CONSOLE
        debugConsole.update();
#endif

        GestureEvent evt;
        while (touchHandler.update(evt)) {
            actionMapper.handleGesture(evt);
        }

        const unsigned long now = millis();
        const unsigned long touchStamp = touchHandler.lastRawChangeTime();
        if (touchStamp != 0) {
            const unsigned long until = touchStamp + TOUCH_MOTION_FREEZE_MS;
            if ((long)(until - motionFreezeUntil) > 0) motionFreezeUntil = until;
        }
        const bool frozen = (long)(motionFreezeUntil - now) > 0;

        if (actionMapper.getMode() == OperationMode::MODE_1_NAVIGATION &&
            !actionMapper.isMotionPaused()) {
            MouseDelta delta;
            if (mpuHandler.update(delta, frozen)) {
                bleHandler.moveMouse(delta.dx, delta.dy);
            }
        }

        if (now - lastStatusPrint >= STATUS_HEARTBEAT_MS) {
            lastStatusPrint = now;
            Serial.print(F("[Status] BLE: "));
            Serial.print(bleHandler.isConnected() ? F("Connected") : F("Advertising..."));
            Serial.print(F(" | Mode: "));
            Serial.print(actionMapper.getMode() == OperationMode::MODE_1_NAVIGATION ? "1" : "2");
            Serial.print(F(" | Sensor gerak: "));
            Serial.print(actionMapper.isMotionPaused() ? F("dijeda") : F("on"));
            Serial.print(F(" | MPU: "));
            Serial.print(mpuStateLabel(mpuHandler.state()));
            Serial.print(F(" | BLE cmd dropped: "));
            Serial.println(bleHandler.getDroppedCommandCount());
        }

        vTaskDelay(msToTicks(SENSOR_TASK_LOOP_DELAY_MS));
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    delay(200);

    Serial.println();
    Serial.println(F("=== Air Mouse (ESP32-S3 Super Mini) - Dual-Core FreeRTOS ==="));

    bleHandler.begin();

    xTaskCreatePinnedToCore(
        sensorTaskEntry,
        "Sensor_Core1_Task",
        TASK_STACK_SIZE_SENSOR,
        nullptr,
        TASK_PRIORITY_SENSOR,
        &sensorTaskHandle,
        TASK_CORE_SENSOR);

    Serial.println(F("[Main] Task Core 0 (BLE) & Task Core 1 (Sensor) telah dibuat."));
}

void loop() {
    vTaskDelay(msToTicks(1000));
}
