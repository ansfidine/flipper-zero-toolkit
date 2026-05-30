## Flipper Zero Toolkit v3.1.0 — Mac + Windows BadUSB Payloads

**Author:** Ansfidine Youssouf
**Firmware:** Momentum SDK mntm-012 (API 87.1, Target 7)

### What's New

- **6 new DuckyScript payloads** for Mac + Windows:
  - `win_test.txt` — Windows Notepad smoke test
  - `win_reverse_shell.txt` — PowerShell reverse shell (192.168.50.98:4444)
  - `win_wifi_dump.txt` — WiFi profile + password dump to Desktop
  - `mac_test.txt` — Mac TextEdit smoke test
  - `mac_reverse_shell.txt` — bash reverse shell
  - `mac_wifi_dump.txt` — WiFi password dump via Terminal

### DuckyScript Parser Support

BadUSB Manager v1.0+ (with Steve's callback fix `c17b06a`) supports:
- `DELAY`, `DEFAULT_DELAY`
- `STRING`, `STRINGLN`
- `REM` comments
- Modifiers: `GUI`/`WINDOWS`/`COMMAND`/`CMD`, `CTRL`/`CONTROL`, `SHIFT`, `ALT`
- Keys: `ENTER`, `TAB`, `SPACE`, `ESC`, `DELETE`, `BACKSPACE`, `UP`/`DOWN`/`LEFT`/`RIGHT`, `HOME`, `END`, `F1`–`F12`
- Arrow keys for UAC bypass dialogs (`LEFT` key on admin prompts)

### Safety Notes

- Reverse shell payloads target `192.168.50.98:4444` — update IP for your lab
- WiFi dump requires admin privileges (UAC bypass via `CTRL SHIFT ENTER`)
- All payloads are for **authorized testing only**

### Included Apps

| App | File | Version |
|-----|------|---------|
| Sub-GHz Easy | `subghz_cn_easy.fap` | v3.0 |
| BadUSB Manager | `badusb_manager.fap` | v1.1 |

### Attribution

Built by Ansfidine Youssouf for Flipper Zero Momentum firmware.
