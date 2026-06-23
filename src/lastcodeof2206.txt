#include <Arduino.h>
#include <driver/twai.h>

// ---- Pin assignments ----
#define CAN_RX_PIN GPIO_NUM_4
#define CAN_TX_PIN GPIO_NUM_5

float targetAngle = 0.0f;
unsigned long lastCanTxTime = 0;
const int CYCLE_TIME = 20; // 20ms broadcast loop

void setup() {
    Serial.begin(115200);
    delay(2000);
    while (!Serial);

    Serial.println("\n=== ANGLE TO CAN: FINAL BENCH TEST MODE ===");
    Serial.println("Enter a target angle (e.g., 25.5, -10, 0) and press Enter.");

    // Using NO_ACK for bench testing so it blasts data to the CANalyst-II analyzer.
    // NOTE: When you plug this into the real steering motor later, change NO_ACK to NORMAL!
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NO_ACK);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
        Serial.println("SUCCESS: CAN Active. Broadcasting 0x169 every 20ms.");
    } else {
        Serial.println("ERROR: CAN failed to start.");
        while(1);
    }
}

void loop() {
    // ------------------------------------------------------------------
    // 1. ROBUST SERIAL MONITOR INPUT (The Fix)
    // ------------------------------------------------------------------
    if (Serial.available() > 0) {
        // Read the entire line until the user hits Enter
        String inputStr = Serial.readStringUntil('\n');
        
        // Strip away hidden \r or \n or spaces that cause glitches
        inputStr.trim(); 
        
        // Only update the target if you actually typed a number
        if (inputStr.length() > 0) {
            targetAngle = inputStr.toFloat();

            // Limit the angle to physical specs (±700°)
            if (targetAngle > 700.0f) targetAngle = 700.0f;
            if (targetAngle < -700.0f) targetAngle = -700.0f;

            Serial.print("\n>>> New Angle Commanded: ");
            Serial.print(targetAngle);
            Serial.println(" degrees <<<");
        }
    }

    // ------------------------------------------------------------------
    // 2. TRANSMIT COMMAND (0x169) EVERY 20ms
    // ------------------------------------------------------------------
    if (millis() - lastCanTxTime >= CYCLE_TIME) {
        lastCanTxTime = millis();

        twai_message_t tx_msg;
        tx_msg.identifier = 0x169;
        tx_msg.extd = 0; // Standard 11-bit ID
        tx_msg.rtr = 0;
        tx_msg.data_length_code = 8;

        // Byte 0: Angle Control Enable (0x02)
        tx_msg.data[0] = 0x02; 

        // Bytes 1 & 2: Calculate target angle (Offset -3000, Res 0.1)
        uint16_t rawAngleData = (uint16_t)((targetAngle + 3000.0f) / 0.1f + 0.5f); 
        tx_msg.data[1] = (rawAngleData >> 8) & 0xFF; // High Byte
        tx_msg.data[2] = rawAngleData & 0xFF;        // Low Byte

        // Bytes 3-7: Zeroed out for safety
        tx_msg.data[3] = 0x00;
        tx_msg.data[4] = 0x00;
        tx_msg.data[5] = 0x00;
        tx_msg.data[6] = 0x00;
        tx_msg.data[7] = 0x00;

        twai_transmit(&tx_msg, pdMS_TO_TICKS(5));
        
        // Print a visual heartbeat to Serial Monitor every 1 second
        if (millis() % 1000 < CYCLE_TIME) {
            Serial.print("[Heartbeat] Target: ");
            Serial.print(targetAngle);
            Serial.print("° | Expected CAN Bytes (Hex): 02 ");
            if (tx_msg.data[1] < 0x10) Serial.print("0");
            Serial.print(tx_msg.data[1], HEX);
            Serial.print(" ");
            if (tx_msg.data[2] < 0x10) Serial.print("0");
            Serial.print(tx_msg.data[2], HEX);
            Serial.println(" 00 00 00 00 00");
        }
    }
}