#include <Arduino.h>
#include <driver/twai.h>

// ---- Pin assignments for SN65HVD230 ----
#define CAN_RX_PIN GPIO_NUM_4
#define CAN_TX_PIN GPIO_NUM_5

// ---- Matrix IDs (standard 11-bit, NOT extended) ----
const uint32_t CMD_ID = 0x169; // VCU_SES_Req
const uint32_t FBK_ID = 0x201; // SES_Status
const int CYCLE_TIME = 20;     // 20ms transmit requirement per spec

unsigned long lastCanTxTime = 0;
unsigned long lastPrintTime = 0;

float targetAngle = 0.0f;
bool gotFirstFeedback = false; // wait for ECU before commanding angle

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("Starting native ESP32-S3 TWAI controller (bench test mode)...");

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); // default per spec, change to 250KBITS if your ECU is set to 250K
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
        Serial.println("SUCCESS: CAN Bus Active.");
        Serial.println("Waiting for first SES_Status (0x201) feedback before sending commands...");
        Serial.println("Once feedback is seen, type a target angle (e.g., 25, -15.5, 0) and press Enter.");
    } else {
        Serial.println("ERROR: CAN initialization failed.");
        while (1);
    }
}

void loop() {
    unsigned long currentMillis = millis();

    // ------------------------------------------------------------------
    // 1. SERIAL MONITOR INPUT (Set Target Angle)
    // ------------------------------------------------------------------
    if (Serial.available() > 0) {
        float input = Serial.parseFloat();
        while (Serial.available() > 0) Serial.read(); // clear buffer

        if (input > 700.0f) input = 700.0f;   // physical limit per spec (±700°)
        if (input < -700.0f) input = -700.0f;

        targetAngle = input;
        Serial.print("\n>>> NEW TARGET: ");
        Serial.print(targetAngle);
        Serial.println(" degrees <<<");
    }

    // ------------------------------------------------------------------
    // 2. READ + RAW-PRINT CAN FEEDBACK (0x201 SES_Status)
    //    No decoding/math here on purpose - just confirm bytes arrive.
    // ------------------------------------------------------------------
    twai_message_t rx_msg;
    while (twai_receive(&rx_msg, 0) == ESP_OK) {
        if (rx_msg.identifier == FBK_ID) {
            gotFirstFeedback = true;
            Serial.print("RX 0x201 [");
            Serial.print(rx_msg.extd ? "EXT" : "STD");
            Serial.print(", len=");
            Serial.print(rx_msg.data_length_code);
            Serial.print("]: ");
            for (int i = 0; i < rx_msg.data_length_code; i++) {
                if (rx_msg.data[i] < 0x10) Serial.print("0");
                Serial.print(rx_msg.data[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        } else {
            // Print any other traffic too, helps confirm bus is alive even
            // if 0x201 specifically isn't showing up yet.
            Serial.print("RX other ID 0x");
            Serial.println(rx_msg.identifier, HEX);
        }
    }

    // ------------------------------------------------------------------
    // 3. STATUS HEARTBEAT every 500ms
    // ------------------------------------------------------------------
    if (currentMillis - lastPrintTime >= 500) {
        lastPrintTime = currentMillis;
        Serial.print("[status] Target: ");
        Serial.print(targetAngle);
        Serial.print("°  |  Feedback received yet: ");
        Serial.println(gotFirstFeedback ? "YES" : "NO");
    }

    // ------------------------------------------------------------------
    // 4. TRANSMIT COMMAND (0x169 VCU_SES_Req) EVERY 20ms
    //    Only starts once we've seen at least one feedback frame,
    //    per Syntree's power-on precaution.
    // ------------------------------------------------------------------
    if (gotFirstFeedback && (currentMillis - lastCanTxTime >= CYCLE_TIME)) {
        lastCanTxTime = currentMillis;

        twai_message_t tx_msg;
        tx_msg.identifier = CMD_ID;
        tx_msg.extd = 0;     // STANDARD 11-bit frame (this was the main bug)
        tx_msg.rtr = 0;
        tx_msg.data_length_code = 8;

        // Byte0: bit0 = calibration enable (0=off), bit1 = angle control enable (1=on)
        tx_msg.data[0] = 0x02;

        // Bytes 1-2: target angle, Motorola/big-endian, offset -3000, res 0.1
        uint16_t rawAngleData = (uint16_t)((targetAngle + 3000.0f) / 0.1f + 0.5f); // +0.5 for rounding
        tx_msg.data[1] = (rawAngleData >> 8) & 0xFF; // High byte
        tx_msg.data[2] = rawAngleData & 0xFF;        // Low byte

        // Bytes 3-7: left at 0 for bench test (no speed limit / no rollcount / no checksum enabled)
        tx_msg.data[3] = 0x00;
        tx_msg.data[4] = 0x00;
        tx_msg.data[5] = 0x00;
        tx_msg.data[6] = 0x00;
        tx_msg.data[7] = 0x00;

        twai_transmit(&tx_msg, 0);
    }
}
