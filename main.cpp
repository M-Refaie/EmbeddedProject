#include "mbed.h"

// =============================
//        Pin Declarations
// =============================

// Shift register control pins (connected to 74HC595 or similar)
DigitalOut latchPin(D4);  // ST_CP (latch pin) – toggled to update the output
DigitalOut clockPin(D7);  // SH_CP (clock pin) – used to shift bits in
DigitalOut dataPin(D8);   // DS (data pin) – sends data serially to shift register

// Input buttons with pull-up resistors (active LOW)
DigitalIn s1(A1, PullUp); // Button S1: resets the timer
DigitalIn s2(A2, PullUp); // Button S2: not used in this project
DigitalIn s3(A3, PullUp); // Button S3: shows potentiometer voltage when held

// Analog input from onboard potentiometer
AnalogIn potentiometer(A0);  // Reads voltage between 0V and 3.3V

// =============================
//     7-Segment Definitions
// =============================

// Segment patterns for digits 0–9 for **common anode** displays
// Each byte defines which segments are ON (inverted logic: 0 = ON)
// e.g., ~0x3F = 0xC0 means segments A–F ON for digit 0
const uint8_t digitPattern[10] = {
    static_cast<uint8_t>(~0x3F), // 0 → segments A-F
    static_cast<uint8_t>(~0x06), // 1 → segments B, C
    static_cast<uint8_t>(~0x5B), // 2 → segments A, B, D, E, G
    static_cast<uint8_t>(~0x4F), // 3 → segments A, B, C, D, G
    static_cast<uint8_t>(~0x66), // 4 → segments B, C, F, G
    static_cast<uint8_t>(~0x6D), // 5 → segments A, C, D, F, G
    static_cast<uint8_t>(~0x7D), // 6 → segments A, C, D, E, F, G
    static_cast<uint8_t>(~0x07), // 7 → segments A, B, C
    static_cast<uint8_t>(~0x7F), // 8 → all segments
    static_cast<uint8_t>(~0x6F)  // 9 → segments A, B, C, D, F, G
};

// Digit position control for 4-digit display (via shift register)
// Each value selects a digit (left to right)
const uint8_t digitPos[4] = { 0x01, 0x02, 0x04, 0x08 };



// =============================
//     Timekeeping Variables
// =============================

volatile int seconds = 0, minutes = 0;  // Global time counters
Ticker timerTicker;  // Periodic interrupt to increment time

// =============================
//       Time Update ISR
// =============================

// Called every second by the ticker interrupt
void updateTime() {
    if (++seconds >= 60) {  // If 60 seconds passed
        seconds = 0;
        if (++minutes >= 100) minutes = 0;  // Limit minutes to 99
    }
}

// =============================
//      Shift Register Output
// =============================

// Sends 8 bits MSB-first to the shift register
void shiftOut(uint8_t value) {
    for (int i = 7; i >= 0; --i) {
        dataPin = (value >> i) & 0x01;  // Set data bit
        clockPin = 1;  // Clock rising edge
        clockPin = 0;  // Clock falling edge
    }
}

// Sends 2 bytes to the shift register (segments and digit select)
void writeToShiftRegister(uint8_t segments, uint8_t digit) {
    latchPin = 0;             // Disable output while shifting
    shiftOut(segments);       // First shift segment pattern
    shiftOut(digit);          // Then shift digit selection
    latchPin = 1;             // Latch new output
}
// =============================
//     Number Display Function
// =============================

// Displays a 4-digit number on the 7-segment display
// Optionally enables decimal point at specified digit position
void displayNumber(int number, bool showDecimal = false, int decimalPos = 1) {
    // Break the number into individual digits
    int digits[4] = {
        (number / 1000) % 10,  // Thousands place
        (number / 100) % 10,   // Hundreds place
        (number / 10) % 10,    // Tens place
        number % 10            // Units place
    };

    // Loop through each digit and display it
    for (int i = 0; i < 4; ++i) {
        uint8_t segments = digitPattern[digits[i]];

        // Activate decimal point for selected digit
        if (showDecimal && i == decimalPos)
            segments &= ~0x80;  // Clear bit 7 (decimal point ON)

        writeToShiftRegister(segments, digitPos[i]);  // Display digit
        ThisThread::sleep_for(2ms);  // Small delay for persistence
    }
}

// =============================
//          Main Program
// =============================

int main() {
    // Initialize output pins to LOW
    latchPin = clockPin = dataPin = 0;

    // Start ticker interrupt to update time every second
    timerTicker.attach(&updateTime, 1s);

    while (true) {
        // If S1 is pressed, reset the timer
        if (!s1) {
            seconds = minutes = 0;
            ThisThread::sleep_for(200ms);  // Debounce delay
        }


        // If S3 is pressed, display voltage from potentiometer
        if (!s3) {
            // Read raw ADC value and convert to voltage (0.0–3.3V)
            float voltage = potentiometer.read_u16() * 3.3f / 65535;

            // Convert voltage to an integer: e.g., 2.75V → 2750
            int displayVal = static_cast<int>(voltage * 1000);

            // Display with decimal after first digit: e.g., 2.750
            displayNumber(displayVal, true, 0);
        } else {
            // Display time in MM:SS format as MM.SS
            int timeValue = minutes * 100 + seconds;

            // Show with decimal point between MM and SS
            displayNumber(timeValue, true, 1);
        }
    }
}
