/*
 * Mandalorian Jetpack Controller v1.0
 * 
 * ESP32 WiFi Access Point + HTTP Server for servo control
 * Controls water dispenser servo for dry ice fog effect
 * 
 * WIRING:
 *   ESP32 GPIO 13 → Servo Signal (orange/yellow wire)
 *   External 5V   → Servo VCC (red wire)
 *   External GND  → Servo GND (brown/black wire)
 *   ESP32 GND     → External GND (common ground!)
 * 
 * USAGE:
 *   1. Power on ESP32
 *   2. Connect phone/laptop to WiFi: "MandalorianJetpack"
 *   3. Password: "thiIsTheWay"
 *   4. Open browser: http://192.168.4.1
 *   5. Click ACTIVATE or use API endpoints
 * 
 * API ENDPOINTS:
 *   GET /           - Test page with button
 *   GET /activate   - Trigger 5-second dispense
 *   GET /status     - Get current state as JSON
 *   GET /stop       - Emergency stop
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ============== CONFIGURATION ==============
// WiFi Access Point settings
const char* AP_SSID = "MandalorianJetpack";
const char* AP_PASSWORD = "thisIsTheWay";  // Min 8 characters

// Servo settings
const int SERVO_PIN = 13;           // GPIO 13
const int SERVO_OFF_POSITION = 10;  // Degrees - no water flow
const int SERVO_ON_POSITION = 90;   // Degrees - water dispensing
const int DISPENSE_DURATION = 5000; // Milliseconds (5 seconds)

// LED settings (built-in LED for visual feedback)
const int LED_PIN = 2;  // Built-in LED on most ESP32 boards

// ============== GLOBAL OBJECTS ==============
WebServer server(80);
Servo dispenserServo;

// ============== STATE VARIABLES ==============
bool isDispensing = false;
unsigned long dispenseStartTime = 0;
int remainingSeconds = 0;

// ============== HTML TEST PAGE ==============
const char* HTML_PAGE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Jetpack Controller</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Arial Black', sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            color: #fff;
            padding: 20px;
        }
        h1 {
            color: #ff6b35;
            text-shadow: 0 0 20px rgba(255, 107, 53, 0.5);
            margin-bottom: 10px;
            font-size: 28px;
        }
        .subtitle {
            color: #888;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .status-box {
            background: rgba(255,255,255,0.1);
            border-radius: 15px;
            padding: 20px;
            margin-bottom: 30px;
            text-align: center;
            min-width: 280px;
        }
        .status-label { color: #888; font-size: 12px; }
        .status-value { 
            font-size: 32px; 
            font-weight: bold;
            margin: 10px 0;
        }
        .status-idle { color: #4ade80; }
        .status-active { color: #ff6b35; animation: pulse 1s infinite; }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .countdown {
            font-size: 64px;
            color: #ff6b35;
            text-shadow: 0 0 30px rgba(255, 107, 53, 0.8);
        }
        .activate-btn {
            background: linear-gradient(145deg, #ff6b35, #e55a2b);
            border: none;
            border-radius: 50%;
            width: 150px;
            height: 150px;
            color: white;
            font-size: 18px;
            font-weight: bold;
            cursor: pointer;
            box-shadow: 0 10px 30px rgba(255, 107, 53, 0.4);
            transition: all 0.2s;
            text-transform: uppercase;
        }
        .activate-btn:hover:not(:disabled) {
            transform: scale(1.05);
            box-shadow: 0 15px 40px rgba(255, 107, 53, 0.6);
        }
        .activate-btn:active:not(:disabled) {
            transform: scale(0.95);
        }
        .activate-btn:disabled {
            background: linear-gradient(145deg, #666, #555);
            cursor: not-allowed;
            box-shadow: none;
        }
        .stop-btn {
            background: #dc2626;
            border: none;
            border-radius: 10px;
            padding: 15px 30px;
            color: white;
            font-size: 14px;
            font-weight: bold;
            cursor: pointer;
            margin-top: 20px;
        }
        .log {
            background: rgba(0,0,0,0.3);
            border-radius: 10px;
            padding: 15px;
            margin-top: 30px;
            font-family: monospace;
            font-size: 12px;
            color: #4ade80;
            max-width: 300px;
            max-height: 150px;
            overflow-y: auto;
        }
    </style>
</head>
<body>
    <h1>🚀 JETPACK CONTROL</h1>
    <p class="subtitle">Mandalorian Fog System v1.0</p>
    
    <div class="status-box">
        <div class="status-label">STATUS</div>
        <div id="status" class="status-value status-idle">READY</div>
        <div id="countdown" class="countdown" style="display:none;">5</div>
    </div>
    
    <button id="activateBtn" class="activate-btn" onclick="activate()">
        ACTIVATE<br>FOG
    </button>
    
    <button class="stop-btn" onclick="emergencyStop()">
        EMERGENCY STOP
    </button>
    
    <div id="log" class="log"></div>

    <script>
        let pollInterval = null;
        
        function log(msg) {
            const logEl = document.getElementById('log');
            const time = new Date().toLocaleTimeString();
            logEl.innerHTML = `[${time}] ${msg}<br>` + logEl.innerHTML;
        }
        
        function updateStatus(data) {
            const statusEl = document.getElementById('status');
            const countdownEl = document.getElementById('countdown');
            const btn = document.getElementById('activateBtn');
            
            if (data.state === 'active') {
                statusEl.textContent = 'DISPENSING';
                statusEl.className = 'status-value status-active';
                countdownEl.style.display = 'block';
                countdownEl.textContent = data.remaining;
                btn.disabled = true;
            } else {
                statusEl.textContent = 'READY';
                statusEl.className = 'status-value status-idle';
                countdownEl.style.display = 'none';
                btn.disabled = false;
            }
        }
        
        function pollStatus() {
            fetch('/status')
                .then(r => r.json())
                .then(data => {
                    updateStatus(data);
                    if (data.state === 'idle' && pollInterval) {
                        clearInterval(pollInterval);
                        pollInterval = null;
                        log('Dispense complete');
                    }
                })
                .catch(e => log('Error: ' + e));
        }
        
        function activate() {
            log('Activating servo...');
            fetch('/activate')
                .then(r => r.json())
                .then(data => {
                    log('Response: ' + data.status);
                    if (data.status === 'activating') {
                        pollInterval = setInterval(pollStatus, 500);
                    }
                    pollStatus();
                })
                .catch(e => log('Error: ' + e));
        }
        
        function emergencyStop() {
            log('EMERGENCY STOP');
            fetch('/stop')
                .then(r => r.json())
                .then(data => {
                    log('Stopped: ' + data.status);
                    pollStatus();
                })
                .catch(e => log('Error: ' + e));
        }
        
        // Initial status check
        pollStatus();
        log('Controller ready');
    </script>
</body>
</html>
)rawliteral";

// ============== SETUP ==============
void setup() {
    // Initialize Serial
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("  MANDALORIAN JETPACK CONTROLLER v1.0");
    Serial.println("========================================");
    Serial.println();
    
    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.println("[LED] Initialized on GPIO " + String(LED_PIN));
    
    // Initialize Servo
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    
    dispenserServo.setPeriodHertz(50);
    dispenserServo.attach(SERVO_PIN, 500, 2400);
    dispenserServo.write(SERVO_OFF_POSITION);
    Serial.println("[SERVO] Initialized on GPIO " + String(SERVO_PIN));
    Serial.println("[SERVO] OFF position: " + String(SERVO_OFF_POSITION) + " degrees");
    Serial.println("[SERVO] ON position: " + String(SERVO_ON_POSITION) + " degrees");
    Serial.println();
    
    // Initialize WiFi Access Point
    Serial.println("[WIFI] Starting Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.println("[WIFI] Access Point started!");
    Serial.println("[WIFI] SSID: " + String(AP_SSID));
    Serial.println("[WIFI] Password: " + String(AP_PASSWORD));
    Serial.println("[WIFI] IP Address: " + IP.toString());
    Serial.println();
    
    // Setup HTTP endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/popup", HTTP_GET, handlePopup);  // Minimal popup for PWA
    server.on("/activate", HTTP_GET, handleActivate);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/stop", HTTP_GET, handleStop);
    server.onNotFound(handleNotFound);
    
    // Start server
    server.begin();
    Serial.println("[HTTP] Web server started on port 80");
    Serial.println();
    Serial.println("========================================");
    Serial.println("  READY - Connect to WiFi and open:");
    Serial.println("  http://" + IP.toString());
    Serial.println("========================================");
    Serial.println();
    
    // Blink LED to indicate ready
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
}

// ============== MAIN LOOP ==============
void loop() {
    // Handle HTTP requests
    server.handleClient();
    
    // Check if dispense timer has expired
    if (isDispensing) {
        unsigned long elapsed = millis() - dispenseStartTime;
        remainingSeconds = (DISPENSE_DURATION - elapsed) / 1000 + 1;
        
        if (elapsed >= DISPENSE_DURATION) {
            stopDispensing();
        } else {
            // Blink LED while dispensing
            digitalWrite(LED_PIN, (millis() / 250) % 2);
        }
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}

// ============== HTTP HANDLERS ==============

void handleRoot() {
    Serial.println("[HTTP] GET / from " + server.client().remoteIP().toString());
    server.send(200, "text/html", HTML_PAGE);
}

// Minimal popup page for PWA integration
void handlePopup() {
    Serial.println("[HTTP] GET /popup from " + server.client().remoteIP().toString());
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>Jetpack</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Arial Black', sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #0f0f1a 100%);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            color: #fff;
            padding: 20px;
        }
        h1 {
            color: #ff6b35;
            font-size: 24px;
            margin-bottom: 20px;
            text-shadow: 0 0 15px rgba(255, 107, 53, 0.5);
        }
        .status {
            font-size: 18px;
            margin-bottom: 30px;
            color: #4ade80;
        }
        .status.active {
            color: #ff6b35;
            animation: pulse 0.5s infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .countdown {
            font-size: 80px;
            color: #ff6b35;
            text-shadow: 0 0 30px rgba(255, 107, 53, 0.8);
            margin-bottom: 20px;
        }
        .btn {
            background: linear-gradient(145deg, #ff6b35, #e55a2b);
            border: none;
            border-radius: 20px;
            width: 200px;
            height: 80px;
            color: white;
            font-size: 22px;
            font-weight: bold;
            cursor: pointer;
            box-shadow: 0 8px 25px rgba(255, 107, 53, 0.5);
            text-transform: uppercase;
            transition: all 0.2s;
        }
        .btn:active {
            transform: scale(0.95);
            box-shadow: 0 4px 15px rgba(255, 107, 53, 0.3);
        }
        .btn:disabled {
            background: linear-gradient(145deg, #666, #444);
            box-shadow: none;
            cursor: not-allowed;
        }
        .btn.stop {
            background: linear-gradient(145deg, #dc2626, #b91c1c);
            box-shadow: 0 8px 25px rgba(220, 38, 38, 0.5);
            margin-top: 15px;
            height: 50px;
            font-size: 16px;
        }
        .close-hint {
            margin-top: 30px;
            color: #666;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <h1>🚀 JETPACK FOG</h1>
    <div id="status" class="status">READY</div>
    <div id="countdown" class="countdown" style="display:none;">5</div>
    <button id="activateBtn" class="btn" onclick="activate()">ACTIVATE</button>
    <button class="btn stop" onclick="stop()">STOP</button>
    <div class="close-hint">Close this tab to return to soundboard</div>
    
    <script>
        let polling = null;
        
        function activate() {
            document.getElementById('activateBtn').disabled = true;
            document.getElementById('status').textContent = 'ACTIVATING...';
            
            fetch('/activate')
                .then(r => r.json())
                .then(data => {
                    if (data.status === 'activating' || data.status === 'already_active') {
                        startPolling();
                    }
                })
                .catch(e => {
                    document.getElementById('status').textContent = 'ERROR';
                    document.getElementById('activateBtn').disabled = false;
                });
        }
        
        function stop() {
            fetch('/stop').then(() => checkStatus());
        }
        
        function startPolling() {
            polling = setInterval(checkStatus, 500);
            checkStatus();
        }
        
        function checkStatus() {
            fetch('/status')
                .then(r => r.json())
                .then(data => {
                    const statusEl = document.getElementById('status');
                    const countdownEl = document.getElementById('countdown');
                    const btn = document.getElementById('activateBtn');
                    
                    if (data.state === 'active') {
                        statusEl.textContent = 'FOG ACTIVE!';
                        statusEl.className = 'status active';
                        countdownEl.style.display = 'block';
                        countdownEl.textContent = data.remaining;
                        btn.disabled = true;
                    } else {
                        statusEl.textContent = 'READY';
                        statusEl.className = 'status';
                        countdownEl.style.display = 'none';
                        btn.disabled = false;
                        if (polling) {
                            clearInterval(polling);
                            polling = null;
                        }
                    }
                });
        }
        
        // Initial status check
        checkStatus();
    </script>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
}

void handleActivate() {
    Serial.println("[HTTP] GET /activate from " + server.client().remoteIP().toString());
    
    // Add CORS headers for cross-origin requests from PWA
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    
    if (isDispensing) {
        Serial.println("[SERVO] Already dispensing - ignoring request");
        server.send(200, "application/json", "{\"status\":\"already_active\",\"remaining\":" + String(remainingSeconds) + "}");
        return;
    }
    
    // Start dispensing
    startDispensing();
    
    server.send(200, "application/json", "{\"status\":\"activating\",\"duration\":" + String(DISPENSE_DURATION / 1000) + "}");
}

void handleStatus() {
    // Add CORS headers
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    
    String json;
    if (isDispensing) {
        json = "{\"state\":\"active\",\"remaining\":" + String(remainingSeconds) + "}";
    } else {
        json = "{\"state\":\"idle\"}";
    }
    
    server.send(200, "application/json", json);
}

void handleStop() {
    Serial.println("[HTTP] GET /stop (EMERGENCY) from " + server.client().remoteIP().toString());
    
    // Add CORS headers
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    
    if (isDispensing) {
        stopDispensing();
        server.send(200, "application/json", "{\"status\":\"stopped\"}");
    } else {
        server.send(200, "application/json", "{\"status\":\"was_idle\"}");
    }
}

void handleNotFound() {
    Serial.println("[HTTP] 404 - " + server.uri());
    server.send(404, "application/json", "{\"error\":\"not_found\"}");
}

// ============== SERVO CONTROL ==============

void startDispensing() {
    Serial.println("[SERVO] *** ACTIVATING - Moving to " + String(SERVO_ON_POSITION) + " degrees ***");
    
    isDispensing = true;
    dispenseStartTime = millis();
    remainingSeconds = DISPENSE_DURATION / 1000;
    
    // Move servo to ON position
    dispenserServo.write(SERVO_ON_POSITION);
    digitalWrite(LED_PIN, HIGH);
    
    Serial.println("[SERVO] Timer started: " + String(DISPENSE_DURATION / 1000) + " seconds");
}

void stopDispensing() {
    Serial.println("[SERVO] *** DEACTIVATING - Moving to " + String(SERVO_OFF_POSITION) + " degrees ***");
    
    isDispensing = false;
    remainingSeconds = 0;
    
    // Move servo to OFF position
    dispenserServo.write(SERVO_OFF_POSITION);
    digitalWrite(LED_PIN, LOW);
    
    Serial.println("[SERVO] Dispense cycle complete");
    Serial.println();
}

