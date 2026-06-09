#include <Arduino.h>
#include <CAN.h>

// Define our native interface pins
#define CAN_RX_PIN 4
#define CAN_TX_PIN 5

// Example: Target ID from your documentation (e.g., 0x10262B27)
// This uses a J1939-style 29-bit extended frame format
const uint32_t TARGET_CAN_ID = 0x10262B27; 

unsigned long lastTxTime = 0;
const int txInterval = 100; // Fixed 100ms send loop required by spec

void setup() {
    Serial.begin(115200);
    while(!Serial);

    Serial.println("Initializing Internal ESP32-S3 TWAI/CAN Controller...");

    // Set custom pins for internal CAN controller
    CAN.setPins(CAN_RX_PIN, CAN_TX_PIN);

    // Start CAN bus at 500 Kbps (Change to 250000 if your unit uses the 250K option)
    if (CAN.begin(500000)) {
        Serial.println("SUCCESS: Native CAN interface initialized successfully!");
    } else {
        Serial.println("ERROR: Failed to initialize CAN controller. Restarting...");
        while (1);
    }
}

void loop() {
    // Maintain strict 100ms transmission pacing to prevent communication timeouts
    if (millis() - lastTxTime >= txInterval) {
        lastTxTime = millis();

        // Begin an Extended (29-bit) CAN packet frame
        if (CAN.beginExtendedPacket(TARGET_CAN_ID)) {
            
            // Build the 8-byte payload based on your target specifications
            CAN.write(0x01); // Byte 0: Enable Working Mode (01)
            CAN.write(0x00); // Byte 1: Voltage instruction payload
            CAN.write(0x00); // Byte 2: Voltage instruction payload
            CAN.write(0x00); // Byte 3: Current instruction payload
            CAN.write(0x00); // Byte 4: Current instruction payload
            CAN.write(0xFF); // Byte 5: Reserved default pad byte
            CAN.write(0xFF); // Byte 6: Reserved default pad byte
            CAN.write(0x00); // Byte 7: Reset flags (No reset)
            
            // Push out to physical bus lines
            CAN.endPacket();
            
            Serial.println("CAN Frame successfully broadcast to network lines.");
        } else {
            Serial.println("Failed to queue packet frame.");
        }
    }

    // Check if any incoming frames are received back from other nodes
    int packetSize = CAN.parsePacket();
    if (packetSize) {
        Serial.print("Received Frame! ID: 0x");
        Serial.print(CAN.packetId(), HEX);
        Serial.print(" | Data length: ");
        Serial.println(packetSize);
        
        // Print out received message data bytes
        while (CAN.available()) {
            Serial.print("0x");
            Serial.print(CAN.read(), HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}