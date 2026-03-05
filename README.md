
# EnOcean Core Framework

A cross-platform EnOcean Bluetooth Low Energy framework supporting **POSIX** (Linux / Raspberry Pi via BlueZ) and **Zephyr RTOS** (nRF52840, nrf7002dk/nrf5340 with native BT stack). It provides a unified API for commissioning, managing, and receiving events from EnOcean BLE devices such as PTM wall switches and STM sensors.

---

## Supported Platforms

| Platform | Target Hardware          | BLE Stack        |
|----------|--------------------------|------------------|
| POSIX    | Linux, Raspberry Pi      | BlueZ            |
| Zephyr   | nRF52840, nrf7002dk/nrf5340    | Zephyr BT stack  |

---

## Architecture Overview

The project is organized into modular components to support code reuse and platform-specific customization:

```text
enocean-core/
├── application/         # Platform-agnostic application entry point
│   ├── AppEntry.cpp     # Wires EnOcean driver + BLE scanner + crypto
│   ├── AppEntry.hpp
│   └── CMakeLists.txt
├── src/
│   └── enocean/         # Platform-agnostic EnOcean driver (header-only template)
│       ├── EnoceanDriver.hpp   # EnoceanDriver<CryptoBackend> — core logic
│       ├── EnoceanDriver.ipp   # Template method implementations
│       ├── EnoceanTypes.hpp    # Device, SensorData, callbacks, protocol constants
│       ├── EnoceanError.hpp    # EnoceanError enum + ErrorTraits
│       └── CMakeLists.txt
├── platform/
│   ├── common/
│   │   ├── ible/        # IBle<Derived> — CRTP BLE scanner interface
│   │   ├── icrypto/     # ICrypto<Derived> — CRTP AES-128-CCM interface
│   │   ├── ithread/     # IThread<Derived> — CRTP thread interface
│   │   ├── imutex/      # IMutex<Derived>
│   │   ├── isemaphore/  # ISemaphore<Derived>
│   │   ├── imessage_queue/
│   │   └── isleep/
│   ├── posix_core/      # POSIX platform (Linux / Raspberry Pi)
│   │   ├── ble/         # BlueZ HCI socket passive scanner
│   │   ├── crypto/      # OpenSSL AES-128-CCM-4
│   │   ├── thread/      # pthreads
│   │   ├── mutex/       # POSIX mutex
│   │   ├── semaphore/   # POSIX semaphore
│   │   ├── message_queue/ # POSIX mqueue
│   │   ├── sleep/
│   │   └── posix_main.cpp
│   └── zephyr_core/     # Zephyr RTOS (nRF52840)
│       ├── ble/         # bt_le_scan_cb_register observer
│       ├── crypto/      # Zephyr PSA AEAD AES-128-CCM-4
│       ├── thread/      # Zephyr k_thread
│       ├── mutex/       # Zephyr k_mutex
│       ├── semaphore/   # Zephyr k_sem
│       ├── message_queue/ # Zephyr k_msgq
│       ├── sleep/
│       └── zephyr_main.cpp
├── boards/
│   ├── common/          # Shared hardware interfaces (IGpio, ISerial, IWatchdog)
│   ├── posix/           # POSIX board drivers
│   └── zephyr/          # Zephyr board drivers (GPIO, LED, Watchdog)
├── test/                # Unit tests (POSIX only, GoogleTest)
│   └── enocean_driver_test/ # EnoceanDriver tests with MockCrypto
├── cmake/               # CMake modules (flags, sanitizers, ccache, codechecker)
├── scripts/             # Formatting and static analysis helpers
└── README.md
```

### Component Details

- **src/enocean/**: Platform-agnostic EnOcean driver. Parses manufacturer-specific BLE advertisements (Company ID `0x03DA`), handles commissioning (extracts AES-128 key), performs AES-CCM-4 tag verification, enforces sequence-number replay protection, and dispatches typed events via callbacks. Templated on `CryptoBackend` — zero virtual-function overhead.
- **platform/common/ible/**: CRTP interface `IBle<Derived>` — `init()`, `start_scan()`, `stop_scan()`. Validated by the `BleImplementation` concept at compile time.
- **platform/common/icrypto/**: CRTP interface `ICrypto<Derived>` — single method `aes_ccm_decrypt()`. Validated by `CryptoImplementation` concept.
- **application/**: Owns `Crypto::AesCcm`, `EnoceanDriver<Crypto::AesCcm>`, and `Ble::Scanner`. Registers callbacks and starts the scan.
- **boards/**: Abstracts hardware interfaces (GPIO, serial, watchdog) with platform-specific implementations.
- **platform/posix_core / zephyr_core/**: Platform startup, OS primitives, and concrete BLE/crypto backends.
- **cmake/**: Custom CMake scripts for flags, sanitizers, ccache, codechecker, and build info generation.
- **test/**: Unit tests for OS primitives and the EnOcean driver, only compiled on POSIX.

### EnOcean Driver Data Flow

```text
BLE adapter (HCI / Zephyr BT)
        │  raw LE advertisement
        ▼
Ble::Scanner::scan_loop()          ← runs on dedicated thread (POSIX)
        │                            or BT RX work-queue (Zephyr)
        │  calls advertisement_cb(ctx, addr, addr_type, adv_type, rssi, data)
        ▼
EnoceanDriver::process_advertisement()
        │
        ├─ extract manufacturer-specific AD (Company ID 0x03DA)
        ├─ detect device type from BLE address bytes [4:5]
        │
        ├─[commissioning mode ON + payload ≥ 22 bytes]──► store key + fire commissioned_cb
        │
        └─[known device]
                ├─ read sequence number (LE uint32)
                ├─ replay check: seq > stored?
                ├─ build 13-byte CCM nonce  (addr[6] ‖ seq[4] ‖ 0x000000)
                ├─ ICrypto::aes_ccm_decrypt() — verify 4-byte CCM tag
                └─ dispatch  button_cb  /  sensor_cb
```

---

## Zephyr Setup

Before building for Zephyr RTOS, initialize the Zephyr environment:

1. **Install Zephyr**: Follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)

2. **Activate Zephyr virtual environment**:

   ```bash
   source ~/zephyrproject/.venv/bin/activate
   ```

3. **Initialize workspace** (from the parent directory of `enocean-core`):

   ```bash
   cd ..
   west init -l enocean-core
   west update
   ```

4. **Source Zephyr environment**:

   ```bash
   source ./zephyr/zephyr-env.sh
   ```

5. **Return to project directory**:

   ```bash
   cd enocean-core
   ```

---

## Build Instructions

### Zephyr RTOS (nRF52840)

```bash
west build -p always -b nrf52840dk/nrf52840 platform/zephyr_core --sysbuild -d build/zephyr
```

### Zephyr RTOS (nRF7002dk/nrf5340/cpuapp)

```bash
west build -p always -b nrf7002dk/nrf5340/cpuapp platform/zephyr_core --sysbuild -d build/zephyr
```

### POSIX (Linux / Raspberry Pi)

#### 1. Install required packages

```bash
sudo apt update
sudo apt install libbluetooth-dev libssl-dev bluez pkg-config -y
```

| Package | Provides |
| --- | --- |
| `libbluetooth-dev` | BlueZ headers and `libhci` — required to compile `platform/posix_core/ble/` |
| `libssl-dev` | OpenSSL headers — required to compile `platform/posix_core/crypto/` |
| `bluez` | BlueZ daemon and `hciconfig` / `hcitool` utilities |
| `pkg-config` | Required by CMake to locate the BlueZ library (`pkg_check_modules`) |

#### 2. Configure and build

```bash
cmake -S ./platform/posix_core -B build/posix -GNinja
cmake --build build/posix
```

#### 3. Prepare the Bluetooth adapter

```bash
# Confirm the adapter is visible
hciconfig

# Bring it up if it shows as DOWN
sudo hciconfig hci0 up
```

#### 4. Run

The application opens a raw HCI socket which requires elevated privileges:

```bash
# Option A — run as root (simplest)
sudo ./build/posix/app

# Option B — grant capabilities once, then run as normal user
sudo setcap 'cap_net_raw,cap_net_admin+ep' ./build/posix/app
./build/posix/app
```

Expected startup output:

```text
[timestamp] EnoceanDriver initialised (max 8 devices)
[timestamp] BLE passive scan started (hci0)
[timestamp] EnOcean scanner ready — commissioning enabled.
```

#### 5. Stop bluetoothd (if running)

The raw HCI socket approach may conflict with `bluetoothd`. If the scan commands fail or no devices appear, stop the daemon first:

```bash
sudo systemctl stop bluetooth
```

Restart it after testing with `sudo systemctl start bluetooth`.

#### 6. Commission an EnOcean device

The application starts with commissioning enabled. You must trigger commissioning mode on the device **while the app is running**:

- **PTM215B wall switch**: Hold the small recessed commissioning button (pinhole) for ~7 seconds until the LED blinks
- **STM550B sensor**: Hold the commissioning button until the LED confirms

The driver logs:

```text
[timestamp] EnOcean device COMMISSIONED: DA:03:XX:XX:XX:XX
```

From that point, every button press produces:

```text
[timestamp] EnOcean button PRESS | addr DA:03:XX:XX:XX:XX | buttons 0x01
```

#### Troubleshooting — no devices found

| Symptom | Likely cause | Fix |
| ------- | ------------ | --- |
| No `BLE scanner: N LE advertising events` message | BLE adapter down or bluetoothd conflict | `sudo hciconfig hci0 up`, stop bluetoothd |
| `hci_le_set_scan_parameters failed: Device or resource busy` | bluetoothd controls the adapter | `sudo systemctl stop bluetooth` |
| Scanner active (counter increments) but no COMMISSIONED log | Device not in commissioning mode | Hold commissioning button until LED blinks |
| COMMISSIONED appears but buttons not logged | Wrong sequence number or auth failure | Re-commission the device |

The scanner prints a progress counter every 100 received LE advertising events — if this never appears within 10–20 seconds of nearby BLE activity, the HCI socket is not receiving events.

---

## Run POSIX Unit Tests

Unit tests are **only supported on the POSIX platform** and are built with GoogleTest. The build system automatically skips tests for Zephyr, even if `-DBUILD_TESTING=ON` is specified.

1. **Configure** (tests are disabled by default):

   ```bash
   cmake -S platform/posix_core -B build/posix -GNinja -DBUILD_TESTING=ON
   ```

2. **Build the test binaries**:

   ```bash
   cmake --build build/posix
   ```

   Or build individual targets, e.g.:

   ```bash
   cmake --build build/posix --target message_queue_tests mutex_tests thread_tests
   ```

3. **Run everything with CTest**:

   ```bash
   ctest --output-on-failure --test-dir=build/posix/app_src/
   ```

You can also run an individual binary directly, e.g. `./build/posix/app_src/message_queue/tests/message_queue_tests`.

---

## Run Static Code Analysis

> **Note:** `clang-tidy` and `cppcheck` must be installed. These are required for the analysis commands below.

### Zephyr RTOS

```bash
west build -d build/zephyr/zephyr_core -t codechecker_analyse
```

### POSIX

```bash
cd build/posix
ninja codechecker_analyse
```

---

## Run Valgrind with POSIX App

```bash
sudo apt update
sudo apt install valgrind kcachegrind massif-visualizer
```

### Memory Check

```bash
sudo valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 -s ./build/posix/app
```

### Callgrind (Profiling)

```bash
sudo valgrind --tool=callgrind ./build/posix/app
kcachegrind callgrind.out.<PID>
```

### Helgrind (Thread Safety)

```bash
sudo valgrind --tool=helgrind ./build/posix/app
```

### Massif (Heap Profiling)

```bash
sudo valgrind --tool=massif ./build/posix/app
massif-visualizer massif.out.<PID>
```

### DRD (Data Race Detection)

```bash
sudo valgrind --tool=drd ./build/posix/app
```

---

## How to Extend

1. **Add new board support**: Create a folder under `boards/` and implement the required interfaces (`IGpio`, `ISerial`, etc.).
2. **Add new platform**: Add a folder under `platform/` and provide main entry, OS primitive implementations, and BLE integration.
3. **Implement application logic**: Use `application/` for platform-independent EnOcean logic and call hardware/OS via abstraction interfaces.
4. **Customize build**: Modify or add CMake scripts in `cmake/` for toolchain or build options.

---

## Example Use Cases

- EnOcean PTM wall switch integration over BLE (Linux/Raspberry Pi via BlueZ, nRF52840 via Zephyr BT)
- EnOcean STM sensor data acquisition (occupancy, light level, temperature)
- Cross-platform EnOcean commissioning and device management
- Desktop simulation and testing of EnOcean event handling (POSIX)
