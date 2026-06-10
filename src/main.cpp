#include <Arduino.h>
#include <driver/twai.h>
#include <esp_err.h>
#include <math.h>

// Explicit pin assignments for your SN65HVD230 setup
#define CAN_RX_PIN GPIO_NUM_4
#define CAN_TX_PIN GPIO_NUM_5

// Target ID and cycle definitions from specification sheets
const uint32_t SES_CMD_ID = 0x169;
const int CAN_CYCLE_TIME = 20; // 20ms update requirement

unsigned long lastCanTxTime = 0;
unsigned long lastPositionToggleTime = 0;
bool targetIsFifteenDegrees = false;

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("Initializing native ESP32-S3 TWAI controller...");

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.println("CRITICAL ERROR: Failed to install TWAI driver.");
        while (1);
    }

    if (twai_start() != ESP_OK) {
        Serial.println("CRITICAL ERROR: Failed to start TWAI controller.");
        while (1);
    }

    Serial.println("SUCCESS: Internal controller linked to transceiver!");
}

/**
 * Packs and transmits a single 8-byte steering command frame
 * @param physicalAngle target orientation offset value in degrees
 */
void transmitSteeringAngle(float physicalAngle) {
    twai_message_t message;
    message.identifier = SES_CMD_ID;
    message.extd = 1;
    message.rtr = 0;
    message.data_length_code = 8;
    message.data[0] = 0x02;

    uint16_t rawAngleData = 0;
    if (physicalAngle >= 0.0f) {
        rawAngleData = (uint16_t)((physicalAngle + 3000.0f) / 0.1f);
    } else {
        rawAngleData = (uint16_t)((3000.0f - fabsf(physicalAngle)) / 0.1f);
    }

    message.data[1] = (rawAngleData >> 8) & 0xFF;
    message.data[2] = rawAngleData & 0xFF;
    message.data[3] = 0x00;
    message.data[4] = 0x00;
    message.data[5] = 0x00;
    message.data[6] = 0x00;
    message.data[7] = 0x00;

    if (twai_transmit(&message, pdMS_TO_TICKS(10)) != ESP_OK) {
        Serial.println("WARNING: CAN transmit failed.");
    }
}

void loop() {
    unsigned long currentMillis = millis();

    // Routine 1: Toggle the targeted structural position target every 4000ms (4 seconds)
    if (currentMillis - lastPositionToggleTime >= 4000) {
        lastPositionToggleTime = currentMillis;
        targetIsFifteenDegrees = !targetIsFifteenDegrees;
        
        if (targetIsFifteenDegrees) {
            Serial.println("COMMAND TARGET CHANGED: Rotating 15 Degrees Right...");
        } else {
            Serial.println("COMMAND TARGET CHANGED: Returning back to 0 Degree Start Position...");
        }
    }

    // Routine 2: Fire execution commands continuously on a strict 20ms baseline cycle
    if (currentMillis - lastCanTxTime >= CAN_CYCLE_TIME) {
        lastCanTxTime = currentMillis;
        
        float targetAngle = targetIsFifteenDegrees ? 15.0 : 0.0;
        transmitSteeringAngle(targetAngle);
    }
}[[-]]