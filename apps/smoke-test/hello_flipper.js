// Flipper Zero JS Smoke Test
// Validates: deploy → SD card → run pipeline
// Uses: event_loop, gui (ViewDispatcher, submenu, widget, dialog), notification
// Target: Copy to SD Card/apps/Scripts/ and launch from Apps → Scripts

let eventLoop = require("event_loop");
let gui = require("gui");
let submenuView = require("gui/submenu");
let dialogView = require("gui/dialog");
let widgetView = require("gui/widget");
let notify = require("notification");

// ── View Definitions ──
let views = {
    menu: submenuView.makeWith({
        header: "Smoke Test v1",
    }, [
        "LED Blink Test",
        "Vibro Test",
        "System Info",
        "About",
        "Exit",
    ]),
    blink_result: dialogView.makeWith({
        header: "LED Blink",
        text: "Green = Success\nRed = Error\nCyan = Info",
        center: "OK",
    }),
    vibro_result: dialogView.makeWith({
        header: "Vibro Test",
        text: "Vibration pulsed!",
        center: "OK",
    }),
    sysinfo: widgetView.makeWith({}, [
        { element: "string", x: 0, y: 0, align: "left", font: "primary", text: "Flipper Smoke Test" },
        { element: "string", x: 0, y: 14, align: "left", font: "secondary", text: "Firmware: OK" },
        { element: "string", x: 0, y: 24, align: "left", font: "secondary", text: "JS Engine: OK" },
        { element: "string", x: 0, y: 34, align: "left", font: "secondary", text: "GUI: OK" },
        { element: "string", x: 0, y: 44, align: "left", font: "secondary", text: "Notifications: OK" },
        { element: "string", x: 0, y: 54, align: "left", font: "secondary", text: "Event Loop: OK" },
    ]),
    about: widgetView.makeWith({}, [
        { element: "string", x: 0, y: 0, align: "left", font: "primary", text: "Smoke Test v1.0" },
        { element: "string", x: 0, y: 14, align: "left", font: "secondary", text: "Steve + Hermes" },
        { element: "string", x: 0, y: 24, align: "left", font: "secondary", text: "Flipper Zero Dev Team" },
        { element: "string", x: 0, y: 34, align: "left", font: "secondary", text: "2026" },
        { element: "string", x: 0, y: 54, align: "left", font: "secondary", text: "Press BACK to return" },
    ]),
};

// ── Menu Selection Handler ──
eventLoop.subscribe(views.menu.chosen, function (_sub, index, gui, eventLoop, views, notify) {
    if (index === 0) {
        // LED Blink Test — cycle through colors
        notify.blink("green", "short");
        notify.blink("cyan", "short");
        notify.blink("yellow", "short");
        gui.viewDispatcher.switchTo(views.blink_result);
    } else if (index === 1) {
        // Vibro Test
        notify.error(); // triggers vibro + red LED
        gui.viewDispatcher.switchTo(views.vibro_result);
    } else if (index === 2) {
        // System Info
        gui.viewDispatcher.switchTo(views.sysinfo);
    } else if (index === 3) {
        // About
        gui.viewDispatcher.switchTo(views.about);
    } else if (index === 4) {
        // Exit
        notify.success();
        eventLoop.stop();
    }
}, gui, eventLoop, views, notify);

// ── Navigation (Back Button) ──
eventLoop.subscribe(gui.viewDispatcher.navigation, function (_sub, _, gui, views, eventLoop) {
    if (gui.viewDispatcher.currentView === views.menu) {
        eventLoop.stop();
        return;
    }
    gui.viewDispatcher.switchTo(views.menu);
}, gui, views, eventLoop);

// ── Dialog OK buttons return to menu ──
eventLoop.subscribe(views.blink_result.input, function (_sub, button, gui, views) {
    if (button === "center")
        gui.viewDispatcher.switchTo(views.menu);
}, gui, views);

eventLoop.subscribe(views.vibro_result.input, function (_sub, button, gui, views) {
    if (button === "center")
        gui.viewDispatcher.switchTo(views.menu);
}, gui, views);

// ── Launch ──
notify.success(); // Boot beep
gui.viewDispatcher.switchTo(views.menu);
eventLoop.run();