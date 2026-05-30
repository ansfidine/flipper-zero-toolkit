# Flipper Zero Toolkit

A collection of custom apps and tools for the **Flipper Zero** hacking multi-tool.

Built by Ansfidine Youssouf

## 📦 Apps Included

### 1. BadUSB Manager (C / FAP) v2.0
**Real USB HID keystroke injection** — load .txt payload scripts from SD card and type them out on connected PCs.

**Location:** `apps/badusb/badusb_manager.fap`

**Features:**
- Load payload scripts from `/ext/apps_data/badusb/*.txt`
- Real `furi_hal_hid_kb_press()` keystroke injection
- USB connection detection (shows status on screen)
- LED feedback (blue = running, green = done, red = error)
- Menu-driven payload selection

**Payload Script Format:**
```
STRING Hello World
DELAY 500
ENTER
```

**Available Commands:**
| Command | Action |
|---------|--------|
| `STRING text` | Type the text |
| `DELAY ms` | Wait milliseconds |
| `ENTER` | Press Return key |
| `TAB` | Press Tab key |
| `ESCAPE` | Press Escape key |
| `REM text` | Comment (ignored) |

**Sample Payloads:** See `apps/badusb/payloads/`

**Install:**
```
1. Copy badusb_manager.fap to: SD Card/apps/USB/
2. Copy payload .txt files to: SD Card/apps_data/badusb/
3. Connect Flipper to target PC via USB
4. Run: Apps → USB → BadUSB Manager
```

---

### 2. Sub-GHz Easy (C / FAP) v1.2
Easy-to-use sub-GHz scanner by **device category** — no frequency knowledge needed!

**Location:** `apps/subghz/subghz_cn_easy.fap`

**Features:**
- **Device categories:** `[C]` Car Remote, `[G]` Garage Door, `[A]` Alarm System, `[S]` Wireless Sensor, `[H]` Smart Home, `[E]` E-Scooter, `[P]` Parking Barrier, `[L]` LED Controller, `[X]` Custom
- Auto-scans the correct frequency band for each device type
- One-tap signal capture + replay
- Real-time RSSI meter with signal strength bar
- Works on **Momentum firmware** (API 87.1)

**Target Frequencies (China ISM bands):**
| Category | Band | Devices |
|----------|------|---------|
| `[C]` Car Remote | 310-320 MHz | Car keys, keyless entry |
| `[G]` Garage Door | 430-440 MHz | Garage remotes, gates |
| `[A]` Alarm System | 315-345 MHz | Home alarms, sirens |
| `[S]` Wireless Sensor | 433-435 MHz | Temp/humidity sensors |
| `[H]` Smart Home | 315-440 MHz | Switches, plugs, bulbs |
| `[E]` E-Scooter | 430-440 MHz | Scooter remotes (433 MHz) |
| `[P]` Parking Barrier | 430-440 MHz | Parking gates |
| `[L]` LED Controller | 315-440 MHz | RGB controllers |
| `[X]` Custom | 300-928 MHz | Full CC1101 range |

**Install:**
```
Copy subghz_cn_easy.fap to: SD Card/apps/Sub-GHz/
```

**Usage:**
1. Run: Apps → Sub-GHz → Sub-GHz Easy
2. Pick device type (e.g., `[C]` Car Remote)
3. App auto-scans the right frequency band
4. When signal detected, press OK to capture
5. Press OK again to replay

---

### 3. BLE Recon Tool (C / FAP) — *Coming Soon*
Bluetooth Low Energy beacon and scanner hybrid.

---

## 🛠️ Development Setup

### Prerequisites
- [Flipper Zero](https://flipperzero.one/)
- [qFlipper](https://flipperzero.one/update) (for file transfer)
- `ufbt` build tool: `pip3 install ufbt`

### Building C Apps
```bash
cd flipper-badusb-fap  # or flipper-subghz-cn
ufbt fap_badusb_manager   # or ufbt fap_subghz_cn_easy
```

### Creating Payload Scripts
Create `.txt` files in `/ext/apps_data/badusb/`:
```
STRING notepad
ENTER
DELAY 1000
STRING Hello from Flipper!
ENTER
```

---

## 📁 Repository Structure

```
flipper-zero-toolkit/
├── apps/
│   ├── badusb/
│   │   ├── badusb_manager.fap   # C FAP binary
│   │   └── payloads/             # Sample .txt scripts
│   └── subghz/
│       └── subghz_cn_easy.fap   # C FAP binary
├── README.md
└── LICENSE
```

---

## ⚠️ Disclaimer

These tools are for **authorized security testing and educational purposes only**. Always obtain permission before testing on systems you do not own. The authors assume no liability for misuse.

BadUSB payloads can trigger AV/EDR on target PCs — test on isolated VMs only.

---

## 📜 License

MIT License — see LICENSE file.
