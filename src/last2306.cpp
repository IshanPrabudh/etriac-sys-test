#include <Arduino.h>
#include <driver/twai.h>

// ---- Pin assignments ----
#define CAN_RX_PIN GPIO_NUM_4
#define CAN_TX_PIN GPIO_NUM_5

float targetAngle = 0.0f;
unsigned long lastCanTxTime = 0;
unsigned long lastHeartbeatTime = 0;
const int CYCLE_TIME = 20; // 20ms broadcast loop

void setup() {
    Serial.begin(115200);
    delay(2000);
    while (!Serial);

    Serial.println("\n=== ANGLE TO CAN: BULLETPROOF MODE ===");
    Serial.println("Enter a target angle (e.g., 25.5, -10, 0) and press Enter.");

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
    unsigned long currentMillis = millis();

    // ------------------------------------------------------------------
    // 1. SERIAL MONITOR INPUT
    // ------------------------------------------------------------------
    if (Serial.available() > 0) {
        String inputStr = Serial.readStringUntil('\n');
        inputStr.trim(); 
        
        if (inputStr.length() > 0) {
            targetAngle = inputStr.toFloat();

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
    if (currentMillis - lastCanTxTime >= CYCLE_TIME) {
        lastCanTxTime = currentMillis;

        twai_message_t tx_msg;
        
        // --- THE CRITICAL FIX ---
        // Forces all hidden configuration memory to 0 to prevent driver panic.
        tx_msg.flags = 0; 
        // ------------------------

        tx_msg.identifier = 0x169;
        tx_msg.data_length_code = 8;

        // Byte 0: Angle Control Enable (0x02)
        tx_msg.data[0] = 0x02; 

        // Bytes 1 & 2: Calculate target angle
        uint16_t rawAngleData = (uint16_t)((targetAngle + 3000.0f) / 0.1f + 0.5f); 
        tx_msg.data[1] = (rawAngleData >> 8) & 0xFF; // High Byte
        tx_msg.data[2] = rawAngleData & 0xFF;        // Low Byte

        // Bytes 3-7: Zeroed out
        tx_msg.data[3] = 0x00;
        tx_msg.data[4] = 0x00;
        tx_msg.data[5] = 0x00;
        tx_msg.data[6] = 0x00;
        tx_msg.data[7] = 0x00;

        // Transmit and catch any internal software rejection
        esp_err_t result = twai_transmit(&tx_msg, pdMS_TO_TICKS(5));
        if (result != ESP_OK) {
            Serial.println("[!] ERROR: ESP32 TWAI Driver rejected the message!");
        }
    }

    // ------------------------------------------------------------------
    // 3. HEARTBEAT PRINT (Fixed to be reliable)
    // ------------------------------------------------------------------
    if (currentMillis - lastHeartbeatTime >= 1000) {
        lastHeartbeatTime = currentMillis;
        
        Serial.print("[Heartbeat] Target: ");
        Serial.print(targetAngle);
        Serial.println("° | ESP32 is actively transmitting...");
    }
}