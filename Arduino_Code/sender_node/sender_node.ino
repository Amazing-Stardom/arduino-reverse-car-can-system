/*
 * MIT License
 * 
 * Copyright (c) 2025 Ganesh Kumar
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights  
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all  
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER  
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN  
 * THE SOFTWARE.
 */

// ------------------- LIBRARIES -------------------
#include <SPI.h>
#include <mcp2515.h>

// ------------------- SENSOR CONFIG -------------------
#define NUM_SENSORS 3
const uint8_t sensorPin[NUM_SENSORS] = {4, 5, 6}; // Echo/Trigger on single pin per sensor

// ------------------- CAN CONTROLLER -------------------
MCP2515 mcp2515(10);  // CS pin D10
struct can_frame canMsg;

// ------------------- STATE VARIABLES -------------------
uint8_t lastLevels[NUM_SENSORS] = {0, 0, 0};  // Last known levels for change detection

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for Serial to initialize (especially on Leonardo)

  // Initialize CAN bus
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS);
  mcp2515.setNormalMode();

  // Prepare CAN message frame
  canMsg.can_id  = 0x100;  // Standard frame ID
  canMsg.can_dlc = 2;      // Only 2 bytes of data (3 sensors packed)

  // Set up pins as output for ultrasonic sensors
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensorPin[i], OUTPUT);
    digitalWrite(sensorPin[i], LOW);
  }
}

// ------------------- SENSOR READING -------------------
// Returns distance in cm or 0xFFFF if invalid
uint16_t readDistance(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(2);
  digitalWrite(pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(pin, LOW);

  pinMode(pin, INPUT);
  long duration = pulseIn(pin, HIGH, 30000L); // Timeout 30ms (~5m)
  return duration > 0 ? duration * 0.0343 / 2.0 : 0xFFFF;
}

// ------------------- DISTANCE TO LEVEL MAPPING -------------------
int getLevel(uint16_t d) {
  if (d == 0xFFFF) return 0;      // Error
  if (d > 50) return 5;
  else if (d > 35) return 4;
  else if (d > 20) return 3;
  else if (d > 10) return 2;
  else return 1;
}

// ------------------- MAIN LOOP -------------------
void loop() {
  uint8_t currentLevels[NUM_SENSORS];
  bool changed = false;

  // Read all sensors
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    uint16_t dist = readDistance(sensorPin[i]);
    currentLevels[i] = getLevel(dist);
    delay(50); // Small delay between readings

    // Print debug info
    Serial.print("Sensor ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(dist);
    Serial.print(" cm → Level ");
    Serial.println(currentLevels[i]);

    // Detect change from last reading
    if (currentLevels[i] != lastLevels[i]) {
      changed = true;
    }
  }

  // If any sensor changed, send CAN message
  if (changed) {
    // Pack 3 x 3-bit values into 2 bytes
    uint16_t packed = ((currentLevels[0] & 0x07) << 6) |
                      ((currentLevels[1] & 0x07) << 3) |
                      (currentLevels[2] & 0x07);

    canMsg.data[0] = highByte(packed);
    canMsg.data[1] = lowByte(packed);

    // Send CAN message
    mcp2515.sendMessage(&canMsg);
    Serial.println("CAN Message Sent");

    // Update last levels
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
      lastLevels[i] = currentLevels[i];
    }
  } else {
    Serial.println("No change in levels – skipping CAN send");
  }

  delay(200); // Main loop delay
}
