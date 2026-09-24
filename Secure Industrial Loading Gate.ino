#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// Pin Definitions
// =====================================================

#define PIR_PIN       27
#define TRIG_PIN       5
#define ECHO_PIN      18
#define POT_PIN       34
#define SERVO_PIN     13
#define BUZZER_PIN    25

#define OLED_SDA      21
#define OLED_SCL      22

// =====================================================
// OLED Configuration
// =====================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);

// =====================================================
// Gate Positions
// =====================================================

#define GATE_CLOSED    0
#define GATE_OPEN     90

// =====================================================
// Safety
// =====================================================

#define SAFETY_DISTANCE 20.0

// =====================================================
// Servo Movement
// =====================================================

const unsigned long SERVO_STEP_INTERVAL = 30;

Servo gateServo;

int currentAngle = GATE_CLOSED;

unsigned long lastServoMoveTime = 0;

// =====================================================
// Gate States
// =====================================================

enum GateState {

    CLOSED,
    OPENING,
    OPEN_HOLD,
    CLOSING,
    SAFETY_HALT,
    AUTOMATIC_RETREAT,
    WAIT_FOR_CLEAR
};

/**
 * @brief Defines the operating mode of the loading gate.
 */
enum OperatingMode {
  MODE_AUTO,
  MODE_MANUAL
};

OperatingMode currentMode = MODE_AUTO;

// =====================================================
// UART Command Buffer
// =====================================================

char commandBuffer[32];
byte commandIndex = 0;

GateState currentState = CLOSED;

// =====================================================
// Timing
// =====================================================

unsigned long holdStartTime = 0;
unsigned long holdOpenTime = 5000;

unsigned long lastBuzzerTime = 0;

unsigned long lastDisplayUpdate = 0;

// =====================================================
// Current Distance
// =====================================================

float currentDistance = 999.0;


// =====================================================
// Function: Get State Name
// =====================================================

/**
 * @brief Converts the current gate state into readable text.
 *
 * @param None This function does not require parameters.
 * @return Text representation of the current gate state.
 */
const char* getStateName() {

    switch (currentState) {

        case CLOSED:
            return "CLOSED";

        case OPENING:
            return "OPENING";

        case OPEN_HOLD:
            return "OPEN/HOLD";

        case CLOSING:
            return "CLOSING";

        case SAFETY_HALT:
            return "SAFETY HALT";

        case AUTOMATIC_RETREAT:
            return "AUTO RETREAT";

        case WAIT_FOR_CLEAR:
            return "WAIT CLEAR";

        default:
            return "UNKNOWN";
    }
}


// =====================================================
// Function: Measure Distance
// =====================================================

/**
 * @brief Measures distance using the HC-SR04 ultrasonic sensor.
 *
 * @param None This function does not require parameters.
 * @return Distance measured in centimetres.
 */
float measureDistance() {

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration =
        pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0) {

        return 999.0;
    }

    float distance =
        duration * 0.0343 / 2.0;

    return distance;
}


// =====================================================
// Function: Calculate Hold Time
// =====================================================

/**
 * @brief Converts the potentiometer value into a hold-open delay.
 *
 * @param None This function does not require parameters.
 * @return Hold-open delay in milliseconds.
 */
unsigned long calculateHoldTime() {

    int adcValue = analogRead(POT_PIN);

    int delaySeconds =
        map(adcValue, 0, 4095, 3, 10);

    return delaySeconds * 1000UL;
}


// =====================================================
// Function: Update OLED
// =====================================================

/**
 * @brief Updates the OLED display according to the current system state.
 *
 * @param None This function does not require parameters.
 * @return None.
 */
void updateOLED() {

    display.clearDisplay();

    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);

    display.setCursor(0, 0);

    // -----------------------------------------------
    // Normal Closed
    // -----------------------------------------------

    if (currentState == CLOSED) {

        display.println("LOADING GATE");
        display.println();
        display.print("MODE: ");
        display.println(currentMode == MODE_AUTO ? "AUTO" : "MANUAL");
        display.print("GATE: ");
        display.println("CLOSED");
    }

    // -----------------------------------------------
    // Opening
    // -----------------------------------------------

    else if (currentState == OPENING) {

        display.println("VEHICLE DETECTED");
        display.println();
        display.println("GATE:");
        display.println("OPENING");
    }

    // -----------------------------------------------
    // Open / Hold
    // -----------------------------------------------

    else if (currentState == OPEN_HOLD) {

        display.println("LOADING GATE");
        display.println();
        display.print("MODE: ");
        display.println(currentMode == MODE_AUTO ? "AUTO" : "MANUAL");
        display.println("GATE: OPEN");

        display.print("HOLD: ");
        display.print(holdOpenTime / 1000);
        display.println("s");
    }

    // -----------------------------------------------
    // Closing
    // -----------------------------------------------

    else if (currentState == CLOSING) {

        display.println("LOADING GATE");
        display.println();
        display.print("MODE: ");
        display.println(currentMode == MODE_AUTO ? "AUTO" : "MANUAL");
        display.println("GATE: CLOSING");

        display.print("DIST: ");
        display.print(currentDistance, 1);
        display.println("cm");
    }

    // -----------------------------------------------
    // Safety Halt
    // -----------------------------------------------

    else if (currentState == SAFETY_HALT) {

        display.println("!!! SAFETY !!!");
        display.println();

        display.println("OBSTRUCTION");

        display.print("DIST: ");
        display.print(currentDistance, 1);
        display.println(" cm");

        display.println("GATE HALTED");
    }

    // -----------------------------------------------
    // Automatic Retreat
    // -----------------------------------------------

    else if (currentState == AUTOMATIC_RETREAT) {

        display.println("SAFETY RECOVERY");
        display.println();

        display.println("OBSTRUCTION");
        display.println("DETECTED");

        display.println();
        display.println("GATE OPENING");
    }

    // -----------------------------------------------
    // Wait for Clear
    // -----------------------------------------------

    else if (currentState == WAIT_FOR_CLEAR) {

        display.println("PATH BLOCKED");
        display.println();

        display.println("GATE: OPEN");

        display.print("DIST: ");
        display.print(currentDistance, 1);
        display.println(" cm");

        display.println();
        display.println("WAITING CLEAR");
    }

    display.display();
}


// =====================================================
// Function: Move Servo One Step
// =====================================================

/**
 * @brief Moves the servo gradually toward a target position.
 *
 * @param targetAngle Desired servo angle.
 * @return True when the target angle has been reached.
 */
bool moveServoTo(int targetAngle) {

    unsigned long currentTime = millis();

    if (currentTime - lastServoMoveTime <
        SERVO_STEP_INTERVAL) {

        return false;
    }

    lastServoMoveTime = currentTime;

    if (currentAngle < targetAngle) {

        currentAngle++;

        gateServo.write(currentAngle);
    }

    else if (currentAngle > targetAngle) {

        currentAngle--;

        gateServo.write(currentAngle);
    }

    return currentAngle == targetAngle;
}


// =====================================================
// Function: Start Opening
// =====================================================

/**
 * @brief Starts opening the loading gate.
 *
 * @param None This function does not require parameters.
 * @return None.
 */
void startOpening() {

    currentState = OPENING;

    Serial.println("Gate OPENING");
}


// =====================================================
// Function: Start Closing
// =====================================================

/**
 * @brief Starts closing the loading gate.
 *
 * @param None This function does not require parameters.
 * @return None.
 */
void startClosing() {

    currentState = CLOSING;

    Serial.println("Gate CLOSING");
}


// =====================================================
// Function: Trigger Safety
// =====================================================

/**
 * @brief Stops the gate because an obstruction was detected.
 *
 * @param distance Current measured distance in centimetres.
 * @return None.
 */
void triggerSafety(float distance) {

    currentState = SAFETY_HALT;

    Serial.println();
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("!!! SAFETY HALT !!!");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!");

    Serial.print("Obstruction detected at ");
    Serial.print(distance);
    Serial.println(" cm");

    Serial.print("Gate stopped at angle: ");
    Serial.println(currentAngle);

    tone(BUZZER_PIN, 2000);
    lastBuzzerTime = millis();
}


// =====================================================
// Function: Start Automatic Retreat
// =====================================================

/**
 * @brief Starts automatic retreat toward the open position.
 *
 * @param None This function does not require parameters.
 * @return None.
 */
void startAutomaticRetreat() {

    digitalWrite(BUZZER_PIN, LOW);

    currentState = AUTOMATIC_RETREAT;

    Serial.println(
        "Automatic safety recovery started."
    );

    Serial.println(
        "Gate RETREATING to OPEN position."
    );
}



// =====================================================
// Function: Handle UART Command
// =====================================================

/**
 * @brief Processes a complete UART command.
 *
 * @param command Command received from the Serial Monitor.
 * @return None.
 */
void handleCommand(const char* command) {

    if (strcmp(command, "MANUAL") == 0) {

        currentMode = MODE_MANUAL;

        Serial.println();
        Serial.println("================================");
        Serial.println("MANUAL MODE ENABLED");
        Serial.println("Automatic PIR control disabled.");
        Serial.println("Use OPEN or CLOSE commands.");
        Serial.println("================================");

        return;
    }

    if (strcmp(command, "AUTO") == 0) {

        currentMode = MODE_AUTO;

        if (currentState == OPEN_HOLD) {
            holdStartTime = millis();
            holdOpenTime = calculateHoldTime();
        }

        Serial.println();
        Serial.println("================================");
        Serial.println("AUTONOMOUS MODE ENABLED");
        Serial.println("PIR control active.");
        Serial.println("================================");

        return;
    }

    if (strcmp(command, "OPEN") == 0) {

        if (currentState == SAFETY_HALT ||
            currentState == AUTOMATIC_RETREAT ||
            currentState == WAIT_FOR_CLEAR) {

            Serial.println(
                "OPEN command unavailable during safety recovery."
            );
            return;
        }

        Serial.println("UART command: OPEN");

        if (currentState == CLOSED ||
            currentState == CLOSING) {

            startOpening();
        }
        else if (currentState == OPEN_HOLD) {

            Serial.println("Gate is already open.");
        }

        return;
    }

    if (strcmp(command, "CLOSE") == 0) {

        if (currentState == SAFETY_HALT ||
            currentState == AUTOMATIC_RETREAT) {

            Serial.println(
                "CLOSE command unavailable during safety recovery."
            );
            return;
        }

        Serial.println("UART command: CLOSE");

        if (currentDistance < SAFETY_DISTANCE) {

            Serial.println("CLOSE command rejected.");
            Serial.print("Path blocked at ");
            Serial.print(currentDistance);
            Serial.println(" cm.");
            Serial.println("Gate remains OPEN.");
            Serial.println("Waiting for obstruction to clear.");

            currentState = WAIT_FOR_CLEAR;
        }
        else if (currentState == OPEN_HOLD) {

            Serial.println("Path clear.");
            startClosing();
        }
        else if (currentState == CLOSED) {

            Serial.println("Gate is already closed.");
        }

        return;
    }

    if (strcmp(command, "STATUS") == 0) {

        Serial.println();
        Serial.println("========== STATUS ==========");

        Serial.print("Mode: ");
        Serial.println(
            currentMode == MODE_AUTO ? "AUTO" : "MANUAL"
        );

        Serial.print("Gate State: ");
        Serial.println(getStateName());

        Serial.print("Servo Angle: ");
        Serial.println(currentAngle);

        Serial.print("Distance: ");
        Serial.print(currentDistance);
        Serial.println(" cm");

        Serial.print("Hold Delay: ");
        Serial.print(holdOpenTime / 1000);
        Serial.println(" seconds");

        Serial.println("============================");

        return;
    }

    if (strcmp(command, "HELP") == 0) {

        Serial.println();
        Serial.println("========== COMMANDS ==========");
        Serial.println("MANUAL - Enter Manual Mode");
        Serial.println("OPEN   - Open gate");
        Serial.println("CLOSE  - Close gate");
        Serial.println("AUTO   - Return to Auto Mode");
        Serial.println("STATUS - Show system status");
        Serial.println("HELP   - Show commands");
        Serial.println("==============================");

        return;
    }

    Serial.print("Unknown command: ");
    Serial.println(command);
    Serial.println("Type HELP for available commands.");
}


// =====================================================
// Function: Process UART Commands
// =====================================================

/**
 * @brief Reads UART input without blocking the main control loop.
 *
 * @param None This function does not require parameters.
 * @return None.
 */
void processSerialCommands() {

    while (Serial.available() > 0) {

        char receivedChar = Serial.read();

        if (receivedChar == '\n' ||
            receivedChar == '\r') {

            if (commandIndex > 0) {

                commandBuffer[commandIndex] = '\0';

                handleCommand(commandBuffer);

                commandIndex = 0;
            }
        }
        else {

            if (commandIndex <
                sizeof(commandBuffer) - 1) {

                if (receivedChar >= 'a' &&
                    receivedChar <= 'z') {

                    receivedChar =
                        receivedChar - 'a' + 'A';
                }

                commandBuffer[commandIndex] =
                    receivedChar;

                commandIndex++;
            }
        }
    }
}


// =====================================================
// Function: Setup
// =====================================================

/**
 * @brief Initializes the ESP32, sensors, servo, buzzer and OLED.
 *
 * @param None This function does not require parameters.
 * @return None.
 */
void setup() {

    Serial.begin(115200);

    // Sensor pins
    pinMode(PIR_PIN, INPUT);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    pinMode(POT_PIN, INPUT);

    // Buzzer
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);

    // Servo
    gateServo.attach(SERVO_PIN);

    currentAngle = GATE_CLOSED;

    gateServo.write(currentAngle);

    currentState = CLOSED;

    // OLED
    Wire.begin(
        OLED_SDA,
        OLED_SCL
    );

    if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
    )) {

        Serial.println("OLED initialization failed!");

        while (true) {
        }
    }

    updateOLED();

    Serial.println();
    Serial.println("================================");
    Serial.println("SECURE LOADING GATE");
    Serial.println("Stage 6 Firmware");
    Serial.println("================================");

    Serial.println("Gate state: CLOSED");
    Serial.println("Safety threshold: 20 cm");
    Serial.println("Automatic recovery: ENABLED");
    Serial.println("OLED: ENABLED");
    Serial.println("UART: ENABLED");
    Serial.println();
    Serial.println("Type HELP for available commands.");
}


// =====================================================
// Function: Main Loop
// =====================================================

/**
 * @brief Continuously manages gate operation, safety and OLED updates.
 *
 * @param None This function does not require parameters.
 * @return None.
 */
void loop() {

    unsigned long currentTime = millis();

    // Process UART commands without blocking the main loop.
    processSerialCommands();

    int motionDetected =
        digitalRead(PIR_PIN);

    currentDistance =
        measureDistance();


    // =================================================
    // SAFETY MONITORING
    // =================================================

    if (currentState == CLOSING) {

        if (currentDistance < SAFETY_DISTANCE) {

            triggerSafety(currentDistance);
        }
    }


    // =================================================
    // CLOSED
    // =================================================

    if (currentState == CLOSED) {

        // PIR control is active only in AUTO mode.
        if (currentMode == MODE_AUTO &&
            motionDetected == HIGH) {

            Serial.println(
                "Vehicle detected by PIR"
            );

            startOpening();
        }
    }


    // =================================================
    // OPENING
    // =================================================

    else if (currentState == OPENING) {

        bool fullyOpen =
            moveServoTo(GATE_OPEN);

        if (fullyOpen) {

            currentState = OPEN_HOLD;

            holdStartTime = millis();

            holdOpenTime =
                calculateHoldTime();

            Serial.print(
                "Gate OPEN | Hold time: "
            );

            Serial.print(
                holdOpenTime / 1000
            );

            Serial.println(" seconds");
        }
    }


    // =================================================
    // OPEN / HOLD
    // =================================================

    else if (currentState == OPEN_HOLD) {

        // Automatic hold timer is only active in AUTO mode.
        if (currentMode == MODE_AUTO &&
            currentTime - holdStartTime >=
            holdOpenTime) {

            Serial.println(
                "Hold time expired"
            );

            // Check path before closing.
            if (currentDistance <
                SAFETY_DISTANCE) {

                Serial.println(
                    "Path blocked before closing."
                );

                Serial.print(
                    "Distance: "
                );

                Serial.print(
                    currentDistance
                );

                Serial.println(" cm");

                Serial.println(
                    "Gate remains OPEN."
                );

                Serial.println(
                    "Waiting for obstruction to clear."
                );

                currentState =
                    WAIT_FOR_CLEAR;
            }

            else {

                startClosing();
            }
        }
    }


    // =================================================
    // CLOSING
    // =================================================

    else if (currentState == CLOSING) {

        bool fullyClosed =
            moveServoTo(GATE_CLOSED);

        if (fullyClosed) {

            currentState = CLOSED;

            Serial.println(
                "Gate CLOSED"
            );
        }
    }


    // =================================================
    // SAFETY HALT
    // =================================================

    else if (currentState == SAFETY_HALT) {

        if (millis() - lastBuzzerTime >=
            500) {

            lastBuzzerTime = currentTime;

            noTone(BUZZER_PIN);

            if (currentAngle ==
                GATE_OPEN) {

                Serial.println(
                    "Gate is already fully open."
                );

                Serial.println(
                    "Waiting for obstruction to clear."
                );

                currentState =
                    WAIT_FOR_CLEAR;
            }

            else {

                startAutomaticRetreat();
            }
        }
    }


    // =================================================
    // AUTOMATIC RETREAT
    // =================================================

    else if (currentState ==
             AUTOMATIC_RETREAT) {

        bool fullyOpen =
            moveServoTo(GATE_OPEN);

        if (fullyOpen) {

            Serial.println(
                "Gate fully open after safety recovery."
            );

            currentState =
                WAIT_FOR_CLEAR;

            Serial.println(
                "Waiting for obstruction to clear."
            );
        }
    }


    // =================================================
    // WAIT FOR CLEAR PATH
    // =================================================

    else if (currentState ==
             WAIT_FOR_CLEAR) {

        if (currentDistance >=
            SAFETY_DISTANCE) {

            Serial.println(
                "Path is clear."
            );

            Serial.println(
                "Returning to normal operation."
            );

            Serial.println(
                "Gate CLOSING after safety recovery."
            );

            currentState = CLOSING;
        }
    }


    // =================================================
    // OLED UPDATE
    // =================================================

    if (currentTime - lastDisplayUpdate >= 200) {

        lastDisplayUpdate = currentTime;

        updateOLED();
    }
}