#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "BleHidDevice.h"

enum class BleCommandType : uint8_t {
    MOUSE_MOVE = 0,
    MOUSE_SCROLL,
    MOUSE_CLICK,
    MOUSE_PRESS,
    MOUSE_RELEASE,
    KEY_TAP,
    KEY_PRESS,
    KEY_RELEASE,
    KEY_RELEASE_ALL,
    MEDIA_PLAY_PAUSE,
    MEDIA_MUTE,
    MEDIA_NEXT_TRACK,
    MEDIA_PREVIOUS_TRACK,
    MEDIA_VOLUME_UP,
    MEDIA_VOLUME_DOWN,
    CLEAR_BONDS
};

struct BleCommand {
    BleCommandType type         = BleCommandType::MOUSE_MOVE;
    int16_t        dx           = 0;
    int16_t        dy           = 0;
    int8_t         scrollAmount = 0;
    uint8_t        button       = 0;
    uint8_t        key          = 0;
};

class BleHandler {
public:
    void begin();
    bool isConnected();

    void moveMouse(int16_t dx, int16_t dy);
    void mouseScroll(int8_t amount);
    void mouseClick(uint8_t button);
    void mousePress(uint8_t button);
    void mouseRelease(uint8_t button);
    bool isMouseButtonPressed(uint8_t button);

    void tapKey(uint8_t key);
    void pressKey(uint8_t key);
    void releaseKey(uint8_t key);
    void releaseAllKeys();

    void mediaPlayPause();
    void mediaMute();
    void mediaNextTrack();
    void mediaPreviousTrack();
    void mediaVolumeUp();
    void mediaVolumeDown();

    void clearBonds();

    uint32_t getDroppedCommandCount() const { return droppedCommands.load(std::memory_order_relaxed); }

private:
    BleHidDevice  hidDevice;
    QueueHandle_t commandQueue = nullptr;
    TaskHandle_t  taskHandle   = nullptr;

    std::atomic<uint8_t>  buttonMask{0};
    std::atomic<uint32_t> droppedCommands{0};

    void enqueue(const BleCommand &cmd);
    void enqueueSimple(BleCommandType type);

    static void bleTaskEntry(void *param);
    void runBleTask();
    void executeCommand(const BleCommand &cmd);
    void executeMouseMove(const BleCommand &first);
};
