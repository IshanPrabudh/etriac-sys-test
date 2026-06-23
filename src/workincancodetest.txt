#include <Arduino.h>
#include <driver/twai.h>

#define CAN_RX_PIN GPIO_NUM_4
#define CAN_TX_PIN GPIO_NUM_5

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== FINAL CAN TEST: BROADCASTING 0x169 ===");

    // Using NO_ACK mode to guarantee the ESP32 pushes data to the wires
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NO_ACK);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
        Serial.println("SUCCESS: TWAI Started. Blasting data...");
    } else {
        Serial.println("ERROR: TWAI Failed to start.");
        while(1);
    }
}

void loop() {
    twai_message_t tx_msg;
    tx_msg.identifier = 0x169;
    tx_msg.extd = 0; // Standard 11-bit ID
    tx_msg.rtr = 0;
    tx_msg.data_length_code = 8;

    // A static test payload
    tx_msg.data[0] = 0x02; 
    tx_msg.data[1] = 0x75; 
    tx_msg.data[2] = 0x30;
    tx_msg.data[3] = 0xAA; // Adding some distinct bytes to spot easily
    tx_msg.data[4] = 0xBB;
    tx_msg.data[5] = 0xCC;
    tx_msg.data[6] = 0x00;
    tx_msg.data[7] = 0x00;

    twai_transmit(&tx_msg, pdMS_TO_TICKS(5));
    
    // Print a visual heartbeat to the Serial Monitor
    Serial.print(".");
    
    delay(100); // Wait 100ms (10 messages per second)
}