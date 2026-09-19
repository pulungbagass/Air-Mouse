# Air Mouse — ESP32-S3 Super Mini

Perangkat BLE HID gabungan **Mouse + Keyboard + Media Keys** dalam satu nama
Bluetooth **"Air Mouse"**, menggunakan sensor gerak 9-axis **MPU9250** dan
4 sensor sentuh jari berbasis metode **Ground Switch** (konduksi tubuh ke
pelat jempol yang di-GND-kan).

## 1. Struktur Proyek

```
AirMouse_ESP32S3/
├── platformio.ini              # Konfigurasi board, build flags, dependensi
├── include/
│   ├── Config.h                 # Pin mapping + semua parameter waktu/sensitivitas
│   ├── AppState.h               # Enum Mode, FingerID, GestureType, struct GestureEvent
│   ├── MpuHandler.h              # Deklarasi modul sensor gerak
│   ├── TouchHandler.h            # Deklarasi state machine 4 tombol jari
│   ├── BleHandler.h              # Deklarasi wrapper BLE HID
│   └── ActionMapper.h            # Deklarasi pemetaan gesture -> aksi (Mode 1 & 2)
├── src/
│   ├── MpuHandler.cpp            # I2C + kalibrasi + konversi gyro -> delta kursor
│   ├── TouchHandler.cpp          # Debounce, tap, double-tap, hold, combo
│   ├── BleHandler.cpp            # Implementasi wrapper BleKeyboard/BleMouse
│   ├── ActionMapper.cpp          # Seluruh tabel shortcut Mode 1 & Mode 2
│   └── main.cpp                  # Inisialisasi & main loop non-blocking
└── .gitignore
```

**Alur data satu arah dan jelas:**
`TouchHandler` (state machine mentah) -> `GestureEvent` -> `ActionMapper`
(business logic) -> `BleHandler` (I/O BLE). `MpuHandler` berjalan paralel dan
hanya dikonsultasikan oleh `main.cpp` saat Mode 1 aktif.

## 2. Wiring

| Fungsi              | Pin ESP32-S3 | Keterangan                                   |
|----------------------|:------------:|-----------------------------------------------|
| MPU9250 SDA          | GPIO 8       | I2C data                                       |
| MPU9250 SCL          | GPIO 9       | I2C clock                                      |
| MPU9250 VCC          | 3V3          |                                                 |
| MPU9250 GND          | GND          |                                                 |
| Sensor Telunjuk      | GPIO 1       | `INPUT_PULLUP`, LOW = tersentuh                |
| Sensor Tengah        | GPIO 2       | `INPUT_PULLUP`, LOW = tersentuh                |
| Sensor Manis         | GPIO 3       | `INPUT_PULLUP`, LOW = tersentuh                |
| Sensor Kelingking    | GPIO 4       | `INPUT_PULLUP`, LOW = tersentuh                |
| Pelat Jempol         | GND          | Disambung LANGSUNG ke GND (bukan ke GPIO)      |

Prinsip kerja: keempat pin jari ditarik HIGH secara internal
(`INPUT_PULLUP`). Saat jempol (yang tersambung ke GND) menyentuh pelat pada
jari lain, rangkaian tertutup melalui tubuh pengguna sehingga pin jari yang
bersangkutan terbaca **LOW** — inilah yang dibaca `TouchHandler` sebagai
"jari ditekan".

## 3. Build & Upload (VSCode + PlatformIO)

1. Install ekstensi **PlatformIO IDE** di VSCode.
2. Buka folder `AirMouse_ESP32S3/` sebagai *PlatformIO Project*.
3. PlatformIO akan otomatis mengunduh `platform = espressif32` dan seluruh
   `lib_deps` (BLE Combo, NimBLE-Arduino, MPU9250) saat build pertama.
4. Hubungkan board via USB-C, lalu klik **Upload** (ikon panah kanan di
   status bar bawah) atau jalankan:
   ```
   pio run -t upload
   ```
5. Buka **Serial Monitor** (115200 baud) untuk melihat status boot,
   kalibrasi MPU9250, dan status koneksi BLE.
6. Di Windows/Android, buka pengaturan Bluetooth dan pasangkan dengan
   perangkat bernama **"Air Mouse"**.

> Jika board Anda gagal ter-flash, tahan tombol **BOOT** saat proses upload
> dimulai (khas sebagian board "Super Mini" varian tertentu).

## 4. Tuning

Semua parameter berikut ada di `include/Config.h`:

| Parameter               | Default | Fungsi                                              |
|--------------------------|:-------:|-------------------------------------------------------|
| `DEBOUNCE_MS`            | 40      | Debounce sensor sentuh                                 |
| `CHORD_WINDOW_MS`        | 80      | Toleransi tap bersamaan untuk combo                    |
| `DOUBLE_CLICK_MS`        | 300     | Jendela deteksi double click                           |
| `HOLD_DURATION_MS`       | 3000    | Ambang batas hold                                      |
| `HOLD_REPEAT_MS`         | 400     | Interval ulang aksi hold kontinu (FF/RW)               |
| `MOUSE_SENSITIVITY`      | 14.0    | Kecepatan kursor (naikkan jika terasa lambat)          |
| `GYRO_DEADZONE_DPS`      | 1.2     | Ambang noise agar kursor tidak "gemetar" saat diam     |
| `MAX_MOUSE_DELTA`        | 30      | Batas lonjakan kursor per update                       |
| `RECENTER_DURATION_MS`   | 400     | Lama sampling saat re-kalibrasi titik nol (non-blocking)|

## 5. Referensi Lengkap Shortcut

### MODE 1 — Navigasi Kursor Utama (default saat boot)

| Gesture         | Telunjuk                              | Tengah                     | Manis                  | Kelingking            |
|------------------|----------------------------------------|-----------------------------|--------------------------|--------------------------|
| 1x Tap           | Klik Kiri                              | Klik Kanan                  | Scroll Up                | Scroll Down              |
| Double Click     | Enter (buka file/folder)               | Toggle Touch Keyboard (Win+Ctrl+O) | Page Up          | Page Down                |
| Hold (3 dtk)     | Drag Lock (tap 1x lagi untuk lepas)    | Re-center MPU9250            | **Toggle ke Mode 2**     | Pause/Resume sensor gerak|

Kombinasi jari (tap bersamaan, Mode 1):

| Kombinasi                     | Aksi                          |
|--------------------------------|-------------------------------|
| Telunjuk + Tengah              | Ctrl + C                      |
| Telunjuk + Manis               | Ctrl + V                      |
| Tengah + Manis                 | Ctrl + Scroll Up (Zoom In)     |
| Tengah + Kelingking            | Ctrl + Scroll Down (Zoom Out)  |
| Telunjuk + Tengah + Manis      | Win + Tab (Task View)          |
| Telunjuk + Tengah + Kelingking | Win + D (Minimize All)         |
| Keempat jari                   | Win + L (Lock Screen)          |

### MODE 2 — Media & Produktivitas (sensor gerak MPU9250 nonaktif total)

| Gesture         | Telunjuk            | Tengah              | Manis                     | Kelingking                |
|------------------|-----------------------|-----------------------|------------------------------|------------------------------|
| 1x Tap           | Play/Pause           | Mute/Unmute            | Next Track                    | Previous Track                |
| Double Click     | F11 (Full Screen)    | Win+D (Show Desktop)   | Next Virtual Desktop (Ctrl+Win+→) | Prev Virtual Desktop (Ctrl+Win+←) |
| Hold (3 dtk)     | Fast Forward (ulang) | Rewind (ulang)         | **Toggle kembali ke Mode 1**  | Win+Tab (Task View)           |

Kombinasi jari (tap bersamaan, Mode 2):

| Kombinasi              | Aksi                        |
|--------------------------|-------------------------------|
| Telunjuk + Tengah        | Volume Up                     |
| Telunjuk + Manis         | Volume Down                   |
| Tengah + Kelingking      | Alt + F4 (Tutup Window)        |

## 6. Catatan Desain & Asumsi

- **Library BLE**: menggunakan fork `Georgegipa/ESP32-BLE-Combo` yang
  membuat SATU perangkat BLE HID gabungan Mouse+Keyboard+Media Keys.
  Menggunakan library `BLE-Mouse` dan `BLE-Keyboard` T-vK secara terpisah
  **tidak bisa** dipakai bersamaan karena masing-masing membuat server BLE
  HID sendiri dan akan bentrok saat pairing.
- **Mode NimBLE** diaktifkan (`USE_NIMBLE`) untuk menghemat RAM/Flash,
  penting karena ESP32-S3 Super Mini umumnya hanya memiliki Flash 4MB.
- **Toggle Virtual Keyboard** (double click Tengah, Mode 1) memakai
  shortcut asli Windows 10/11 `Win+Ctrl+O`. Di Android, shortcut ini tidak
  berlaku universal — silakan sesuaikan di `ActionMapper::handleMode1DoubleTap`
  jika target utama Anda adalah Android.
- **Fast Forward/Rewind** (Mode 2) diemulasikan dengan tombol Panah
  Kanan/Kiri yang diulang tiap `HOLD_REPEAT_MS` selama ditahan — ini adalah
  shortcut "skip 5-10 detik" yang didukung luas oleh YouTube, VLC, dan
  sebagian besar pemutar media, karena kode HID Consumer Control khusus
  Fast Forward/Rewind tidak tersedia di library BLE Combo yang dipakai.
- **Close Window** (Mode 2, Tengah+Kelingking) memakai `Alt+F4` (universal
  untuk semua aplikasi). Alternatif `Ctrl+W` (khusus tab/dokumen) bisa
  diaktifkan dengan mengubah satu baris di `ActionMapper::handleMode2Combo`.
- Re-center MPU9250 tidak memakai fungsi `calibrateAccelGyro()` bawaan
  library (yang blocking selama beberapa detik), melainkan sampling bias
  gyro custom yang **non-blocking** (`RECENTER_DURATION_MS`), agar tidak
  melanggar aturan "tanpa delay() yang memblokir".
