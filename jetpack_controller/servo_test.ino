/*
 * ESP32 Servo Test
 * 
 * Simple test to verify servo wiring is correct.
 * Cycles servo between 0° and 180° every 3 seconds.
 * 
 * WIRING:
 *   ESP32 GPIO 13 → Servo Signal (orange/yellow wire)
 *   External 5V   → Servo VCC (red wire)
 *   External GND  → Servo GND (brown/black wire)
 *   ESP32 GND     → External GND (common ground!)
 * 
 * IMPORTANT: Install "ESP32Servo" library in Arduino IDE:
 *   Tools → Manage Libraries → Search "ESP32Servo" → Install
 */

#include <ESP32Servo.h>

// Pin configuration
const int SERVO_PIN = 13;  // GPIO 13 - safe pin for servo

// Servo object
Servo myServo;

// Position tracking
int currentPosition = 0;
bool movingToMax = true;

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(1000);  // Give serial time to connect
  
  Serial.println("================================");
  Serial.println("  ESP32 Servo Test v1.0");
  Serial.println("================================");
  Serial.println();
  
  // Allow allocation of all timers for servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  // Attach servo to pin with standard 50Hz PWM
  myServo.setPeriodHertz(50);  // Standard servo frequency
  myServo.attach(SERVO_PIN, 500, 2400);  // Min/max pulse width in microseconds
  
  Serial.print("Servo attached to GPIO ");
  Serial.println(SERVO_PIN);
  Serial.println();
  Serial.println("Starting servo test cycle...");
  Serial.println("Cycling between 0° and 180° every 3 seconds");
  Serial.println();
  
  // Move to starting position
  myServo.write(0);
  Serial.println("→ Moving to 0 degrees (starting position)");
}

void loop() {
  // Wait 3 seconds
  delay(3000);
  
  // Toggle direction
  if (movingToMax) {
    currentPosition = 180;
    Serial.println("→ Moving to 180 degrees");
  } else {
    currentPosition = 0;
    Serial.println("→ Moving to 0 degrees");
  }
  
  // Move servo
  myServo.write(currentPosition);
  
  // Flip direction for next cycle
  movingToMax = !movingToMax;
}

