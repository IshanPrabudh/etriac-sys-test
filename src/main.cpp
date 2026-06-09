#include <Arduino.h>
#include "driver/twai.h"

// Define our native interface pins
static const gpio_num_t CAN_RX_PIN = GPIO_NUM_4;
static const gpio_num_t CAN_TX_PIN = GPIO_NUM_5;

// Example: Target ID from your documentation (e.g., 0x10262B27)
// This uses a 29-bit extended frame format
const uint32_t TARGET_CAN_ID = 0x10262B27;

unsigned long lastTxTime = 0;
const int txInterval = 100; // Fixed 100ms send loop required by spec

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("Initializing Internal ESP32-S3 TWAI/CAN Controller...");

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
        Serial.println("SUCCESS: Native TWAI interface initialized successfully!");
    } else {
        Serial.println("ERROR: Failed to initialize TWAI controller. Halting...");
        while (1);
    }
}

void loop() {
    if (millis() - lastTxTime >= txInterval) {
        lastTxTime = millis();

        twai_message_t message = {};
        message.identifier = TARGET_CAN_ID;
        message.extd = 1;
        message.data_length_code = 8;
        message.data[0] = 0x01; // Byte 0: Enable Working Mode (01)
        message.data[1] = 0x00; // Byte 1: Voltage instruction payload
        message.data[2] = 0x00; // Byte 2: Voltage instruction payload
        message.data[3] = 0x00; // Byte 3: Current instruction payload
        message.data[4] = 0x00; // Byte 4: Current instruction payload
        message.data[5] = 0xFF; // Byte 5: Reserved default pad byte
        message.data[6] = 0xFF; // Byte 6: Reserved default pad byte
        message.data[7] = 0x00; // Byte 7: Reset flags (No reset)

        if (twai_transmit(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
            Serial.println("TWAI Frame successfully broadcast to network lines.");
        } else {
            Serial.println("Failed to queue packet frame.");
        }
    }

    twai_message_t rx_message;
    if (twai_receive(&rx_message, 0) == ESP_OK) {
        Serial.print("Received Frame! ID: 0x");
        Serial.print(rx_message.identifier, HEX);
        Serial.print(" | Data length: ");
        Serial.println(rx_message.data_length_code);

        for (uint8_t i = 0; i < rx_message.data_length_code; i++) {
            Serial.print("0x");
            if (rx_message.data[i] < 0x10) {
                Serial.print("0");
            }
            Serial.print(rx_message.data[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}
