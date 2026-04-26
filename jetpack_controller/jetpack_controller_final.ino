/*
 * Mandalorian Jetpack Controller FINAL v2.2
 * 
 * ESP32 WiFi Access Point + HTTP Server for jetpack effect control
 * Controls water pumps + NeoPixel LED exhaust effect for realistic jetpack
 * 
 * WIRING:
 *   Water Pumps (L298N H-Bridge):
 *     PUMP 1 (Motor A):
 *       ESP32 GPIO 23  → L298N IN1 (Motor A direction control 1)
 *       ESP32 GPIO 22  → L298N IN2 (Motor A direction control 2) *OPTIONAL*
 *       Pump 1 wire 1  → L298N OUT1
 *       Pump 1 wire 2  → L298N OUT2
 *     
 *     PUMP 2 (Motor B):
 *       ESP32 GPIO 19  → L298N IN3 (Motor B direction control 1)
 *       ESP32 GPIO 18  → L298N IN4 (Motor B direction control 2) *OPTIONAL*
 *       Pump 2 wire 1  → L298N OUT3
 *       Pump 2 wire 2  → L298N OUT4
 *     
 *     COMMON:
 *       ESP32 GND      → L298N GND (REQUIRED - common ground!)
 *       Power 12V      → L298N +12V
 *       Power GND      → L298N GND
 *       Jumper on ENA  → Leave ON (enables Motor A)
 *       Jumper on ENB  → Leave ON (enables Motor B)
 *     
 *     *NOTE: GPIO 22 (IN2) and GPIO 18 (IN4) can be left UNCONNECTED
 *            for forward-only operation. The L298N has internal pull-down
 *            resistors that will keep these LOW.
 *   
 *   LED (NeoPixel):
 *     ESP32 GPIO 21  → NeoPixel Data In
 *     ESP32 GND      → Common ground
 *     External 5V    → NeoPixel VCC
 *     External GND   → NeoPixel GND + ESP32 GND
 * 
 * USAGE:
 *   1. Power on ESP32
 *   2. Connect phone/laptop to WiFi: "MandalorianJetpack"
 *   3. Password: "thisIsTheWay"
 *   4. Open browser: http://192.168.4.1
 *   5. Click ACTIVATE from iPhone soundboard app (v5.9.6)
 * 
 * API ENDPOINTS:
 *   GET /           - Test page with button
 *   GET /popup      - Minimal popup for PWA integration
 *   GET /activate   - Trigger jetpack sequence (pump + LED)
 *   GET /status     - Get current state as JSON
 *   GET /stop       - Emergency stop
 * 
 * LIBRARY REQUIRED:
 *   Adafruit NeoPixel (install via Library Manager)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ============== WIFI CONFIGURATION ==============
const char* AP_SSID = "MandalorianJetpack";
const char* AP_PASSWORD = "thisIsTheWay";  // Min 8 characters

// ============== PUMP CONFIGURATION ==============
// PUMP 1 (Motor A on L298N)
const int PUMP1_IN1 = 23;   // ESP32 GPIO 23 → L298N IN1 (active control)
const int PUMP1_IN2 = 22;   // ESP32 GPIO 22 → L298N IN2 (adjacent to GPIO 23, near GND)

// PUMP 2 (Motor B on L298N)
const int PUMP2_IN3 = 19;   // ESP32 GPIO 19 → L298N IN3 (active control)
const int PUMP2_IN4 = 18;   // ESP32 GPIO 18 → L298N IN4 (adjacent to GPIO 19)

// Timing
const unsigned long pumpOnTime = 5000;   // 5 seconds pump ON
const unsigned long LED_START_DELAY = 2000;  // LED starts 2 seconds after pump

// ============== LED CONFIGURATION ==============
#define LED_PIN     21         // ESP32 GPIO 21 for NeoPixel data
#define LED_COUNT   16         // 16-LED circular NeoPixel ring

const uint8_t BASE_BRIGHTNESS = 60;   // Normal operating brightness
const uint8_t MAX_BRIGHTNESS  = 160;  // Peak during full thrust / bursts

// LED phase timing (in milliseconds)
const uint32_t IGNITION_TIME_MS    = 5000;   // 5 seconds - sparks and flame catch
const uint32_t FULL_THRUST_TIME_MS = 33000;  // 33 seconds - main burn
const uint32_t SHUTDOWN_TIME_MS    = 16000;  // 16 seconds - cool down and sputter out
const uint32_t OFF_TIME_MS         = 3000;   // 3 seconds - completely off before next cycle

// ============== GLOBAL OBJECTS ==============
WebServer server(80);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============== STATE TRACKING ==============
enum SystemState {
  STATE_IDLE,
  STATE_PUMP_STARTING,
  STATE_LED_RUNNING,
  STATE_WAITING_FOR_LED
};

SystemState currentState = STATE_IDLE;
unsigned long pumpStartTime = 0;
unsigned long ledStartTime = 0;
bool pumpIsOn = false;
bool ledSequenceActive = false;
bool systemBusy = false;  // For HTTP status responses

// LED phase tracking
enum LEDPhase {
  PHASE_IGNITION,
  PHASE_FULL_THRUST,
  PHASE_SHUTDOWN,
  PHASE_OFF,
  PHASE_COMPLETE
};

LEDPhase currentLEDPhase = PHASE_IGNITION;
unsigned long phaseStartTime = 0;

// LED state variables
static int numAttempts = 0;
static int currentAttempt = 0;
static uint32_t attemptTime = 0;
static uint32_t timePerAttempt = 0;
static uint32_t rampTime = 0;
static uint32_t attemptStart = 0;
static float attemptStrength = 0;
static uint32_t sparkDuration = 0;
static uint32_t flameDuration = 0;
static uint32_t dieDuration = 0;
static uint32_t flameStart = 0;
static uint32_t dieStart = 0;
static uint32_t rampStart = 0;
static float burstLevel = 0.0f;
static const float periodMs = 1500.0f;
static bool isBlackedOut = false;
static uint32_t blackoutEndTime = 0;

enum IgnitionSubPhase {
  IGNITION_INIT,
  IGNITION_SPARKS,
  IGNITION_FLAME_CATCH,
  IGNITION_FLAME_DIE,
  IGNITION_PAUSE,
  IGNITION_SUCCESS_FLASH,
  IGNITION_RAMP
};

static IgnitionSubPhase ignitionSubPhase = IGNITION_INIT;

// Remaining seconds counter for status API
int remainingSeconds = 0;

// ============== LED FLAME PALETTE ==============
struct FlameColor {
  uint8_t r, g, b;
  int weight;
};

const FlameColor FLAME_PALETTE[] = {
  {140,  15,   0, 35},  // Deep red (most common)
  {180,  30,   0, 25},  // Red-orange
  {220,  60,   5, 20},  // Orange
  {255, 100,  10, 12},  // Bright orange
  {255, 150,  30,  5},  // Yellow-orange (less common)
  {255, 200,  80,  2},  // Yellow (rare)
  {255, 240, 150,  1},  // White-hot (very rare)
};
const int PALETTE_SIZE = sizeof(FLAME_PALETTE) / sizeof(FLAME_PALETTE[0]);
const int TOTAL_WEIGHT = 35 + 25 + 20 + 12 + 5 + 2 + 1;

// ================== LED HELPER FUNCTIONS =================

uint8_t lerp8(uint8_t a, uint8_t b, float t) {
  return a + (int16_t)((b - a) * t);
}

void getWeightedFlameColor(uint8_t &r, uint8_t &g, uint8_t &b) {
  int roll = random(0, TOTAL_WEIGHT);
  int cumulative = 0;
  
  for (int i = 0; i < PALETTE_SIZE; i++) {
    cumulative += FLAME_PALETTE[i].weight;
    if (roll < cumulative) {
      r = FLAME_PALETTE[i].r;
      g = FLAME_PALETTE[i].g;
      b = FLAME_PALETTE[i].b;
      return;
    }
  }
  r = 140; g = 15; b = 0;
}

void applyFlicker(uint8_t &r, uint8_t &g, uint8_t &b, int flicker) {
  int16_t rr = r + random(-flicker, flicker + 1);
  int16_t gg = g + random(-flicker, flicker + 1);
  int16_t bb = b + random(-flicker / 2, flicker / 2 + 1);

  r = constrain(rr, 0, 255);
  g = constrain(gg, 0, 255);
  b = constrain(bb, 0, 255);
}

void drawCircularFlame(int hotSpotChance = 10, float warmth = 1.0, int flicker = 20) {
  for (int i = 0; i < strip.numPixels(); i++) {
    uint8_t r, g, b;
    
    getWeightedFlameColor(r, g, b);
    
    if (random(0, 100) < hotSpotChance) {
      r = 255;
      g = random(180, 255);
      b = random(60, 150);
    }

    float warmR = r * warmth;
    float warmG = g * warmth;
    float warmB = b * (0.7 + (warmth - 1.0) * 0.1);

    r = constrain((int)warmR, 0, 255);
    g = constrain((int)warmG, 0, 255);
    b = constrain((int)warmB, 0, 255);

    if (flicker > 0) {
      applyFlicker(r, g, b, flicker);
    }

    strip.setPixelColor(i, strip.Color(r, g, b));
  }
}

void allOff() {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
}

// ================== PUMP CONTROL =================

void startPumps() {
  digitalWrite(PUMP1_IN1, HIGH);
  digitalWrite(PUMP1_IN2, LOW);
  digitalWrite(PUMP2_IN3, HIGH);
  digitalWrite(PUMP2_IN4, LOW);
  
  pumpIsOn = true;
  pumpStartTime = millis();
  Serial.println("[PUMP] ON (" + String(pumpOnTime / 1000.0, 1) + "s) - FORWARD");
}

void stopPumps() {
  digitalWrite(PUMP1_IN1, LOW);
  digitalWrite(PUMP1_IN2, LOW);
  digitalWrite(PUMP2_IN3, LOW);
  digitalWrite(PUMP2_IN4, LOW);
  
  pumpIsOn = false;
  Serial.println("[PUMP] OFF");
}

// ================== LED ANIMATION =================

void startLEDSequence() {
  ledSequenceActive = true;
  currentLEDPhase = PHASE_IGNITION;
  phaseStartTime = millis();
  ledStartTime = millis();
  
  numAttempts = random(3, 5);
  currentAttempt = 0;
  attemptTime = (uint32_t)(IGNITION_TIME_MS * 0.6f);
  rampTime = IGNITION_TIME_MS - attemptTime;
  timePerAttempt = attemptTime / numAttempts;
  ignitionSubPhase = IGNITION_INIT;
  
  Serial.println(F("[LED] Phase 1: Ignition"));
  Serial.print(F("  Attempts: "));
  Serial.println(numAttempts);
}

void updateLEDAnimation() {
  if (!ledSequenceActive) return;
  
  unsigned long now = millis();
  unsigned long phaseElapsed = now - phaseStartTime;
  
  switch (currentLEDPhase) {
    case PHASE_IGNITION:
      updateIgnitionPhase(now, phaseElapsed);
      if (phaseElapsed >= IGNITION_TIME_MS) {
        currentLEDPhase = PHASE_FULL_THRUST;
        phaseStartTime = now;
        burstLevel = 0.0f;
        Serial.println(F("[LED] Phase 2: Full Thrust"));
      }
      break;
      
    case PHASE_FULL_THRUST:
      updateFullThrustPhase(now, phaseElapsed);
      if (phaseElapsed >= FULL_THRUST_TIME_MS) {
        currentLEDPhase = PHASE_SHUTDOWN;
        phaseStartTime = now;
        isBlackedOut = false;
        Serial.println(F("[LED] Phase 3: Shutdown"));
      }
      break;
      
    case PHASE_SHUTDOWN:
      updateShutdownPhase(now, phaseElapsed);
      if (phaseElapsed >= SHUTDOWN_TIME_MS) {
        currentLEDPhase = PHASE_OFF;
        phaseStartTime = now;
        allOff();
        Serial.println(F("[LED] Phase 4: Off"));
      }
      break;
      
    case PHASE_OFF:
      if (phaseElapsed >= OFF_TIME_MS) {
        currentLEDPhase = PHASE_COMPLETE;
        ledSequenceActive = false;
        Serial.println(F("[LED] Sequence complete"));
      }
      break;
      
    case PHASE_COMPLETE:
      break;
  }
}

void updateIgnitionPhase(unsigned long now, unsigned long phaseElapsed) {
  switch (ignitionSubPhase) {
    case IGNITION_INIT:
      if (currentAttempt >= numAttempts) {
        ignitionSubPhase = IGNITION_SUCCESS_FLASH;
        break;
      }
      attemptStart = now;
      attemptStrength = 0.3f + (0.5f * currentAttempt / (float)(numAttempts - 1));
      sparkDuration = timePerAttempt * 0.3f;
      ignitionSubPhase = IGNITION_SPARKS;
      break;
      
    case IGNITION_SPARKS:
      {
        strip.setBrightness(40);
        for (int i = 0; i < strip.numPixels(); i++) {
          strip.setPixelColor(i, strip.Color(5, 0, 0));
        }
        int numSparks = random(1, 3 + currentAttempt);
        for (int s = 0; s < numSparks; s++) {
          int p = random(0, strip.numPixels());
          strip.setPixelColor(p, strip.Color(255, random(180, 255), random(80, 150)));
        }
        strip.show();
        
        if (now - attemptStart >= sparkDuration) {
          flameDuration = timePerAttempt * 0.35f;
          flameStart = now;
          ignitionSubPhase = IGNITION_FLAME_CATCH;
        }
      }
      break;
      
    case IGNITION_FLAME_CATCH:
      {
        float flameProgress = (float)(now - flameStart) / (float)flameDuration;
        float intensity;
        if (flameProgress < 0.4f) {
          intensity = flameProgress / 0.4f;
        } else {
          intensity = 1.0f - ((flameProgress - 0.4f) / 0.6f);
        }
        intensity *= attemptStrength;
        
        uint8_t brightness = (uint8_t)(BASE_BRIGHTNESS * intensity * 0.8f);
        strip.setBrightness(max((uint8_t)15, brightness));
        
        float warmth = 0.6f + 0.3f * intensity;
        int hotSpotChance = (int)(8 * intensity);
        
        drawCircularFlame(hotSpotChance, warmth, 20);
        strip.show();
        
        if (now - flameStart >= flameDuration) {
          dieDuration = timePerAttempt * 0.35f;
          dieStart = now;
          ignitionSubPhase = IGNITION_FLAME_DIE;
        }
      }
      break;
      
    case IGNITION_FLAME_DIE:
      {
        float dieProgress = (float)(now - dieStart) / (float)dieDuration;
        uint8_t brightness = lerp8((uint8_t)(BASE_BRIGHTNESS * attemptStrength * 0.3f), 0, dieProgress);
        strip.setBrightness(brightness);
        
        for (int i = 0; i < strip.numPixels(); i++) {
          if (random(0, 100) < 20 - (int)(15 * dieProgress)) {
            strip.setPixelColor(i, strip.Color(random(80, 150), random(5, 20), 0));
          } else {
            strip.setPixelColor(i, 0);
          }
        }
        strip.show();
        
        if (now - dieStart >= dieDuration) {
          allOff();
          attemptStart = now;
          ignitionSubPhase = IGNITION_PAUSE;
        }
      }
      break;
      
    case IGNITION_PAUSE:
      if (now - attemptStart >= random(150, 350)) {
        currentAttempt++;
        ignitionSubPhase = IGNITION_INIT;
      }
      break;
      
    case IGNITION_SUCCESS_FLASH:
      Serial.println(F("  IGNITION SUCCESS!"));
      strip.setBrightness(MAX_BRIGHTNESS);
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(255, 200, 80));
      }
      strip.show();
      delay(80);
      rampStart = now;
      ignitionSubPhase = IGNITION_RAMP;
      break;
      
    case IGNITION_RAMP:
      {
        float progress = (float)(now - rampStart) / (float)rampTime;
        progress = constrain(progress, 0.0f, 1.0f);
        
        uint8_t brightness;
        if (progress < 0.2f) {
          brightness = lerp8(MAX_BRIGHTNESS, BASE_BRIGHTNESS, progress / 0.2f);
        } else {
          brightness = lerp8(BASE_BRIGHTNESS, (uint8_t)(BASE_BRIGHTNESS * 1.1f), (progress - 0.2f) / 0.8f);
        }
        strip.setBrightness(brightness);
        
        float warmth = 0.8f + 0.3f * progress;
        int hotSpotChance = 8 + (int)(7 * progress);
        
        drawCircularFlame(hotSpotChance, warmth, 25);
        strip.show();
      }
      break;
  }
}

void updateFullThrustPhase(unsigned long now, unsigned long phaseElapsed) {
  float t = (float)phaseElapsed;
  
  float phase = (t / periodMs) * 2.0f * PI;
  float breath = 0.85f + 0.15f * (0.5f * (sin(phase) + 1.0f));
  
  if (burstLevel <= 0.01f && random(0, 1000) < 25) {
    burstLevel = 1.0f;
  }
  
  float brightFactor = 1.2f * breath * (1.0f + 0.8f * burstLevel);
  uint8_t brightness = constrain((int)(BASE_BRIGHTNESS * brightFactor), 0, MAX_BRIGHTNESS);
  strip.setBrightness(brightness);
  
  float warmth = 1.1f + 0.4f * burstLevel;
  int hotSpotChance = 15 + (int)(25 * burstLevel);
  
  drawCircularFlame(hotSpotChance, warmth, 30);
  strip.show();
  
  if (burstLevel > 0.0f) {
    burstLevel *= 0.88f;
    if (burstLevel < 0.02f) burstLevel = 0.0f;
  }
}

void updateShutdownPhase(unsigned long now, unsigned long phaseElapsed) {
  float progress = (float)phaseElapsed / (float)SHUTDOWN_TIME_MS;
  progress = constrain(progress, 0.0f, 1.0f);
  
  if (progress < 0.3f) {
    float stageProgress = progress / 0.3f;
    uint8_t brightness = lerp8(BASE_BRIGHTNESS, (uint8_t)(BASE_BRIGHTNESS * 0.7f), stageProgress);
    strip.setBrightness(brightness);
    
    float warmth = 1.0f - 0.2f * stageProgress;
    int hotSpotChance = 12 - (int)(5 * stageProgress);
    
    drawCircularFlame(hotSpotChance, warmth, 25);
    strip.show();
  }
  else if (progress < 0.7f) {
    float stageProgress = (progress - 0.3f) / 0.4f;
    int blackoutChance = 8 + (int)(20 * stageProgress);
    
    if (!isBlackedOut && random(0, 100) < blackoutChance) {
      isBlackedOut = true;
      int blackoutDuration = random(80, 200 + (int)(150 * stageProgress));
      blackoutEndTime = now + blackoutDuration;
      allOff();
    }
    
    if (isBlackedOut) {
      if (now >= blackoutEndTime) {
        isBlackedOut = false;
      } else {
        return;
      }
    }
    
    uint8_t brightness = lerp8((uint8_t)(BASE_BRIGHTNESS * 0.7f), (uint8_t)(BASE_BRIGHTNESS * 0.4f), stageProgress);
    strip.setBrightness(brightness);
    
    float warmth = 0.8f - 0.2f * stageProgress;
    int hotSpotChance = 7 - (int)(4 * stageProgress);
    
    drawCircularFlame(hotSpotChance, warmth, 20);
    
    int deadPixels = random(0, 3 + (int)(5 * stageProgress));
    for (int d = 0; d < deadPixels; d++) {
      strip.setPixelColor(random(0, strip.numPixels()), 0);
    }
    
    strip.show();
  }
  else {
    float stageProgress = (progress - 0.7f) / 0.3f;
    int blackoutChance = 35 + (int)(40 * stageProgress);
    
    if (!isBlackedOut && random(0, 100) < blackoutChance) {
      isBlackedOut = true;
      int blackoutDuration = random(150, 400 + (int)(600 * stageProgress));
      blackoutEndTime = now + blackoutDuration;
      allOff();
    }
    
    if (isBlackedOut) {
      if (now >= blackoutEndTime) {
        isBlackedOut = false;
        if (random(0, 100) < 40) {
          strip.setBrightness(BASE_BRIGHTNESS * 0.6f);
          drawCircularFlame(20, 0.9f, 30);
          strip.show();
          delay(random(50, 150));
        }
      } else {
        return;
      }
    }
    
    uint8_t brightness = lerp8((uint8_t)(BASE_BRIGHTNESS * 0.4f), 5, stageProgress);
    strip.setBrightness(brightness);
    
    float warmth = 0.6f - 0.3f * stageProgress;
    int hotSpotChance = 3 - (int)(3 * stageProgress);
    
    drawCircularFlame(hotSpotChance, warmth, 10);
    
    int deadPixels = 4 + (int)(10 * stageProgress);
    for (int d = 0; d < deadPixels; d++) {
      strip.setPixelColor(random(0, strip.numPixels()), 0);
    }
    
    strip.show();
  }
}

// ================== JETPACK SEQUENCE CONTROL =================

void startJetpackSequence() {
  Serial.println("[JETPACK] *** ACTIVATING SEQUENCE ***");
  
  systemBusy = true;
  currentState = STATE_IDLE;  // Will immediately transition to PUMP_STARTING
  
  Serial.println("[JETPACK] Sequence timer started");
}

void stopJetpackSequence() {
  Serial.println("[JETPACK] *** EMERGENCY STOP ***");
  
  stopPumps();
  allOff();
  currentState = STATE_IDLE;
  ledSequenceActive = false;
  systemBusy = false;
  
  Serial.println("[JETPACK] System stopped");
}

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
    <p class="subtitle">Mandalorian Jetpack v2.2</p>
    
    <div class="status-box">
        <div class="status-label">STATUS</div>
        <div id="status" class="status-value status-idle">READY</div>
    </div>
    
    <button id="activateBtn" class="activate-btn" onclick="activate()">
        ACTIVATE<br>JETPACK
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
            const btn = document.getElementById('activateBtn');
            
            if (data.state === 'active') {
                statusEl.textContent = 'FIRING';
                statusEl.className = 'status-value status-active';
                btn.disabled = true;
            } else {
                statusEl.textContent = 'READY';
                statusEl.className = 'status-value status-idle';
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
                        log('Sequence complete');
                    }
                })
                .catch(e => log('Error: ' + e));
        }
        
        function activate() {
            log('Activating jetpack...');
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

// Minimal popup page for PWA integration (iPhone app)
const char* POPUP_PAGE = R"rawliteral(
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
    </style>
</head>
<body>
    <h1>🚀 JETPACK</h1>
    <div id="status" class="status">READY</div>
    <button id="activateBtn" class="btn" onclick="activate()">ACTIVATE</button>
    <button class="btn stop" onclick="goBack()">DONE</button>
    
    <script>
        let polling = null;
        
        function activate() {
            document.getElementById('activateBtn').disabled = true;
            document.getElementById('status').textContent = 'ACTIVATING...';
            
            // Tell soundboard tab to play the rocket sound (file already cached on phone)
            if (window.opener) {
                window.opener.postMessage('play-rocket', '*');
            }
            
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
        
        function goBack() {
            window.close();
            history.back();
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
                    const btn = document.getElementById('activateBtn');
                    
                    if (data.state === 'active') {
                        statusEl.textContent = 'FIRING ROCKETS!';
                        statusEl.className = 'status active';
                        btn.disabled = true;
                    } else {
                        statusEl.textContent = 'READY';
                        statusEl.className = 'status';
                        btn.disabled = false;
                        if (polling) {
                            clearInterval(polling);
                            polling = null;
                        }
                    }
                });
        }
        
        checkStatus();
    </script>
</body>
</html>
)rawliteral";

// ============== HTTP HANDLERS ==============

void handleRoot() {
  Serial.println("[HTTP] GET / from " + server.client().remoteIP().toString());
  server.send(200, "text/html", HTML_PAGE);
}

void handlePopup() {
  Serial.println("[HTTP] GET /popup from " + server.client().remoteIP().toString());
  server.send(200, "text/html", POPUP_PAGE);
}

void handleActivate() {
  Serial.println("[HTTP] GET /activate from " + server.client().remoteIP().toString());
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  
  if (systemBusy) {
    Serial.println("[JETPACK] Already active - ignoring request");
    server.send(200, "application/json", "{\"status\":\"already_active\"}");
    return;
  }
  
  startJetpackSequence();
  
  server.send(200, "application/json", "{\"status\":\"activating\"}");
}

void handleStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  
  String json;
  if (systemBusy) {
    json = "{\"state\":\"active\"}";
  } else {
    json = "{\"state\":\"idle\"}";
  }
  
  server.send(200, "application/json", json);
}

void handleStop() {
  Serial.println("[HTTP] GET /stop (EMERGENCY) from " + server.client().remoteIP().toString());
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  
  if (systemBusy) {
    stopJetpackSequence();
    server.send(200, "application/json", "{\"status\":\"stopped\"}");
  } else {
    server.send(200, "application/json", "{\"status\":\"was_idle\"}");
  }
}

void handleNotFound() {
  Serial.println("[HTTP] 404 - " + server.uri());
  server.send(404, "application/json", "{\"error\":\"not_found\"}");
}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("  MANDALORIAN JETPACK CONTROLLER v2.2");
  Serial.println("========================================");
  Serial.println();
  
  // Pump setup
  pinMode(PUMP1_IN1, OUTPUT);
  pinMode(PUMP1_IN2, OUTPUT);
  pinMode(PUMP2_IN3, OUTPUT);
  pinMode(PUMP2_IN4, OUTPUT);
  
  digitalWrite(PUMP1_IN1, LOW);
  digitalWrite(PUMP1_IN2, LOW);
  digitalWrite(PUMP2_IN3, LOW);
  digitalWrite(PUMP2_IN4, LOW);
  
  Serial.println("[PUMP] Initialized");
  Serial.println("  Pump 1: IN1=GPIO" + String(PUMP1_IN1) + ", IN2=GPIO" + String(PUMP1_IN2));
  Serial.println("  Pump 2: IN3=GPIO" + String(PUMP2_IN3) + ", IN4=GPIO" + String(PUMP2_IN4));
  Serial.println("  Pump ON time: " + String(pumpOnTime / 1000.0, 1) + " seconds");
  Serial.println("  LED start delay: " + String(LED_START_DELAY / 1000.0, 1) + " seconds");
  Serial.println();
  
  // LED setup
  strip.begin();
  strip.setBrightness(0);
  strip.show();
  
  randomSeed(esp_random());
  
  Serial.println("[LED] Initialized on GPIO " + String(LED_PIN));
  Serial.println("  LED Count: " + String(LED_COUNT));
  Serial.println("  Phase Timing:");
  Serial.println("    Ignition:    " + String(IGNITION_TIME_MS / 1000) + "s");
  Serial.println("    Full Thrust: " + String(FULL_THRUST_TIME_MS / 1000) + "s");
  Serial.println("    Shutdown:    " + String(SHUTDOWN_TIME_MS / 1000) + "s");
  Serial.println("    Off:         " + String(OFF_TIME_MS / 1000) + "s");
  Serial.println("    Total:       " + String((IGNITION_TIME_MS + FULL_THRUST_TIME_MS + SHUTDOWN_TIME_MS + OFF_TIME_MS) / 1000) + "s");
  Serial.println();
  
  // WiFi Access Point
  Serial.println("[WIFI] Starting Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("[WIFI] Access Point started!");
  Serial.println("  SSID: " + String(AP_SSID));
  Serial.println("  Password: " + String(AP_PASSWORD));
  Serial.println("  IP Address: " + IP.toString());
  Serial.println();
  
  // HTTP endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/popup", HTTP_GET, handlePopup);
  server.on("/activate", HTTP_GET, handleActivate);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/stop", HTTP_GET, handleStop);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");
  Serial.println();
  Serial.println("========================================");
  Serial.println("  READY - Connect to WiFi and open:");
  Serial.println("  http://" + IP.toString());
  Serial.println("========================================");
  Serial.println();
}

// ============== MAIN LOOP ==============
void loop() {
  server.handleClient();
  
  unsigned long now = millis();
  
  // State machine for jetpack sequence
  switch (currentState) {
    case STATE_IDLE:
      if (systemBusy) {
        // Start new cycle
        startPumps();
        currentState = STATE_PUMP_STARTING;
      }
      break;
      
    case STATE_PUMP_STARTING:
      // Wait for LED start delay
      if (now - pumpStartTime >= LED_START_DELAY) {
        startLEDSequence();
        currentState = STATE_LED_RUNNING;
      }
      
      // Check if pumps should turn off
      if (now - pumpStartTime >= pumpOnTime) {
        stopPumps();
        currentState = STATE_WAITING_FOR_LED;
      }
      break;
      
    case STATE_LED_RUNNING:
      updateLEDAnimation();
      
      // Check if pumps should turn off
      if (pumpIsOn && (now - pumpStartTime >= pumpOnTime)) {
        stopPumps();
        currentState = STATE_WAITING_FOR_LED;
      }
      
      // Check if LED sequence is complete
      if (!ledSequenceActive) {
        Serial.println(F("[JETPACK] Cycle complete\n"));
        currentState = STATE_IDLE;
        systemBusy = false;
      }
      break;
      
    case STATE_WAITING_FOR_LED:
      updateLEDAnimation();
      
      // Wait for LED to finish
      if (!ledSequenceActive) {
        Serial.println(F("[JETPACK] Cycle complete\n"));
        currentState = STATE_IDLE;
        systemBusy = false;
      }
      break;
  }
  
  delay(10);
}
