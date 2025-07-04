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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// ------------------- LIBRARIES -------------------
#include <SPI.h>
#include <mcp2515.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ------------------- OLED SETTINGS -------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------------------- CAN CONTROLLER -------------------
MCP2515 mcp2515(10); // CS pin = 10
struct can_frame canMsg;

// ------------------- BUZZER SETTINGS -------------------
#define BUZZER_PIN 9

// ------------------- GLOBAL VARIABLES -------------------
uint8_t lastLevels[3] = {0, 0, 0};  // Left, Center, Right proximity levels
unsigned long lastBeepTime = 0;
bool beepState = false;
uint16_t currentBeepInterval = 1000;  // Default: no beep

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);

  // Initialize CAN
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS);
  mcp2515.setNormalMode();

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (1);  // Halt system if OLED fails
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Waiting for CAN data...");
  display.display();

  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

// ------------------- LEVEL TO LABEL -------------------
const char* getRangeLabel(uint8_t lvl) {
  switch (lvl) {
    case 1: return "Very Short";
    case 2: return "Short";
    case 3: return "Medium";
    case 4: return "High";
    case 5: return "Very High";
    default: return "No Data";
  }
}

// ------------------- OLED DISPLAY FUNCTION -------------------
void updateDisplay(uint8_t level[3]) {
  const char* labels[3] = { "Back L", "Back", "Back R" };
  const int barWidth = 10;
  const int barSpacing = 40;
  const int barBaseY = 35;
  const int barHeightStep = 6;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Reverse Proximity:");

  for (int i = 0; i < 3; i++) {
    int x = 5 + i * barSpacing;
    int lvl = level[i];

    for (int l = 0; l < lvl; l++) {
      int y = barBaseY - (l * barHeightStep);
      display.drawLine(x, y, x + barWidth, y, SSD1306_WHITE);
    }

    display.setCursor(x - 2, barBaseY + 2);
    display.print(labels[i]);

    display.setCursor(x - 2, barBaseY + 12);
    display.print(getRangeLabel(lvl));
  }

  display.display();
}

// ------------------- BUZZER LOGIC -------------------
void handleBuzzer(uint8_t level[3]) {
  uint8_t minLevel = 5;

  for (int i = 0; i < 3; i++) {
    if (level[i] > 0 && level[i] < minLevel) {
      minLevel = level[i];
    }
  }

  // Map level to beep interval
  switch (minLevel) {
    case 1: currentBeepInterval = 150; break;
    case 2: currentBeepInterval = 400; break;
    case 3: currentBeepInterval = 800; break;
    case 4: currentBeepInterval = 1500; break;
    default: currentBeepInterval = 0; break;
  }

  // Buzzer ON/OFF control
  if (currentBeepInterval > 0) {
    if (!beepState && millis() - lastBeepTime >= currentBeepInterval) {
      lastBeepTime = millis();
      digitalWrite(BUZZER_PIN, HIGH);
      beepState = true;
    }
    if (beepState && millis() - lastBeepTime >= 100) {
      digitalWrite(BUZZER_PIN, LOW);
      beepState = false;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    beepState = false;
  }
}

// ------------------- MAIN LOOP -------------------
void loop() {
  // Read CAN message
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK && canMsg.can_dlc >= 2) {
    uint16_t packedBits = (uint16_t(canMsg.data[0]) << 8) | canMsg.data[1];

    lastLevels[0] = (packedBits >> 6) & 0x07;
    lastLevels[1] = (packedBits >> 3) & 0x07;
    lastLevels[2] = packedBits & 0x07;

    Serial.print("Levels: ");
    for (int i = 0; i < 3; i++) {
      Serial.print(lastLevels[i]);
      Serial.print(" ");
    }
    Serial.println();
  }

  // Update display and buzzer
  updateDisplay(lastLevels);
  handleBuzzer(lastLevels);
}
