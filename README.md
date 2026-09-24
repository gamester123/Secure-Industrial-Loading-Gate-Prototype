# Secure Industrial Loading Gate Prototype

An ESP32-based embedded system prototype developed for the COMP50069 – Hardware, Microcontrollers and Sensors module.

The system demonstrates an automated industrial loading gate with vehicle detection, manual UART control, adjustable hold-open timing, obstacle detection, and automatic safety recovery.

## Project Overview

The prototype is designed for a logistics and warehouse environment where a loading gate needs to operate automatically while maintaining safety during gate movement.

The ESP32 processes inputs from the PIR sensor, ultrasonic sensor, and potentiometer and controls the servo motor, buzzer, and I2C OLED display.

## Main Features

- Automatic gate operation using a PIR sensor
- Manual gate control through UART commands
- Adjustable gate hold-open delay using a potentiometer
- Servo motor control using PWM
- Ultrasonic obstacle detection
- Safety halt when an obstruction is detected below 20 cm during closing
- Automatic retreat to the open position after a safety halt
- Wait-for-clear safety behaviour
- I2C OLED status display
- Timer-based control using `millis()`
- Safety monitoring in both Auto and Manual modes

## Hardware

- ESP32 development board
- PIR sensor
- HC-SR04 ultrasonic sensor
- Potentiometer
- Servo motor
- Buzzer
- I2C OLED display
- 1 kΩ resistor
- 2 kΩ resistor
- Breadboard and jumper wires

## GPIO Configuration

| Component | ESP32 GPIO |
|---|---:|
| PIR Sensor | GPIO 27 |
| HC-SR04 TRIG | GPIO 5 |
| HC-SR04 ECHO | GPIO 18 |
| Potentiometer | GPIO 34 |
| Servo Motor | GPIO 13 |
| Buzzer | GPIO 25 |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| UART TX | GPIO 17 |
| UART RX | GPIO 16 |

The HC-SR04 ECHO signal is connected to GPIO 18 through a 1 kΩ and 2 kΩ voltage divider.

## Operating Modes

### Auto Mode

Auto Mode is the default operating mode.

When the PIR sensor detects movement while the gate is closed, the gate automatically opens. The servo moves the gate from 0° to 90°.

After reaching the open position, the gate remains open for a configurable hold-open period. The potentiometer adjusts this delay between approximately 3 and 10 seconds.

When the hold period expires and the path is clear, the gate begins closing.

### Manual Mode

Manual Mode allows an operator to control the gate using UART commands through the Serial Monitor.

| Command | Function |
|---|---|
| `MANUAL` | Enter Manual Mode |
| `OPEN` | Open the gate |
| `CLOSE` | Close the gate |
| `AUTO` | Return to Auto Mode |
| `STATUS` | Display system status |
| `HELP` | Display available commands |

Safety monitoring remains active while Manual Mode is being used.

## Safety System

The ultrasonic sensor monitors the gate path while the gate is closing.

If an obstruction is detected below 20 cm during the closing process:

1. The gate stops immediately.
2. The system enters the Safety Halt state.
3. The buzzer provides a warning.
4. The OLED displays the safety condition.
5. The gate automatically retreats to the open position.
6. The system waits for the obstruction to clear.
7. Normal operation can then resume.

If an obstruction is already present before the gate begins closing, the gate remains open and waits for the path to become clear.

## Gate States

The system uses the following gate states:

- `CLOSED`
- `OPENING`
- `OPEN_HOLD`
- `CLOSING`
- `SAFETY_HALT`
- `AUTOMATIC_RETREAT`
- `WAIT_FOR_CLEAR`

Auto Mode and Manual Mode are operating modes that control how the gate receives commands.

## OLED Display

The I2C OLED provides information about the current system condition, including:

- Operating mode
- Gate state
- Servo position
- Hold-open delay
- Ultrasonic distance
- Safety condition

## Timing and Control

The firmware uses `millis()`-based timing for gate hold timing, servo movement timing, and OLED updates.

Servo movement is performed gradually, allowing the system to continue processing sensor inputs and UART commands during gate operation.

## Software Documentation

The source code contains Doxygen-style documentation for the implemented functions, including:

- `@brief`
- `@param`
- `@return`

This documents the purpose, parameters, and return values of the functions used by the firmware.

## Development and Testing

The system was developed using an ESP32-based design and initially tested using a Wokwi simulation before testing the physical hardware.

Testing covered:

- PIR vehicle detection
- Potentiometer hold-delay adjustment
- Servo movement
- Ultrasonic distance detection
- Safety halt
- Automatic retreat
- Wait-for-clear behaviour
- UART Manual Mode
- OLED status display
- Complete gate operation

## Project Context

**Module:** COMP50069 – Hardware, Microcontrollers and Sensors

**Scenario:** Secure Industrial Loading Gate

**Controller:** ESP32

**Project Type:** Embedded System Prototype
