#pragma once
#include <stdint.h>

constexpr uint8_t MOUSE_LEFT   = 0x01;
constexpr uint8_t MOUSE_RIGHT  = 0x02;
constexpr uint8_t MOUSE_MIDDLE = 0x04;

constexpr uint8_t KEY_LEFT_CTRL   = 0x80;
constexpr uint8_t KEY_LEFT_SHIFT  = 0x81;
constexpr uint8_t KEY_LEFT_ALT    = 0x82;
constexpr uint8_t KEY_LEFT_GUI    = 0x83;
constexpr uint8_t KEY_RIGHT_CTRL  = 0x84;
constexpr uint8_t KEY_RIGHT_SHIFT = 0x85;
constexpr uint8_t KEY_RIGHT_ALT   = 0x86;
constexpr uint8_t KEY_RIGHT_GUI   = 0x87;

constexpr uint8_t KEY_RETURN      = 0xB0;
constexpr uint8_t KEY_TAB         = 0xB3;
constexpr uint8_t KEY_F4          = 0xC5;
constexpr uint8_t KEY_F11         = 0xCC;
constexpr uint8_t KEY_PAGE_UP     = 0xD3;
constexpr uint8_t KEY_PAGE_DOWN   = 0xD6;
constexpr uint8_t KEY_RIGHT_ARROW = 0xD7;
constexpr uint8_t KEY_LEFT_ARROW  = 0xD8;

constexpr uint16_t CONSUMER_NEXT_TRACK   = 0x00B5;
constexpr uint16_t CONSUMER_PREV_TRACK   = 0x00B6;
constexpr uint16_t CONSUMER_PLAY_PAUSE   = 0x00CD;
constexpr uint16_t CONSUMER_MUTE         = 0x00E2;
constexpr uint16_t CONSUMER_VOLUME_UP    = 0x00E9;
constexpr uint16_t CONSUMER_VOLUME_DOWN  = 0x00EA;
