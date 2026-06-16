#include <Arduino.h>
#include <driver/twai.h>
#include <esp_err.h>
#include <math.h>

// Explicit pin assignments for your SN65HVD230 setup
#define CAN_RX_PIN GPIO_NUM_4
#define CAN_TX_PIN GPIO_NUM_5

// Target ID and cycle definitions from specification sheets
const uint32_t SES_CMD_ID = 0x169; // Output Command ID
const uint32_t SES_FBK_ID = 0x201; // Input Feedback ID
const int CAN_CYCLE_TIME = 20;     // 20ms transmit requirement

// Safety Limits
const float MAX_ANGLE_LIMIT = 25.0f;

unsigned long lastCanTxTime = 0;
unsigned long lastPrintTime = 0;

float currentTargetAngle = 0.0f;
float currentFeedbackAngle = 0.0f;
bool targetReached = true; 
bool hardwareConnected = false; // Tracks if we are receiving live signals

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

    Serial.println("SUCCESS: Controller active! Local transmission running.");
    Serial.println("=================================================");
    Serial.println("READY: Enter a test angle (Max +/- 25):");
    Serial.println("=================================================");
}

/**
 * Packs and transmits a single 8-byte steering command frame
 */
void transmitSteeringAngle(float physicalAngle, uint16_t speedLimit) {
    twai_message_t message;
    message.identifier = SES_CMD_ID;
    message.extd = 1;
    message.rtr = 0;
    message.data_length_code = 8;
    
    message.data[0] = 0x02; // Enable angle control

    uint16_t rawAngleData = 0;
    if (physicalAngle >= 0.0f) {
        rawAngleData = (uint16_t)((physicalAngle + 3000.0f) / 0.1f);
    } else {
        rawAngleData = (uint16_t)((3000.0f - fabsf(physicalAngle)) / 0.1f);
    }
    
    message.data[1] = (rawAngleData >> 8) & 0xFF; 
    message.data[2] = rawAngleData & 0xFF;        

    message.data[3] = (speedLimit >> 8) & 0xFF;
    message.data[4] = speedLimit & 0xFF;

    message.data[5] = 0x00;
    message.data[6] = 0x00;
    message.data[7] = 0x00;

    twai_transmit(&message, pdMS_TO_TICKS(5));
}

void loop() {
    unsigned long currentMillis = millis();

    // ------------------------------------------------------------------
    // ROUTINE 1: Read Incoming CAN Messages (If Connected)
    // ------------------------------------------------------------------
    twai_message_t rx_msg;
    while (twai_receive(&rx_msg, 0) == ESP_OK) {
        if (rx_msg.identifier == SES_FBK_ID) {
            hardwareConnected = true; // We heard from the steering motor!
            uint16_t rawFeedback = (rx_msg.data[3] << 8) | rx_msg.data[4];
            currentFeedbackAngle = (rawFeedback * 0.1f) - 3000.0f;
        }
    }

    // ------------------------------------------------------------------
    // ROUTINE 2: Read Keyboard Input (Always Active)
    // ------------------------------------------------------------------
    if (Serial.available() > 0) {
        float inputAngle = Serial.parseFloat();
        while(Serial.available() > 0) Serial.read(); // Clear buffer
        
        // Safety Clamp: Restrict angle to +/- 25 degrees
        if (inputAngle > MAX_ANGLE_LIMIT) {
            inputAngle = MAX_ANGLE_LIMIT;
            Serial.println("\n[!] SAFETY LIMIT: Angle capped at +25.0 degrees.");
        } else if (inputAngle < -MAX_ANGLE_LIMIT) {
            inputAngle = -MAX_ANGLE_LIMIT;
            Serial.println("\n[!] SAFETY LIMIT: Angle capped at -25.0 degrees.");
        }

        currentTargetAngle = inputAngle;
        targetReached = false; 
        
        Serial.print("\n>>> NEW COMMAND: Target set to ");
        Serial.print(currentTargetAngle);
        Serial.println(" degrees.");
        Serial.println(">>> (Enter a new angle at any time to update) <<<");
    }

    // ------------------------------------------------------------------
    // ROUTINE 3: Monitor Feedback (Only if hardware is replying)
    // ------------------------------------------------------------------
    if (hardwareConnected && !targetReached) {
        if (currentMillis - lastPrintTime >= 250) {
            lastPrintTime = currentMillis;
            Serial.print("   Live Position: ");
            Serial.print(currentFeedbackAngle);
            Serial.println(" deg");
        }

        if (fabs(currentFeedbackAngle - currentTargetAngle) <= 0.5f) {
            targetReached = true;
            Serial.println("\n[✔] SUCCESS: Physical motor reached designated angle!");
        }
    }

    // ------------------------------------------------------------------
    // ROUTINE 4: Transmit Target Command (Strictly every 20ms)
    // ------------------------------------------------------------------
    if (currentMillis - lastCanTxTime >= CAN_CYCLE_TIME) {
        lastCanTxTime = currentMillis;
        transmitSteeringAngle(currentTargetAngle, 0); 
    }
}