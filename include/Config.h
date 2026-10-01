#pragma once

#define BLE_DEVICE_NAME              "Air Mouse"
#define BLE_MANUFACTURER_NAME        "AirMouse"
#define BLE_APPEARANCE_MOUSE         0x03C2
#define BLE_PNP_VENDOR_ID_SOURCE     0x02
#define BLE_PNP_VENDOR_ID            0x303A
#define BLE_PNP_PRODUCT_ID           0x8001
#define BLE_PNP_VERSION              0x0100
#define BLE_PROFILE_REVISION         2
#define BLE_CONN_INTERVAL_MIN        6      // 1.25 ms
#define BLE_CONN_INTERVAL_MAX        12     // 1.25 ms
#define BLE_CONN_LATENCY             0
#define BLE_CONN_TIMEOUT             400    // 10 ms
#define BLE_ADV_INTERVAL_MIN         32     // 0.625 ms
#define BLE_ADV_INTERVAL_MAX         48     // 0.625 ms
#define BLE_KEY_REPORT_GAP_MS        8
#define BLE_CLICK_GAP_MS             12
#define BLE_MOUSE_SPLIT_GAP_MS       1

#define PIN_MPU_SDA                  8
#define PIN_MPU_SCL                  9
#define MPU_ADDR_PRIMARY             0x68
#define MPU_ADDR_SECONDARY           0x69
#define I2C_CLOCK_SAFE_HZ            100000
#define I2C_CLOCK_FAST_HZ            400000
#define I2C_TIMEOUT_MS               20
#define I2C_POWER_SETTLE_MS          150
#define MPU_RETRY_INTERVAL_MS        2000
#define MPU_READ_FAIL_LIMIT          12
#define MPU_STALE_SAMPLE_LIMIT       150
#define MPU_ACCEPT_UNKNOWN_WHOAMI    1

#define PIN_FINGER_INDEX             1
#define PIN_FINGER_MIDDLE            2
#define PIN_FINGER_RING              3
#define PIN_FINGER_PINKY             4

#define DEBOUNCE_MS                  40UL
#define CHORD_WINDOW_MS              80UL
#define DOUBLE_CLICK_MS              300UL
#define HOLD_DURATION_MS             3000UL
#define HOLD_REPEAT_MS               400UL

#define AXIS_PLUS_X                  0
#define AXIS_MINUS_X                 1
#define AXIS_PLUS_Y                  2
#define AXIS_MINUS_Y                 3
#define AXIS_PLUS_Z                  4
#define AXIS_MINUS_Z                 5

#define MPU_AXIS_FORWARD             AXIS_PLUS_X
#define MPU_AXIS_UP                  AXIS_PLUS_Z

#define CURSOR_INVERT_X              0
#define CURSOR_INVERT_Y              0
#define MOUSE_SENSITIVITY            24.0f  // px per degree
#define MOUSE_Y_RATIO                1.0f
#define GYRO_DEADZONE_DPS            1.5f
#define CURSOR_ACCEL_START_DPS       30.0f
#define CURSOR_ACCEL_FULL_DPS        200.0f
#define CURSOR_ACCEL_MAX_GAIN        1.8f
#define CURSOR_SMOOTH_TAU_SLOW_MS    40.0f
#define CURSOR_SMOOTH_TAU_FAST_MS    4.0f
#define CURSOR_SMOOTH_LOW_DPS        8.0f
#define CURSOR_SMOOTH_HIGH_DPS       100.0f
#define CURSOR_MAX_SPEED_PX_S        7000.0f
#define CURSOR_REPORT_INTERVAL_MS    8

#define TILT_ACCEL_TAU_S             0.30f
#define TILT_ACCEL_MIN_G             0.85f
#define TILT_ACCEL_MAX_G             1.15f
#define POINT_WORLD_BLEND_FULL       0.50f
#define POINT_WORLD_BLEND_ZERO       0.20f

#define GYRO_CALIB_DURATION_MS       1200UL
#define GYRO_CALIB_MAX_STD_DPS       1.5f
#define GYRO_CALIB_MAX_ATTEMPTS      5
#define RECENTER_DURATION_MS         600UL
#define GYRO_STATIONARY_ENTER_DPS    1.6f
#define GYRO_STATIONARY_EXIT_DPS     3.0f
#define GYRO_STATIONARY_HOLD_MS      300
#define GYRO_BIAS_TRACK_TAU_S        1.5f
#define TOUCH_MOTION_FREEZE_MS       150UL

#define ENABLE_DEBUG_CONSOLE         1
#define STATUS_HEARTBEAT_MS          5000UL
#define TELEMETRY_INTERVAL_MS        100UL

#define TASK_CORE_BLE                0
#define TASK_CORE_SENSOR             1
#define TASK_PRIORITY_BLE            2
#define TASK_PRIORITY_SENSOR         2
#define TASK_STACK_SIZE_BLE          6144
#define TASK_STACK_SIZE_SENSOR       6144
#define BLE_COMMAND_QUEUE_LENGTH     64
#define BLE_TASK_QUEUE_WAIT_MS       20UL
#define SENSOR_TASK_LOOP_DELAY_MS    4UL
