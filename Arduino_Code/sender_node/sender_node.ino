#include <SPI.h>
#include <mcp2515.h>

#define NUM_SENSORS 3
const uint8_t sensorPin[NUM_SENSORS] = {4, 5, 6};

MCP2515 mcp2515(10);  // CS on D10
struct can_frame canMsg;

uint8_t lastLevels[NUM_SENSORS] = {0, 0, 0};

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Init CAN
  mcp2515.reset();
  mcp2515.setBitrate(CAN_125KBPS);
  mcp2515.setNormalMode();

  // CAN frame setup
  canMsg.can_id  = 0x100;
  canMsg.can_dlc = 2; // sending only 2 bytes

  // Init sensor pins
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensorPin[i], OUTPUT);
    digitalWrite(sensorPin[i], LOW);
  }
}

uint16_t readDistance(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(2);
  digitalWrite(pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(pin, LOW);

  pinMode(pin, INPUT);
  long duration = pulseIn(pin, HIGH, 30000L);
  return duration > 0 ? duration * 0.0343 / 2.0 : 0xFFFF;
}

int getLevel(uint16_t d) {
  if (d == 0xFFFF) return 0;
  if (d > 50) return 5;
  else if (d > 35) return 4;
  else if (d > 20) return 3;
  else if (d > 10) return 2;
  else return 1;
}

void loop() {
  uint8_t currentLevels[NUM_SENSORS];
  bool changed = false;

  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    uint16_t dist = readDistance(sensorPin[i]);
    currentLevels[i] = getLevel(dist);
    delay(50); // short wait between sensor reads

    Serial.print("Sensor ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(dist);
    Serial.print(" cm → Level ");
    Serial.println(currentLevels[i]);

    // Check for change
    if (currentLevels[i] != lastLevels[i]) {
      changed = true;
    }
  }

  if (changed) {
    // Pack 3 × 3-bit levels into 2 bytes
    uint16_t packed = ((currentLevels[0] & 0x07) << 6) |
                      ((currentLevels[1] & 0x07) << 3) |
                      (currentLevels[2] & 0x07);

    canMsg.data[0] = highByte(packed);
    canMsg.data[1] = lowByte(packed);

    mcp2515.sendMessage(&canMsg);
    Serial.println("CAN Message Sent");

    // Update previous levels
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
      lastLevels[i] = currentLevels[i];
    }
  } else {
    Serial.println("No change in levels – skipping CAN send");
  }

  delay(200); // main loop wait
}