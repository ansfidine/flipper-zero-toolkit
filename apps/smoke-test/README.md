# Flipper Zero JS Smoke Test

Milestone M0 — validates JS runtime, event loop, GUI, and notification subsystems.

## Deploy

1. Copy `hello_flipper.js` to Flipper SD card: `/apps/Scripts/`
2. On Flipper: Apps → Scripts → hello_flipper

## Test Cases

| # | Menu Item | Expected Result |
|---|-----------|----------------|
| 0 | LED Blink Test | Green + Cyan + Yellow blink, dialog shows color meanings |
| 1 | Vibro Test | Error notification (vibro + red LED), dialog confirms |
| 2 | System Info | Widget displays FW/JS/GUI/Notif/EventLoop status |
| 3 | About | Widget shows version, authors, year |
| 4 | Exit | Success notification, clean exit |

## API Pattern

- `submenuView.makeWith()` — declarative view factory
- `eventLoop.subscribe(views.menu.chosen, callback)` — event wiring
- `gui.viewDispatcher.switchTo()` — view navigation
- `eventLoop.stop()` — clean exit
- `notify.blink()`, `notify.error()`, `notify.success()` — notifications
