#include "BleHandler.h"
#include "Config.h"
#include "RtosUtil.h"

void BleHandler::begin() {
    commandQueue = xQueueCreate(BLE_COMMAND_QUEUE_LENGTH, sizeof(BleCommand));

    xTaskCreatePinnedToCore(
        bleTaskEntry,
        "BLE_Core0_Task",
        TASK_STACK_SIZE_BLE,
        this,
        TASK_PRIORITY_BLE,
        &taskHandle,
        TASK_CORE_BLE);
}

void BleHandler::bleTaskEntry(void *param) {
    static_cast<BleHandler *>(param)->runBleTask();
}

void BleHandler::runBleTask() {
    hidDevice.begin();
    Serial.println(F("[BleHandler] Task BLE HID aktif di Core 0 (profil: Mouse + Keyboard + Consumer)."));

    BleCommand cmd;
    for (;;) {
        if (xQueueReceive(commandQueue, &cmd, msToTicks(BLE_TASK_QUEUE_WAIT_MS)) == pdTRUE) {
            hidDevice.serviceHousekeeping();
            if (hidDevice.isConnected() || cmd.type == BleCommandType::CLEAR_BONDS) {
                executeCommand(cmd);
            }
        } else {
            hidDevice.serviceHousekeeping();
        }
    }
}

bool BleHandler::isConnected() {
    return hidDevice.isConnected();
}

void BleHandler::enqueue(const BleCommand &cmd) {
    if (commandQueue == nullptr) return;
    if (!hidDevice.isConnected() && cmd.type != BleCommandType::CLEAR_BONDS) return;
    if (xQueueSend(commandQueue, &cmd, 0) != pdTRUE) {
        droppedCommands.fetch_add(1, std::memory_order_relaxed);
    }
}

void BleHandler::enqueueSimple(BleCommandType type) {
    BleCommand cmd;
    cmd.type = type;
    enqueue(cmd);
}

void BleHandler::executeMouseMove(const BleCommand &first) {
    int32_t dx = first.dx;
    int32_t dy = first.dy;
    BleCommand next;
    while (xQueuePeek(commandQueue, &next, 0) == pdTRUE && next.type == BleCommandType::MOUSE_MOVE) {
        xQueueReceive(commandQueue, &next, 0);
        dx += next.dx;
        dy += next.dy;
    }
    hidDevice.mouseMove(dx, dy);
}

void BleHandler::executeCommand(const BleCommand &cmd) {
    switch (cmd.type) {
        case BleCommandType::MOUSE_MOVE:
            executeMouseMove(cmd);
            break;
        case BleCommandType::MOUSE_SCROLL:
            hidDevice.mouseMove(0, 0, cmd.scrollAmount);
            break;
        case BleCommandType::MOUSE_CLICK:
            hidDevice.mouseClick(cmd.button);
            break;
        case BleCommandType::MOUSE_PRESS:
            hidDevice.mousePress(cmd.button);
            buttonMask.fetch_or(cmd.button, std::memory_order_relaxed);
            break;
        case BleCommandType::MOUSE_RELEASE:
            hidDevice.mouseRelease(cmd.button);
            buttonMask.fetch_and(static_cast<uint8_t>(~cmd.button), std::memory_order_relaxed);
            break;
        case BleCommandType::KEY_TAP:
            hidDevice.keyWrite(cmd.key);
            break;
        case BleCommandType::KEY_PRESS:
            hidDevice.keyPress(cmd.key);
            break;
        case BleCommandType::KEY_RELEASE:
            hidDevice.keyRelease(cmd.key);
            break;
        case BleCommandType::KEY_RELEASE_ALL:
            hidDevice.keyReleaseAll();
            break;
        case BleCommandType::MEDIA_PLAY_PAUSE:     hidDevice.consumerTap(CONSUMER_PLAY_PAUSE);  break;
        case BleCommandType::MEDIA_MUTE:           hidDevice.consumerTap(CONSUMER_MUTE);        break;
        case BleCommandType::MEDIA_NEXT_TRACK:     hidDevice.consumerTap(CONSUMER_NEXT_TRACK);  break;
        case BleCommandType::MEDIA_PREVIOUS_TRACK: hidDevice.consumerTap(CONSUMER_PREV_TRACK);  break;
        case BleCommandType::MEDIA_VOLUME_UP:      hidDevice.consumerTap(CONSUMER_VOLUME_UP);   break;
        case BleCommandType::MEDIA_VOLUME_DOWN:    hidDevice.consumerTap(CONSUMER_VOLUME_DOWN); break;
        case BleCommandType::CLEAR_BONDS:
            hidDevice.clearBonds();
            break;
    }
}

void BleHandler::moveMouse(int16_t dx, int16_t dy) {
    BleCommand cmd;
    cmd.type = BleCommandType::MOUSE_MOVE;
    cmd.dx = dx;
    cmd.dy = dy;
    enqueue(cmd);
}

void BleHandler::mouseScroll(int8_t amount) {
    BleCommand cmd;
    cmd.type = BleCommandType::MOUSE_SCROLL;
    cmd.scrollAmount = amount;
    enqueue(cmd);
}

void BleHandler::mouseClick(uint8_t button) {
    BleCommand cmd;
    cmd.type = BleCommandType::MOUSE_CLICK;
    cmd.button = button;
    enqueue(cmd);
}

void BleHandler::mousePress(uint8_t button) {
    BleCommand cmd;
    cmd.type = BleCommandType::MOUSE_PRESS;
    cmd.button = button;
    enqueue(cmd);
}

void BleHandler::mouseRelease(uint8_t button) {
    BleCommand cmd;
    cmd.type = BleCommandType::MOUSE_RELEASE;
    cmd.button = button;
    enqueue(cmd);
}

bool BleHandler::isMouseButtonPressed(uint8_t button) {
    return (buttonMask.load(std::memory_order_relaxed) & button) != 0;
}

void BleHandler::tapKey(uint8_t key) {
    BleCommand cmd;
    cmd.type = BleCommandType::KEY_TAP;
    cmd.key = key;
    enqueue(cmd);
}

void BleHandler::pressKey(uint8_t key) {
    BleCommand cmd;
    cmd.type = BleCommandType::KEY_PRESS;
    cmd.key = key;
    enqueue(cmd);
}

void BleHandler::releaseKey(uint8_t key) {
    BleCommand cmd;
    cmd.type = BleCommandType::KEY_RELEASE;
    cmd.key = key;
    enqueue(cmd);
}

void BleHandler::releaseAllKeys()        { enqueueSimple(BleCommandType::KEY_RELEASE_ALL); }
void BleHandler::mediaPlayPause()        { enqueueSimple(BleCommandType::MEDIA_PLAY_PAUSE); }
void BleHandler::mediaMute()             { enqueueSimple(BleCommandType::MEDIA_MUTE); }
void BleHandler::mediaNextTrack()        { enqueueSimple(BleCommandType::MEDIA_NEXT_TRACK); }
void BleHandler::mediaPreviousTrack()    { enqueueSimple(BleCommandType::MEDIA_PREVIOUS_TRACK); }
void BleHandler::mediaVolumeUp()         { enqueueSimple(BleCommandType::MEDIA_VOLUME_UP); }
void BleHandler::mediaVolumeDown()       { enqueueSimple(BleCommandType::MEDIA_VOLUME_DOWN); }
void BleHandler::clearBonds()            { enqueueSimple(BleCommandType::CLEAR_BONDS); }
