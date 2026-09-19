#include "TouchHandler.h"
#include "Config.h"

namespace {
    inline uint8_t countBits(uint8_t v) {
        uint8_t c = 0;
        while (v) { c += (v & 0x01); v >>= 1; }
        return c;
    }
}

void TouchHandler::begin() {
    pins[(uint8_t)FingerID::INDEX]  = PIN_FINGER_INDEX;
    pins[(uint8_t)FingerID::MIDDLE] = PIN_FINGER_MIDDLE;
    pins[(uint8_t)FingerID::RING]   = PIN_FINGER_RING;
    pins[(uint8_t)FingerID::PINKY]  = PIN_FINGER_PINKY;

    for (uint8_t f = 0; f < NUM_FINGERS; f++) {
        pinMode(pins[f], INPUT_PULLUP);

        lastRawRead[f]        = false;
        pressed[f]            = false;
        lastDebounceTime[f]   = 0;
        pressStartTime[f]     = 0;
        lastHoldRepeatTime[f] = 0;
        holdFired[f]          = false;
        suppressRelease[f]    = false;
        clickPending[f]       = false;
        clickPendingSince[f]  = 0;
    }

    groupActive    = false;
    groupLocked    = false;
    groupStartTime = 0;
    groupMask      = 0;

    queueHead  = 0;
    queueTail  = 0;
    queueCount = 0;
}

void TouchHandler::pushEvent(GestureType type, uint8_t mask, unsigned long ts) {
    if (queueCount >= EVENT_QUEUE_SIZE) {
        // Sangat jarang terjadi (butuh >8 gesture dalam satu iterasi loop).
        // Event terbaru dibuang demi menjaga integritas buffer.
        return;
    }
    eventQueue[queueTail].type       = type;
    eventQueue[queueTail].fingerMask = mask;
    eventQueue[queueTail].timestamp  = ts;
    queueTail = (uint8_t)((queueTail + 1) % EVENT_QUEUE_SIZE);
    queueCount++;
}

bool TouchHandler::update(GestureEvent &outEvent) {
    unsigned long now = millis();

    // -------------------------------------------------------------------
    // 1) Baca & debounce tiap pin -> hasilkan event tepi tekan/lepas
    // -------------------------------------------------------------------
    for (uint8_t f = 0; f < NUM_FINGERS; f++) {
        bool raw = (digitalRead(pins[f]) == LOW); // LOW = jari menyentuh (lihat Config.h)

        if (raw != lastRawRead[f]) {
            lastDebounceTime[f] = now;
            lastRawRead[f] = raw;
        }

        if ((now - lastDebounceTime[f]) > DEBOUNCE_MS && raw != pressed[f]) {
            pressed[f] = raw;
            if (pressed[f]) onFingerPressed(f, now);
            else            onFingerReleased(f, now);
        }
    }

    // -------------------------------------------------------------------
    // 2) Tutup jendela chord jika CHORD_WINDOW_MS telah lewat
    // -------------------------------------------------------------------
    if (groupActive && !groupLocked && (now - groupStartTime) >= CHORD_WINDOW_MS) {
        finalizeGroup(now);
    }

    // -------------------------------------------------------------------
    // 3) Deteksi HOLD & HOLD_REPEAT per jari (independen dari mekanisme grup,
    //    kecuali jari tsb masih berstatus kandidat chord yang belum final,
    //    atau baru saja dikonsumsi oleh sebuah COMBO_TAP)
    // -------------------------------------------------------------------
    for (uint8_t f = 0; f < NUM_FINGERS; f++) {
        if (!pressed[f] || suppressRelease[f]) continue;

        bool stillCollecting = groupActive && !groupLocked && (groupMask & (1u << f));
        if (stillCollecting) continue;

        if (!holdFired[f]) {
            if ((now - pressStartTime[f]) >= HOLD_DURATION_MS) {
                holdFired[f] = true;
                lastHoldRepeatTime[f] = now;
                pushEvent(GestureType::HOLD_TRIGGERED, (uint8_t)(1u << f), now);
            }
        } else {
            if ((now - lastHoldRepeatTime[f]) >= HOLD_REPEAT_MS) {
                lastHoldRepeatTime[f] = now;
                pushEvent(GestureType::HOLD_REPEAT, (uint8_t)(1u << f), now);
            }
        }
    }

    // -------------------------------------------------------------------
    // 4) Jendela double-click yang kadaluarsa -> resolusi menjadi SINGLE_TAP
    // -------------------------------------------------------------------
    for (uint8_t f = 0; f < NUM_FINGERS; f++) {
        if (clickPending[f] && (now - clickPendingSince[f]) >= DOUBLE_CLICK_MS) {
            clickPending[f] = false;
            pushEvent(GestureType::SINGLE_TAP, (uint8_t)(1u << f), now);
        }
    }

    // -------------------------------------------------------------------
    // 5) Keluarkan satu event dari antrian, jika ada
    // -------------------------------------------------------------------
    if (queueCount == 0) return false;

    outEvent = eventQueue[queueHead];
    queueHead = (uint8_t)((queueHead + 1) % EVENT_QUEUE_SIZE);
    queueCount--;
    return true;
}

void TouchHandler::onFingerPressed(uint8_t f, unsigned long now) {
    pressStartTime[f]  = now;
    holdFired[f]       = false;
    suppressRelease[f] = false;

    // Apakah ini tap ke-2 yang melengkapi sebuah double-click yang ditunggu?
    if (clickPending[f] && (now - clickPendingSince[f]) < DOUBLE_CLICK_MS) {
        clickPending[f] = false;
        pushEvent(GestureType::DOUBLE_TAP, (uint8_t)(1u << f), now);
        return; // Tekanan ini MELENGKAPI double-click, bukan awal chord baru.
    }

    if (!groupActive) {
        groupActive    = true;
        groupLocked    = false;
        groupStartTime = now;
        groupMask      = (uint8_t)(1u << f);
    } else if (!groupLocked) {
        groupMask |= (uint8_t)(1u << f);
    } else {
        // Grup sebelumnya sudah final (terkunci); ini adalah tekanan baru
        // yang independen -> mulai jendela chord baru.
        groupActive    = true;
        groupLocked    = false;
        groupStartTime = now;
        groupMask      = (uint8_t)(1u << f);
    }
}

void TouchHandler::onFingerReleased(uint8_t f, unsigned long now) {
    if (holdFired[f]) {
        holdFired[f] = false;
        return; // Aksi hold sudah dieksekusi sebelumnya; lepas tidak memicu tap.
    }

    if (suppressRelease[f]) {
        suppressRelease[f] = false;
        return; // Jari ini sudah "terpakai" oleh COMBO_TAP; abaikan saat lepas.
    }

    if (groupActive && !groupLocked) {
        // Jari ini lepas dengan cepat SEBELUM jendela chord selesai -> keluarkan
        // dari kandidat combo, ia akan diproses sebagai tap tunggal miliknya sendiri.
        groupMask &= (uint8_t)~(1u << f);
    }

    clickPending[f]      = true;
    clickPendingSince[f] = now;
}

void TouchHandler::finalizeGroup(unsigned long now) {
    groupLocked = true;

    uint8_t stillPressedMask = 0;
    for (uint8_t f = 0; f < NUM_FINGERS; f++) {
        if (pressed[f]) stillPressedMask |= (uint8_t)(1u << f);
    }

    // Hanya jari yang MASIH ditekan hingga akhir jendela yang dihitung sebagai
    // anggota combo (jari yang sudah lepas duluan sudah dikeluarkan di
    // onFingerReleased dan diproses sebagai tap tersendiri).
    uint8_t finalMask = groupMask & stillPressedMask;

    if (countBits(finalMask) > 1) {
        pushEvent(GestureType::COMBO_TAP, finalMask, now);
        for (uint8_t f = 0; f < NUM_FINGERS; f++) {
            if (finalMask & (1u << f)) suppressRelease[f] = true;
        }
    }
    // Jika finalMask hanya berisi 0 atau 1 jari, tidak ada tindakan tambahan -
    // logika tap/hold jari tunggal yang normal sudah berjalan secara independen.

    groupActive = false;
}
