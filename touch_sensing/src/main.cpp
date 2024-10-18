// Main code for the cloud LED lamp project
// Cycle between different modes of LED lighting based on touch input
#include <Adafruit_CAP1188.h>
#include <Arduino.h>
#include <OctoWS2811.h>
#include <SPI.h>
#include <Wire.h>
#include "SerialHandler.h"

#define LED_COUNT 248

// Reset Pin is used for I2C or SPI
#define CAP1188_RESET 9

// CS pin is used for software or hardware SPI
#define CAP1188_CS 10

// These are defined for software SPI, for hardware SPI, check your
// board's SPI pins in the Arduino documentation
#define CAP1188_MOSI 11
#define CAP1188_MISO 12
#define CAP1188_CLK 13

Adafruit_CAP1188 cap = Adafruit_CAP1188();

const int numPins = 1;
byte pinList[numPins] = {7};

const int ledsPerStrip = LED_COUNT;

const int bytesPerLED = 3; // change to 4 if using RGBW
DMAMEM int displayMemory[ledsPerStrip * numPins * bytesPerLED / 4];
int drawingMemory[ledsPerStrip * numPins * bytesPerLED / 4];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

enum LedMode { THUNDER,
               SUNLIGHT,
               RAINBOW,
               COLOR,
               GREEN,
};

LedMode ledMode = THUNDER;
LedMode lastMode = THUNDER;
unsigned long lastModeChangeTime = 0;
const unsigned long modeChangeCooldown = 1000; // 1 second cooldown
unsigned long lastTouchTime = 0;
const unsigned long touchBufferTime = 5000; // 200 ms cooldown

void updateThunderMode();
void updateSunlightMode(uint32_t color);
void updateRainbowMode();
void updateBrightness();

// Add this near the top of the file, with other global variables
float globalBrightness = 150;          // Current brightness level
const int maxBrightness = 255;         // Maximum brightness level
const int minBrightness = 0;           // Minimum brightness level
const float brightnessIncrement = 0.5; // Amount to change brightness

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(9600);
    if (!cap.begin()) {
        Serial.println("CAP1188 not found");
        while (1)
            ;
    }
    Serial.println("CAP1188 found!");
    leds.begin();
    leds.show();
}

// #define RED 0x080000
// #define GREEN 0x001600
#define BLUE 0x000016
#define YELLOW 0x101400
#define PINK 0x120009
#define ORANGE 0x100400
#define CYAN 0x001010
#define WHITE 0x101010

int colors[] = {RED, GREEN, BLUE, YELLOW, PINK, ORANGE, WHITE};

void loop() {
    digitalWrite(LED_BUILTIN, millis() % 1000 < 500);
    uint8_t touched = cap.touched();
    static bool thresholdReached = false;
    
    if (touched && (millis() - lastTouchTime > touchBufferTime)) {
        unsigned long currentTime = millis();
        if (!thresholdReached) {
            lastTouchTime = currentTime;
            thresholdReached = true;
        } else if (currentTime - lastTouchTime < touchBufferTime) {
            // Execute this block if touched for less than touchBufferTime
            Serial.println("Quick touch detected!");
            // Add your desired quick touch action here
        }
    }

    else if ((touched & 0b00000011) == 0b00000011) {
        unsigned long currentTime = millis();
        if (currentTime - lastModeChangeTime > modeChangeCooldown) {
            // Change LED mode beetween 0 , 1 , 2. Do not use 3 and 4
            ledMode = static_cast<LedMode>((static_cast<int>(ledMode) + 1) % 3);
            lastModeChangeTime = currentTime;
            Serial.print("Changing mode to: ");
            Serial.println(ledMode);
        }
    }
    // Add brightness control
    else if (touched & 0b00000001) {
        // Increase brightness
        globalBrightness = min(maxBrightness, globalBrightness + brightnessIncrement);
        if (ledMode != LedMode::GREEN && ledMode != LedMode::RED) {
            lastMode = ledMode;
            ledMode = LedMode::GREEN;
            // Clear the LEDs
            for (int i = 0; i < LED_COUNT; i++) {
                leds.setPixelColor(i, 0, 0, 0);
            }
        }
    } else if (touched & 0b00000010) {
        // Decrease brightness
        globalBrightness = max(minBrightness, globalBrightness - brightnessIncrement);
        if (ledMode != LedMode::RED && ledMode != LedMode::GREEN) {
            lastMode = ledMode;
            ledMode = LedMode::RED;
            // Clear the LEDs
            for (int i = 0; i < LED_COUNT; i++) {
                leds.setPixelColor(i, 0, 0, 0);
            }
        }
    } else {
        ledMode = lastMode;
        thresholdReached = false;
    }

    // Update LEDs based on current mode
    switch (ledMode) {
    case THUNDER:
        updateThunderMode();
        break;
    case SUNLIGHT:
        updateSunlightMode(); // Pass the touched variable
        break;
    case RAINBOW:
        updateRainbowMode();
        break;
    case RED: {
        // Set X number of LEDs to red depending on brightness
        // Clear the LEDs
        for (int i = 0; i < LED_COUNT; i++) {
            leds.setPixelColor(i, 0, 0, 0);
        }
        int numLEDs = LED_COUNT * globalBrightness / 255;
        for (int i = 0; i < numLEDs; i++) {
            leds.setPixelColor(i, 255, 0, 0);
        }
        
        break;
    }
    case GREEN: {
        // Set X number of LEDs to green depending on brightness
        int numLEDs = LED_COUNT * globalBrightness / 255;
        for (int i = 0; i < numLEDs; i++) {
            leds.setPixelColor(i, 0, 255, 0);
        }
        break;
    }
    }
    updateBrightness();
}

// Base values for thunder effect parameters
const uint32_t BACKGROUND_BLUE = leds.Color(0, 0, 50);
uint32_t lightningColor = leds.Color(255, 255, 255);
const int THUNDER_CHANCE = 10;
const unsigned long LIGHTNING_DURATION = 50;
const unsigned long LAST_FLASH_DURATION = 200;
const unsigned long FLASH_INTERVAL = 100;
const unsigned long FADE_INTERVAL = 10;
const unsigned long LIGHTNING_COOLDOWN = 10000;
const int MAX_LIGHTNING_LENGTH = LED_COUNT / 8;
const int MIN_FLASHES = 1;
const int MAX_FLASHES = 6;

unsigned long lastLightningTime = 0;
unsigned long lastFlashTime = 0;
unsigned long lastFadeTime = 0;
bool isLightningSequence = false;
int currentFlash = 0;
int totalFlashes = 0;
int lightningStart = 0;
int lightningLength = 0;
int fadingIndex = 0;

void updateThunderMode() {
    unsigned long currentTime = millis();

    // Check if we're currently in a lightning sequence
    if (isLightningSequence) {
        if (currentFlash < totalFlashes) {
            unsigned long flashDuration = (currentFlash == totalFlashes - 1) ? LAST_FLASH_DURATION + random(-50, 51) : LIGHTNING_DURATION + random(-10, 11);

            // Flash on
            if (currentTime - lastFlashTime < flashDuration) {
                for (int i = lightningStart; i < lightningStart + lightningLength; i++) {
                    leds.setPixelColor(i % LED_COUNT, lightningColor);
                }
            }
            // Flash off (only for non-last flashes)
            else if (currentFlash < totalFlashes - 1 && currentTime - lastFlashTime < FLASH_INTERVAL + random(-20, 21)) {
                for (int i = lightningStart; i < lightningStart + lightningLength; i++) {
                    leds.setPixelColor(i % LED_COUNT, BACKGROUND_BLUE);
                }
            }
            // Start next flash
            else if (currentFlash < totalFlashes - 1) {
                currentFlash++;
                lastFlashTime = currentTime;
            }
            // End of last flash
            else if (currentTime - lastFlashTime >= flashDuration) {
                isLightningSequence = false;
                fadingIndex = 0;
                lastFadeTime = currentTime;
            }
        }
        return; // Skip the rest of the function during lightning sequence
    }

    // Fading logic
    if (fadingIndex < lightningLength) {
        if (currentTime - lastFadeTime >= FADE_INTERVAL + random(-5, 6)) {
            int fadePos = (lightningStart + fadingIndex) % LED_COUNT;
            leds.setPixelColor(fadePos, BACKGROUND_BLUE);
            fadingIndex++;
            lastFadeTime = currentTime;
        }
        return; // Skip the rest of the function while fading
    }

    // Set all LEDs to the background blue color
    for (int i = 0; i < LED_COUNT; i++) {
        leds.setPixelColor(i, BACKGROUND_BLUE);
    }

    // Randomly generate lightning
    if (currentTime - lastLightningTime >= LIGHTNING_COOLDOWN + random(-5000, 5001) &&
        random(100) < THUNDER_CHANCE) {
        // Start a new lightning sequence
        isLightningSequence = true;
        currentFlash = 0;
        totalFlashes = random(MIN_FLASHES, MAX_FLASHES + 1); // Random number of flashes
        lastFlashTime = currentTime;
        lastLightningTime = currentTime;

        // Determine random start and length for lightning
        lightningStart = random(LED_COUNT);
        lightningLength = random(1, MAX_LIGHTNING_LENGTH + 1);
        // Slightly vary from white
        lightningColor = leds.Color(235, 235, 235);
        uint8_t r = (lightningColor >> 16) & 0xFF;
        uint8_t g = (lightningColor >> 8) & 0xFF;
        uint8_t b = lightningColor & 0xFF;
        r += random(-20, 21);
        g += random(-20, 21);
        b += random(-20, 21);
        lightningColor = leds.Color(r, g, b);
    }

    // Add some subtle variation to the background
    for (int i = 0; i < LED_COUNT; i++) {
        if (random(100) < 20) { // 5% chance to slightly vary each LED
            int variation = random(-15, 16);
            uint32_t color = leds.Color(0, 0, max(0, min(255, 50 + variation)));
            leds.setPixelColor(i, color);
        }
    }
}

void updateSunlightMode(uint32_t clr=leds.Color(255, 128, 0)) {
    // Set the brightness of each LED to a warmer orange color with flickering effect
    for (int i = 0; i < LED_COUNT; i++) {
        int flicker = random(-20, 21);
        r = constrain(clr >> 16 + flicker, 0, 255);
        g = constrain((clr >> 8 & 0xFF) + flicker, 0, 255);
        b = constrain(clr & 0xFF + flicker, 0, 255);
        leds.setPixelColor(i, r, g, b); // Warmer orange sunlight effect
    }
}

// Function to convert a hue value to a color
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos; // Reverse the wheel for a different effect
    if (WheelPos < 85) {
        return leds.Color(255 - WheelPos * 3, 0, WheelPos * 3); // Red to Green
    } else if (WheelPos < 170) {
        WheelPos -= 85;
        return leds.Color(0, WheelPos * 3, 255 - WheelPos * 3); // Green to Blue
    } else {
        WheelPos -= 170;
        return leds.Color(WheelPos * 3, 255 - WheelPos * 3, 0); // Blue to Red
    }
}
void updateRainbowMode() {
    static uint32_t lastUpdateTime = 0; // Last time the rainbow was updated
    static int hue = 0;                 // Current hue value for the rainbow effect
    const int speed = 5;                // Speed of the rainbow transition

    // Update the rainbow effect every few milliseconds
    if (millis() - lastUpdateTime > 20) {
        lastUpdateTime = millis();

        // Set each LED to the current hue
        for (int i = 0; i < LED_COUNT; i++) {
            // Calculate the color based on the current hue and LED index
            uint32_t color = Wheel((hue + (i * 256 / LED_COUNT)) & 255);
            leds.setPixelColor(i, color);
        }

        // Increment the hue for the next frame
        hue += speed;
        if (hue >= 256) {
            hue = 0; // Reset hue to loop the colors
        }
    }
}

// I hate myself for this
void updateBrightness() {
    // Save the current rgb values in an array
    static uint32_t currentRGB[LED_COUNT];
    // Get the current brightness for the LEDs
    for (int i = 0; i < LED_COUNT; i++) {
        currentRGB[i] = leds.getPixel(i);
        uint32_t color = leds.getPixel(i);
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        uint8_t newR = max(0, min(255, r * globalBrightness / 255));
        uint8_t newG = max(0, min(255, g * globalBrightness / 255));
        uint8_t newB = max(0, min(255, b * globalBrightness / 255));
        leds.setPixelColor(i, newR, newG, newB);
    }
    leds.show();
    // Set the old brightness values back after show(), so the brightness scaling is not applied repeatedly
    for (int i = 0; i < LED_COUNT; i++) {
        leds.setPixelColor(i, currentRGB[i]);
    }
}