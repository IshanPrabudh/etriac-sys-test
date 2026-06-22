#include <Arduino.h>

#define CAN_TX_PIN 5

void setup() {
    Serial.begin(115200);
    pinMode(CAN_TX_PIN, OUTPUT);
    Serial.println("Starting Raw TX Pin Test...");
}

void loop() {
    // 1. Force Recessive (Idle state)
    digitalWrite(CAN_TX_PIN, HIGH);
    Serial.println("TX HIGH: Bus should be IDLE. Multimeter difference = 0V.");
    delay(3000);

    // 2. Force Dominant (Active state)
    digitalWrite(CAN_TX_PIN, LOW);
    Serial.println("TX LOW: Bus should be ACTIVE. Multimeter difference = ~2.0V.");
    delay(3000);
}