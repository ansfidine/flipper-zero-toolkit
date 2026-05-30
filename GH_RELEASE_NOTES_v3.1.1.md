## Flipper Zero Toolkit v3.1.1 — Boss Spec Compliant

**Author:** Ansfidine Youssouf  
**Firmware:** Momentum SDK mntm-012 (API 87.1, Target 7)  
**Collaboration:** Hermes + Steve

### Fixes in v3.1.1

| Issue | Fix |
|-------|-----|
| Custom Scan range 300-928 MHz | **Fixed** → 315/433.92 MHz only |
| `subghz_cn_easy.c` source missing | **Restored** → Full RAW capture/replay source |
| BadUSB payload path wrong | **Fixed** → `/ext/apps_data/badusb/` |
| Duplicate payload folders | **Consolidated** → All 12 payloads in `apps/badusb/payloads/` |

### What's Included

**Apps (3 .fap binaries):**
| App | File | Version | Description |
|-----|------|---------|-------------|
| Sub-GHz Easy | `subghz_cn_easy.fap` | v3.0 | RAW capture + replay, 315/433.92 MHz |
| Sub-GHz Pentest | `subghz_pentest.fap` | v0.5.2 | Category scan + capture + transmit |
| BadUSB Manager | `badusb_manager.fap` | v1.1 | DuckyScript engine, Mac+Windows payloads |

**Payloads (12 DuckyScript files):**
- `win_test.txt` — Windows Notepad smoke test
- `win_reverse_shell.txt` — PowerShell reverse shell (192.168.50.98:4444)
- `win_wifi_dump.txt` — WiFi profile + password dump
- `mac_test.txt` — Mac TextEdit smoke test
- `mac_reverse_shell.txt` — bash reverse shell
- `mac_wifi_dump.txt` — WiFi password dump
- `hello_world.txt` — Universal hello world
- `lock_screen.txt` — Lock screen command
- `matrix_rain.txt` — Matrix rain effect
- `rickroll.txt` — YouTube rickroll
- `wifi_dump.txt` — Generic WiFi dump
- `china_freq_ref.txt` — Chinese frequency reference

### Safety Notes
- Reverse shells target `192.168.50.98:4444` — update for your lab
- WiFi dumps require admin/UAC privileges
- All payloads for **authorized testing only**

### Attribution
Built by Ansfidine Youssouf for Flipper Zero Momentum firmware.
