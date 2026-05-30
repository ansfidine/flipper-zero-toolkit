# Sub-GHz CN Pentest Tool

A sub-GHz scanner, emulator, and analysis tool designed for **Chinese ISM frequencies** on Flipper Zero with **Momentum firmware**.

## 🇨🇳 Target Frequencies

| Band | Center Freq | Range | Common Uses in China |
|------|------------|-------|---------------------|
| **315 MHz** | 315.000 MHz | 310-320 MHz | Car remotes, alarms, wireless keypads |
| **433 MHz** | 433.920 MHz | 430-440 MHz | Garage doors, sensors, wireless switches |

## 🎯 Features

### 1. Scan 315 MHz / Scan 433 MHz
- Sweeps 1 MHz range around center frequency
- 5 kHz step resolution
- Real-time RSSI meter with signal strength bar
- Auto-detection of active signals above -80 dBm threshold
- Signal counter

### 2. Read Signal
- Lock onto 433.92 MHz for signal capture
- Displays real-time RSSI
- Captures signal metadata for emulation

### 3. Emulate
- TX burst on captured frequency
- Simple OOK modulation for replay
- **For educational/authorized testing only**

### 4. Analyze
- Protocol detection placeholder
- Signal statistics display

## 📥 Install

1. Download `subghz_cn_pentest.fap` from Releases
2. Copy to Flipper: `SD Card/apps/` via qFlipper
3. Run from Apps → Sub-GHz → Sub-GHz CN

## 🎮 Controls

| Button | Action |
|--------|--------|
| **↑/↓** | Navigate menu |
| **OK** | Select / Transmit |
| **Back** | Stop scan / Return to menu / Exit |

## ⚠️ Legal Notice

This tool is for **authorized security testing and educational purposes only**. In China, transmitting on ISM bands without proper authorization may violate regulations. Always:
- Obtain permission before testing
- Do not interfere with emergency or licensed services
- Use only in controlled environments

## 🔧 Technical Details

- **Modulation**: OOK (On-Off Keying), AM_650 preset
- **Hardware Path**: Automatic switching (315/433/868)
- **API Level**: 87.1 (Momentum compatible)
- **Stack**: 4KB

## 🏗️ Future Enhancements

- PT2262 / EV1527 protocol decoding
- Rolling code detection
- Raw signal recording to SD card
- Brute-force packet replay
- Frequency hopping analysis

---

**Built by Ansfidine Youssouf**
