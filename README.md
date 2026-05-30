# Flipper Zero Toolkit

A collection of custom apps and tools for the **Flipper Zero** hacking multi-tool.

## 📦 Apps Included

### 1. BadUSB Payload Manager (JavaScript)
Quick-deploy USB HID attack payloads with a menu-driven interface.

**Location:** `apps/badusb/payload_manager.js`

**Features:**
- 5 built-in payloads (hello_world, rickroll, fake_update, wifi_password_dump, reverse_shell_hint)
- Plug-and-play via qFlipper file manager
- No compilation needed

**Install:**
```
Copy payload_manager.js to: SD Card/apps/Scripts/badusb/
```

**Usage:**
1. Connect Flipper to target PC via USB
2. Navigate to Apps → Scripts → badusb → payload_manager.js
3. Select payload from menu
4. Watch it execute!

---

### 2. RF Scanner (C / FAP) — *Coming Soon*
Custom sub-GHz frequency scanner for analyzing wireless signals.

---

### 3. BLE Recon Tool (C / FAP) — *Coming Soon*
Bluetooth Low Energy beacon and scanner hybrid.

---

## 🛠️ Development Setup

### Prerequisites
- [Flipper Zero](https://flipperzero.one/)
- [qFlipper](https://flipperzero.one/update) (for file transfer)
- `fbt` build tool (for C apps)

### Building C Apps
```bash
cd flipper-zero-firmware
./fbt fap_{APPID}
```

### JavaScript Apps
No build needed! Drop `.js` files into `SD Card/apps/Scripts/`.

## 📁 Repository Structure

```
flipper-zero-toolkit/
├── apps/
│   ├── badusb/           # BadUSB scripts
│   ├── scripts/          # General JS utilities
│   └── rf_scanner/       # C-based RF scanner (planned)
├── assets/
│   └── icons/            # App icons
├── docs/
│   └── api_reference.md  # Flipper API notes
└── README.md
```

## ⚠️ Disclaimer

These tools are for **authorized security testing and educational purposes only**. Always obtain permission before testing on systems you do not own. The authors assume no liability for misuse.

## 📜 License

MIT License — see LICENSE file.

---

**Built by Ansfidine Youssouf**
