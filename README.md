# Air Mouse v2 — ESP32-S3 Super Mini (Dual-Core FreeRTOS)

Perangkat BLE HID: **Mouse sebagai profil utama**, Keyboard/Media Key hanya dipakai di latar belakang untuk mengirim shortcut saat sensor jari disentuh.

Revisi ini memperbaiki dua masalah:

1. MPU9250 tidak merespons → I2C dan driver sensor ditulis ulang (bus recovery, scanner, retry otomatis, driver register-level) + algoritma gerak baru.
2. Ikon Keyboard di Windows/Android → `Appearance` dan urutan `HID Report Map` diperbaiki.

> **Status verifikasi.** Build PlatformIO penuh dan pengujian di board/host BLE **tidak bisa** dilakukan di lingkungan tempat kode ini ditulis. Yang sudah diverifikasi: (a) seluruh sumber lolos cek sintaks C++11 terhadap stub API Arduino/FreeRTOS/NimBLE yang meniru signature asli NimBLE-Arduino 1.4.3; (b) `MotionEngine` diuji numerik di host (15 skenario); (c) driver + state machine MPU diuji dengan simulator register MPU9250 (10 skenario, lihat bagian 2.5). Perilaku di hardware nyata (khususnya ikon di Windows/Android) perlu Anda konfirmasi; langkah verifikasinya ada di bagian 3.4.

---

## 1. Perubahan Dependensi

| Sebelum | Sesudah | Alasan |
|---|---|---|
| `Georgegipa/ESP32-BLE-Combo` | dihapus | Mengunci `setAppearance(HID_KEYBOARD)` dan menaruh koleksi Keyboard pertama di Report Map |
| `hideakitai/MPU9250` | dihapus | `setup()` gagal total jika WHO_AM_I di luar {0x71, 0x73, 0x70} **atau** AK8963 tidak terbaca, tanpa pesan (verbose mati) dan tanpa retry |
| `NimBLE-Arduino ^1.4.1` | `NimBLE-Arduino @ 1.4.3` | Satu-satunya dependensi; HID GATT ditulis langsung di atasnya |

---

## 2. Perbaikan Pembacaan I2C MPU9250

### 2.1 Akar masalah (dari analisis kode lama)

Saya tidak bisa memastikan penyebab pada board Anda, tetapi kode lama memiliki beberapa titik gagal yang semuanya berakhir "diam-diam tidak ada respons":

1. **WHO_AM_I terlalu ketat.** Modul "MPU9250" murah sering berisi MPU6500/MPU9255/klon MPU6050 dengan ID berbeda.
2. **Magnetometer dianggap wajib.** Bila AK8963 tidak terbaca, `setup()` mengembalikan `false` sehingga gyro juga tidak dipakai. Padahal air mouse tidak butuh magnetometer.
3. **Satu kali percobaan, tanpa log alasan.** Jika sensor telat mendapat daya atau bus sempat glitch saat boot, sensor tetap mati sampai reset manual.
4. **Tidak ada bus recovery.** Reset ESP32 di tengah transaksi I2C dapat membuat slave menahan SDA LOW.
5. **Tidak ada timeout Wire dan tidak ada fallback kecepatan.** 400 kHz langsung dipakai, meski pull-up/kabel belum tentu mendukung.

### 2.2 Urutan inisialisasi baru (`MpuHandler` + `I2cBus` + `Mpu9250Driver`)

1. Tunggu `I2C_POWER_SETTLE_MS` (150 ms) agar modul stabil.
2. **Inspeksi jalur**: baca level SDA/SCL. Jika tertahan LOW, dicatat beserta petunjuk (VCC, short, pull-up).
3. **Bus recovery**: sampai 9 pulsa clock pada SCL selama SDA LOW, lalu kondisi STOP.
4. `Wire.begin(8, 9)` pada **100 kHz** dengan `setTimeOut(20 ms)`.
5. **I2C scan** 0x08–0x77 (dicetak pada percobaan pertama dan tiap 5 percobaan). Alamat 0x68 dan 0x69 (AD0) dicoba.
6. **Driver register-level**: baca WHO_AM_I → reset perangkat → tunggu bit reset bersih → wake dengan clock PLL → konfigurasi (200 Hz, DLPF 41 Hz, ±1000 dps, ±4 g). Setiap register kritis **ditulis lalu dibaca ulang**. Terakhir, 4 sampel uji divalidasi (bukan semuanya 0/0xFF, magnitudo akselerometer wajar).
7. Magnetometer hanya **dideteksi untuk diagnosis** (mengaktifkan bypass, cek 0x0C) dan tidak pernah menggagalkan inisialisasi.
8. Clock dinaikkan ke 400 kHz dan diuji 10 kali baca; bila gagal kembali ke 100 kHz.
9. Kalibrasi bias gyro (lihat 3.3).

**Retry non-blocking.** Bila gagal, `MpuHandler` tetap `SEARCHING` dan mencoba lagi pada 250 ms → 500 ms → 1 s → lalu tiap 2 s. Jari (GPIO 1–4) dan BLE tetap berjalan selama sensor belum ada.

**Auto-recovery saat runtime.** Recovery penuh (reset bus + init ulang) dipicu oleh:
- 12 pembacaan gagal berturut-turut, atau
- 150 sampel mentah identik berturut-turut (sensor hang/beku).

Setelah recovery pertama, clock dikunci 100 kHz.

**Chip yang diterima:** WHO_AM_I 0x71 (MPU-9250), 0x73 (MPU-9255), 0x70 (MPU-6500), 0x68 (MPU-6050/klon). ID lain diterima dengan flag `MPU_ACCEPT_UNKNOWN_WHOAMI` (nilai 0x00/0xFF dan ICM-20948/0xEA ditolak dengan alasan jelas).

### 2.3 Membaca log serial untuk diagnosis

Contoh boot normal:

```
[I2C] Percobaan #1 | sebelum: SDA/SCL HIGH (normal) | recovery: bus bebas | sesudah: SDA/SCL HIGH (normal)
[I2C] Scan SDA=8 SCL=9: 2 perangkat 0x0C 0x68
[MPU] Terdeteksi @0x68 | WHO_AM_I=0x71 | MPU-9250 | magnetometer AK8963: ada
[MPU] Kalibrasi gyro: diamkan perangkat.
[MPU] Siap. Bias gyro = 0.412, -0.180, 0.095 dps | I2C 400000 Hz
```

| Pesan | Artinya | Tindakan |
|---|---|---|
| `SDA/SCL tertahan LOW` setelah recovery | Short ke GND, modul tanpa VCC, atau tanpa pull-up | Cek 3V3/GND; tambahkan pull-up 4.7 kΩ ke 3V3 bila modul tidak punya |
| `Scan ...: 0 perangkat` | Tidak ada slave yang menjawab | Cek SDA=GPIO 8, SCL=GPIO 9 (jangan tertukar), solder header, tegangan **3.3 V** |
| Scan menampilkan alamat lain, bukan 0x68/0x69 | Modul lain / alamat berbeda | Sesuaikan `MPU_ADDR_*` |
| `WHO_AM_I=0x00/0xFF` | Bus/daya bermasalah | Periksa kabel dan VCC |
| `ICM-20948 ... tidak didukung` | Modul bukan keluarga MPU | Gunakan MPU9250/6500/6050 |
| `Perangkat bergerak saat kalibrasi` | Sensor digerakkan saat boot | Diamkan 1–2 detik; setelah 5 kali, sampel terbaik dipakai |

Perintah serial tambahan (Serial Monitor 115200): `u` scan I2C, `t` re-init MPU, `y` re-center, `o` status MPU, `g` telemetri IMU on/off, `B` hapus semua bond BLE, `h` bantuan.

### 2.4 Kompromi yang perlu diketahui

- Satu percobaan inisialisasi yang sensornya ada tetapi gagal di tengah jalan dapat menahan polling jari hingga ±250 ms (reset chip + jeda). Saat sensor **tidak ada**, satu percobaan hanya ±20 ms.
- Magnetometer tidak dipakai untuk kursor (alasan di 3.1).

### 2.5 Yang diuji pada simulator register

| Skenario | Hasil |
|---|---|
| MPU9250 asli (0x71) + AK8963, yaw 60 dps | RUNNING, kursor bergerak ke kiri |
| MPU6500 (0x70) tanpa magnetometer | RUNNING (pada library lama: gagal) |
| Klon WHO_AM_I=0x68 dan ID tak dikenal (0x98) | RUNNING |
| Sensor absen saat boot lalu muncul | SEARCHING → RUNNING otomatis |
| SDA tertahan LOW saat boot | Dibebaskan oleh pulsa clock, lalu RUNNING |
| Gagal baca di tengah operasi | Recovery → RUNNING otomatis |
| Data sensor beku | Terdeteksi, keluar dari RUNNING dan re-init |
| Bias gyro 2 dps saat diam 20 detik | Kursor tidak bergerak (0 px) |
| Sentuh jari (freeze) | Tidak ada output kursor |

Simulator tidak menggantikan uji hardware: ia membuktikan logika, bukan kualitas kabel/solder/pull-up.

---

## 3. Algoritma Gerak Tangan → Kursor

### 3.1 Prinsip

Kursor digerakkan oleh **kecepatan tangensial vektor penunjuk** (sumbu "maju" tangan), bukan oleh sumbu gyro mentah:

```
fdot  = ω × f                 (f = sumbu maju tangan di frame sensor, ω = gyro terkoreksi bias)
kiri  = fdot · (up × f)/|up × f|        (arah horizontal lokal)
atas  = fdot · meridian                  (meridian = komponen up yang tegak lurus f)
dx = -kiri,  dy = -atas   (× sensitivitas)
```

`up` adalah vektor "atas" dunia di frame sensor, diestimasi dengan **filter komplementer**: dipropagasi memakai gyro (rotasi Rodrigues) dan dikoreksi pelan oleh akselerometer (τ = 0,3 s) hanya saat |a| ≈ 1 g (0,85–1,15 g). Karena itu gravitasi tetap terlacak saat gerakan cepat.

Konsekuensinya, ketiga sumbu rotasi otomatis ditangani:

| Gerakan tangan | Perilaku |
|---|---|
| **Yaw** (tangan menoleh kiri/kanan) | Kursor ke kiri/kanan |
| **Pitch** (ujung tangan naik/turun) | Kursor ke atas/bawah |
| **Roll** (memutar pergelangan di sekitar arah tunjuk) | **Diabaikan** (tidak menggerakkan kursor) |
| Tangan sedang miring/diputar 90° | Yaw dan pitch tetap benar (kompensasi roll dari gravitasi) |
| Menunjuk naik 45° lalu roll | Tetap tidak ada gerakan kursor (terbukti di uji numerik) |
| Menunjuk hampir vertikal (> ±60–78°) | Blending mulus ke pemetaan sumbu-perangkat agar tidak noisy |

Magnetometer sengaja **tidak dipakai**: mouse relatif tidak butuh arah absolut, dan medan magnet dalam ruangan menimbulkan lompatan kursor.

### 3.2 Pipeline per sampel (≈200 Hz)

1. Kurangi bias gyro 3 sumbu.
2. Deteksi diam (|ω| < 1,6 dps selama 300 ms; keluar bila > 3,0 dps). Saat diam: **bias dilacak pelan** (τ = 1,5 s) sehingga drift suhu dikoreksi otomatis, dan output kursor dipaksa nol.
3. Propagasi + koreksi vektor gravitasi.
4. Proyeksi kecepatan tangensial (rumus di atas).
5. **Deadzone radial lembut** (1,5 dps): nilai dikurangi ambang, bukan dipotong, sehingga tidak ada lompatan saat keluar dari deadzone.
6. **Smoothing adaptif kecepatan**: τ 40 ms untuk gerakan lambat (menekan jitter) sampai τ 4 ms untuk gerakan cepat (latensi rendah).
7. **Akselerasi pointer**: gain 1,0× (pelan, presisi) sampai 1,8× (cepat, jangkauan).
8. Konversi ke piksel dengan **akumulator sub-piksel**. Gerakan lambat menjadi langkah 1 px yang halus. Kode lama memotong desimal tiap sampel sehingga gerakan lambat menjadi 0 px lalu tersendat.
9. Laporan kursor digabung dan dikirim tiap 8 ms (≈125 Hz) agar tidak membanjiri BLE.

### 3.3 Anti-drift

- **Kalibrasi awal** 1,2 detik dengan pengecekan stabilitas (sigma gyro ≤ 1,5 dps). Bila perangkat bergerak, diulang sampai 5 kali, lalu sampel terbaik dipakai.
- **Re-center** (tahan Tengah 3 detik, Mode 1) memakai rutinitas yang sama tetapi ditolak bila perangkat bergerak.
- **Freeze saat sentuh jari**: kursor dibekukan 150 ms sejak perubahan mentah pin jari (sebelum debounce), sehingga goyangan saat menyentuhkan jari tidak menggeser kursor sebelum klik.
- Bias 2 dps saat diam selama 20 detik menghasilkan **0 px** pergeseran pada uji.

### 3.4 Penyesuaian orientasi pemasangan (wajib dicek sekali)

Asumsi bawaan: **sumbu X chip mengarah ke ujung jari (maju), sumbu Z chip mengarah ke atas (punggung tangan)**. Bila modul Anda terpasang berbeda:

1. Aktifkan telemetri: ketik `g` di Serial Monitor.
2. Tangan pada pose netral (telapak ke bawah). Kolom `a=` menunjukkan sumbu yang ≈ +1 g: itulah sumbu "atas". Set `MPU_AXIS_UP` (`AXIS_PLUS_Z`, `AXIS_MINUS_Y`, dst.).
3. Putar pergelangan di sekitar arah tunjuk. Kolom `g=` yang bereaksi kuat adalah sumbu "maju". Set `MPU_AXIS_FORWARD` (tidak boleh sumbu yang sama dengan atas).
4. Uji: tolehkan tangan ke kanan → kursor harus ke kanan; angkat ujung tangan → kursor naik. Jika arah terbalik, atur `CURSOR_INVERT_X` / `CURSOR_INVERT_Y` di `Config.h`.

Konfigurasi sumbu yang tidak ortogonal dideteksi saat boot dan dikembalikan ke +X/+Z dengan peringatan.

---

## 4. Perbaikan Ikon BLE (Keyboard → Mouse)

### 4.1 Penyebab (dari kode library lama)

Di `BleCombo.cpp` (library Georgegipa):
- `advertising->setAppearance(HID_KEYBOARD)` menetapkan Appearance `0x03C1` (Keyboard) secara hardcode.
- Koleksi **Keyboard adalah application collection pertama** di Report Map (Report ID 1), sedangkan Mouse baru ID 3.
- PnP ID memakai Vendor ID Apple (0x05AC).
- `setSecurityAuth(true, true, true)` meminta MITM padahal perangkat tanpa input/output, yang berisiko membuat pairing tidak stabil di sebagian host.

Ada satu hal lagi di NimBLE-Arduino 1.4.3: `NimBLEAdvertising::setAppearance()` hanya mengisi **paket advertising**. Karakteristik GAP **Appearance (0x2A01)** yang dibaca host setelah terhubung adalah nilai terpisah dan tidak diisi oleh fungsi itu.

### 4.2 Perbaikan (`BleHidDevice.cpp`)

1. **Appearance `0x03C2` (HID Mouse) di dua tempat**: paket advertising (`advertising->setAppearance`) dan karakteristik GAP 0x2A01 (`ble_svc_gap_device_appearance_set`, dipanggil setelah `NimBLEDevice::init`).
2. **Report Map dengan Mouse pertama** (Report ID 1: 5 tombol, X, Y, wheel, AC Pan), lalu Keyboard (ID 2), lalu Consumer/Media (ID 3).
3. PnP ID memakai Vendor ID Espressif (0x303A) dan PID placeholder 0x8001; ubah di `Config.h` bila punya ID sendiri.
4. Keamanan: bonding + Secure Connections, **tanpa MITM** (Just Works), IO capability `NO_INPUT_OUTPUT`.
5. Parameter koneksi diminta ke host: interval 7,5–15 ms, latency 0, timeout 4 s (kursor lebih responsif).
6. **Revisi profil** (`BLE_PROFILE_REVISION`): pada boot pertama setelah firmware ini, bond lama di NVS ESP32 dihapus otomatis.

### 4.3 Wajib: hapus cache pairing di host

Windows dan Android menyimpan (cache) Appearance, Report Map, dan ikon per alamat Bluetooth. Firmware baru **tidak akan mengubah ikon** bila perangkat lama tidak dilepas:

- **Windows:** Settings → Bluetooth & devices → "Air Mouse" → *Remove device*, matikan lalu nyalakan Bluetooth, lalu pairing ulang.
- **Android:** Bluetooth → "Air Mouse" → *Forget/Unpair*, matikan lalu nyalakan Bluetooth, lalu pairing ulang.
- Sisi ESP32 dibersihkan otomatis (poin 6). Manual: kirim `B` di Serial Monitor.

### 4.4 Cara memverifikasi

Pada daftar perangkat, "Air Mouse" harus muncul sebagai **Mouse**. Log serial menampilkan `Task BLE HID aktif di Core 0 (profil: Mouse + Keyboard + Consumer)`. Bila ikon masih Keyboard setelah unpair penuh, kirimkan versi OS/perangkat host; itu bagian yang tidak bisa saya uji dari sini.

---

## 5. Arsitektur Dual-Core (dipertahankan)

| Task | Core | Isi | Jeda |
|---|:-:|---|---|
| `BLE_Core0_Task` | 0 | Satu-satunya pemilik stack NimBLE HID. Mengonsumsi `BleCommand` dari FreeRTOS Queue (64 slot) dan memakai `xQueueReceive` timeout 20 ms | Blocking pada queue (0 % CPU saat idle) + `vTaskDelay` di setiap laporan keyboard/consumer |
| `Sensor_Core1_Task` | 1 | Polling I2C MPU9250, pin jari GPIO 1–4 (`INPUT_PULLUP`), logika `millis()` tap/double/hold/combo, `ActionMapper` | `vTaskDelay(4 ms)` di setiap iterasi |

Perubahan pendukung:
- `msToTicks()` menjamin jeda minimal 1 tick, sehingga `vTaskDelay` tidak pernah menjadi 0 (busy-loop) walau tick rate berubah.
- Perintah `MOUSE_MOVE` beruntun di queue **digabung** oleh task Core 0 (urutan terhadap klik/tombol tetap terjaga) dan dipecah otomatis bila > 127 per laporan.
- Perintah tidak diantrekan saat BLE belum terhubung (counter *dropped* kini hanya menghitung queue penuh yang sebenarnya).
- Stack kedua task dinaikkan ke 6144 byte (NimBLE init dan `Serial.printf`).
- `Serial.setTxTimeoutMs(0)`: log tidak menahan task bila tidak ada terminal USB yang terbuka.

---

## 6. Referensi Lengkap Shortcut

### Mode 1: Navigasi (default saat boot)

| Gesture | Telunjuk (GPIO 1) | Tengah (GPIO 2) | Manis (GPIO 3) | Kelingking (GPIO 4) |
|---|---|---|---|---|
| 1× Tap | Klik kiri | Klik kanan | Scroll up | Scroll down |
| Double tap | Enter | Toggle Touch Keyboard (Win+Ctrl+O) | Page Up | Page Down |
| Hold 3 dtk | Drag lock (tap 1× untuk lepas) | Re-center MPU | **Pindah ke Mode 2** | Pause/resume sensor gerak |

| Kombinasi (tap bersamaan ≤ 80 ms) | Aksi |
|---|---|
| Telunjuk + Tengah | Ctrl + C |
| Telunjuk + Manis | Ctrl + V |
| Tengah + Manis | Ctrl + Scroll Up (zoom in) |
| Tengah + Kelingking | Ctrl + Scroll Down (zoom out) |
| Telunjuk + Tengah + Manis | Win + Tab |
| Telunjuk + Tengah + Kelingking | Win + D |
| Keempat jari | Win + L |

### Mode 2: Media & Produktivitas (sensor gerak tidak dipoll)

| Gesture | Telunjuk | Tengah | Manis | Kelingking |
|---|---|---|---|---|
| 1× Tap | Play/Pause | Mute | Next track | Previous track |
| Double tap | F11 | Win + D | Ctrl+Win+→ (desktop berikutnya) | Ctrl+Win+← (desktop sebelumnya) |
| Hold 3 dtk | Fast forward (→ berulang tiap 400 ms) | Rewind (← berulang tiap 400 ms) | **Kembali ke Mode 1** | Win + Tab (sekali) |

| Kombinasi | Aksi |
|---|---|
| Telunjuk + Tengah | Volume up |
| Telunjuk + Manis | Volume down |
| Tengah + Kelingking | Alt + F4 |

Catatan:
- **Bug diperbaiki:** pada kode lama, double tap memicu aksi double **lalu** 300 ms kemudian memicu single tap (mis. Enter diikuti klik kiri), karena lepas jari ke-2 mengaktifkan ulang tap tertunda. Kini pelepasan tap ke-2 ditekan (`suppressRelease`).
- Karena mekanisme double tap, **single tap baru dieksekusi setelah 300 ms** (`DOUBLE_CLICK_MS`) agar bisa dibedakan dari double tap. Ini perilaku bawaan desain, bukan bug baru.
- Toggle Touch Keyboard (Win+Ctrl+O) khusus Windows.
- Keyboard mendukung karakter ASCII huruf, angka, spasi, plus tombol spesial pada `HidKeys.h`.

---

## 7. Struktur Proyek

```
air_mouse_v2/
├── platformio.ini
├── README.md
├── include/
│   ├── Config.h            Semua parameter (pin, timing, gerak, BLE, task)
│   ├── AppState.h          Enum mode/jari/gesture
│   ├── RtosUtil.h          msToTicks (tick minimal 1)
│   ├── HidKeys.h           Konstanta tombol mouse/keyboard/consumer
│   ├── I2cBus.h            Handler I2C: recovery, scan, timeout
│   ├── Mpu9250Driver.h     Driver register-level MPU
│   ├── MotionEngine.h      Algoritma gerak (murni matematika)
│   ├── MpuHandler.h        State machine: cari → kalibrasi → jalan → recovery
│   ├── TouchHandler.h      GPIO jari (debounce, tap, double, hold, combo)
│   ├── ActionMapper.h      Gesture → aksi
│   ├── BleHidDevice.h      Transport NimBLE HID (Report Map, Appearance)
│   ├── BleHandler.h        Queue + task Core 0
│   └── DebugConsole.h      Uji/diagnosis lewat Serial
└── src/  (satu .cpp per header di atas) + main.cpp
```

## 8. Build dan Upload

1. Buka folder ini di VSCode dengan PlatformIO, lalu build/upload (`pio run -t upload`).
2. Serial Monitor 115200. Perhatikan log `[I2C]`/`[MPU]` (bagian 2.3).
3. **Hapus "Air Mouse" lama** dari daftar Bluetooth host, lalu pairing ulang (bagian 4.3).
4. Cek orientasi pemasangan (bagian 3.4).

Bila upload gagal, tahan tombol BOOT saat memulai upload (mode USB TinyUSB pada `platformio.ini` dipertahankan seperti sebelumnya).

## 9. Parameter Tuning Utama (`Config.h`)

| Parameter | Default | Fungsi |
|---|---|---|
| `MOUSE_SENSITIVITY` | 24.0 | Piksel per derajat gerak tangan; `MOUSE_Y_RATIO` untuk sumbu Y |
| `GYRO_DEADZONE_DPS` | 1.5 | Deadzone radial lembut |
| `CURSOR_ACCEL_MAX_GAIN` | 1.8 | Gain maksimum akselerasi pointer |
| `CURSOR_SMOOTH_TAU_SLOW_MS` / `_FAST_MS` | 40 / 4 | Smoothing gerakan lambat/cepat |
| `CURSOR_MAX_SPEED_PX_S` | 7000 | Batas kecepatan kursor |
| `CURSOR_REPORT_INTERVAL_MS` | 8 | Interval laporan mouse (≈125 Hz) |
| `TOUCH_MOTION_FREEZE_MS` | 150 | Freeze kursor saat sentuhan jari |
| `GYRO_CALIB_DURATION_MS` | 1200 | Durasi kalibrasi awal |
| `MPU_AXIS_FORWARD` / `MPU_AXIS_UP` | +X / +Z | Orientasi pemasangan modul |
| `CURSOR_INVERT_X` / `_Y` | 0 / 0 | Membalik arah |
| `I2C_CLOCK_FAST_HZ` | 400000 | Kecepatan target (fallback otomatis 100 kHz) |
| `MPU_ACCEPT_UNKNOWN_WHOAMI` | 1 | Terima WHO_AM_I tak dikenal |
| `ENABLE_DEBUG_CONSOLE` | 1 | Perintah uji via Serial |

Nilai sensitivitas dan smoothing adalah titik awal yang masuk akal, **belum dituning di tangan Anda**; sesuaikan setelah mencoba langsung.
