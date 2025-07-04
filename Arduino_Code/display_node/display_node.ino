#include <SPI.h>
#include <mcp2515.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// CAN
MCP2515 mcp2515(10);
struct can_frame canMsg;

// Buzzer pin
#define BUZZER_PIN 9

// Last sensor levels
uint8_t lastLevels[3] = {0, 0, 0};

// Buzzer logic
unsigned long lastBeepTime = 0;
bool beepState = false;
uint16_t currentBeepInterval = 1000;  // Default no beep

void setup() {
  Serial.begin(115200);

  // Init CAN
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS);
  mcp2515.setNormalMode();

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Waiting for CAN data...");
  display.display();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

// Map level to label
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

// OLED drawing
void updateDisplay(uint8_t level[3]) {
  const char* labels[3] = { "Back L", "Back", "Back R" };

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Reverse Proximity:");

  const int barWidth = 10;
  const int barSpacing = 40;
  const int barBaseY = 35;
  const int barHeightStep = 6;

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

// Buzzer pulse control
void handleBuzzer(uint8_t level[3]) {
  uint8_t minLevel = 5;

  for (int i = 0; i < 3; i++) {
    if (level[i] > 0 && level[i] < minLevel) {
      minLevel = level[i];
    }
  }

  switch (minLevel) {
    case 1: currentBeepInterval = 150; break;
    case 2: currentBeepInterval = 400; break;
    case 3: currentBeepInterval = 800; break;
    case 4: currentBeepInterval = 1500; break;
    default: currentBeepInterval = 0; break;
  }

  if (currentBeepInterval > 0) {
    if (!beepState && millis() - lastBeepTime >= currentBeepInterval) {
      lastBeepTime = millis();
      digitalWrite(BUZZER_PIN, HIGH);  // Start beep
      beepState = true;
    }
    if (beepState && millis() - lastBeepTime >= 100) {
      digitalWrite(BUZZER_PIN, LOW);   // End beep
      beepState = false;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    beepState = false;
  }
}

void loop() {
  // Check CAN message
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

  // Always update display and buzzer with latest known levels
  updateDisplay(lastLevels);
  handleBuzzer(lastLevels);
}