/*
  LoRaWAN®-LED-Christmas-Tree main code v.1.0 by.: Jan-Ole Giebel

  Some parts of this code are copied from the Sensirion I²C SHT4X Arduino Library released under the BSD 3-Clause License (https://github.com/Sensirion/arduino-i2c-sht4x)
  and from the RAK documentation center (https://docs.rakwireless.com/product-categories/software-apis-and-libraries/rui3/overview/)
  
  This code is released under the Apache 2.0 License.

  Copyright 2024 Jan-Ole Giebel

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include "WS2816_Driver.h"

#define FW_VERSION 3

// Neopixel configuration
#define NEOPIXEL_PIN PA1  // Adjust pin number as needed
#define NUM_PIXELS 16
WS2816_Driver strip(NUM_PIXELS, NEOPIXEL_PIN);

// User button configuration
#define USR_BTN PA9

#define COLOR_AMBER     (uint16_t)65535, (uint16_t)20000,     (uint16_t)0
#define COLOR_AMBER_WITH_BRIGHTNESS maxBrightness, maxBrightness*0.31, 0

// We don't define the EUIs and keys here since we'll read them from the module
uint8_t node_device_eui[8];
uint8_t node_join_eui[8];
uint8_t node_app_key[16];

// Packet buffer
uint8_t data_buffer[64];
uint8_t data_len = 0;

// LED-Programm
uint8_t mode = 0;
uint8_t lastMode = 0;

// Timer for periodic uplink
uint32_t TX_INTERVAL = 300000;  // Default 5 minutes in milliseconds

uint16_t maxBrightness = 15000;

// Flash storage
#define FLASH_OFFSET_TX_INTERVAL 0
#define FLASH_OFFSET_MAX_BRIGHNTNESS 4
#define FLASH_OFFSET_MODE 6
#define FLASH_OFFSET_PIXELSTATE01 10
#define FLASH_OFFSET_PIXELSTATE02 58
#define FLASH_OFFSET_PIXELSTATE03 106

// Größe der Daten pro Frame: 2 Byte für Delay/Fade + 3 Bytes pro Pixel
int payloadSize = 2 + (NUM_PIXELS * 3);

// Sensor data structure
struct {
    float temperature;
    float humidity;
    bool valid;
} sensorData;

SensirionI2cSht4x sensor;

// Neopixel state structure
struct NeopixelState {
    uint8_t colors[NUM_PIXELS*3] = {0};
};
NeopixelState pixelState01;
NeopixelState pixelState02;

rt rtAnimation1;
rt rtAnimation2;
rt rtAnimation3;
rt rtCustomAnimation;

// Define debounce delay in milliseconds
const unsigned long DEBOUNCE_DELAY = 50;
volatile unsigned long lastDebounceTime = 0;

bool firstUplink = false;

// Structure to store raw color values
struct RawColor {
    byte r, g, b;
};

uint8_t animationBuffer[302];
uint8_t animationRecieveBuffer[302];
uint8_t animationBufferLength;

uint8_t numberOfSavedFrames = 0;

void joinCallback(int32_t status)
{
    Serial.printf("Join status: %d\r\n", status);
}

void loadNeopixelState(NeopixelState &pixelState, uint32_t offset) {
    if (api.system.flash.get(offset, pixelState.colors, sizeof(pixelState.colors))) {
    } else {
      for(int i=0; i<sizeof(pixelState.colors); i++) {
        pixelState.colors[i] = 0;
      }
    }

    Serial.println("Loaded colors:");
    
    int storageCounter = 0;
    strip.clear();
    // Apply loaded state to pixels
    for (int i = 0; i < NUM_PIXELS; i++) {
        Serial.print("R: ");
        Serial.println(pixelState.colors[storageCounter]);
        Serial.print("G: ");
        Serial.println(pixelState.colors[storageCounter+1]);
        Serial.print("B: ");
        Serial.println(pixelState.colors[storageCounter+2]);

        strip.setPixelColor(i, map(pixelState.colors[storageCounter], 0, 255, 0, maxBrightness), map(pixelState.colors[storageCounter+1], 0, 255, 0, maxBrightness), map(pixelState.colors[storageCounter+2], 0, 255, 0, maxBrightness));

        storageCounter += 3;
    }
    strip.show();

    storageCounter = 0;
    // Apply loaded state to pixels
    for (int i = 0; i < NUM_PIXELS; i++) {
        strip.setPixelColor(i, map(pixelState.colors[storageCounter], 0, 255, 0, maxBrightness), map(pixelState.colors[storageCounter+1], 0, 255, 0, maxBrightness), map(pixelState.colors[storageCounter+2], 0, 255, 0, maxBrightness));

        storageCounter += 3;
    }
    strip.show();
}

void saveNeopixelState(NeopixelState &pixelState, uint32_t offset) {
    api.system.flash.set(offset, pixelState.colors, sizeof(pixelState.colors));
}

void saveCustomAnimation(uint32_t offset) {
    api.system.flash.set(offset, animationBuffer, sizeof(animationBuffer));
}

void loadCustomAnimation(uint32_t offset) {
  if (!api.system.flash.get(offset, animationBuffer, sizeof(animationBuffer))) {
    mode = 0;
  }
}

void printHex(uint8_t* data, uint8_t length) {
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X", data[i]);
    }
    Serial.println();
}

bool readSHT45() {
    static char errorMessage[64];
    static int16_t error;
    
    delay(20);
    error = sensor.measureHighPrecision(sensorData.temperature, sensorData.humidity);
    if (error != 0) {
        Serial.print("Error trying to execute measureHighPrecision(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        sensorData.valid = false;
        return false;
    }

    sensorData.valid = true;
    return true;
}

int animation1(struct rt *rt) {
  RT_BEGIN(rt);

  uint16_t animation_step = 0;
  while(mode == (uint8_t)10) {
    for(uint16_t i = 0; i < strip.numPixels(); i++) {
      uint16_t phase = (i + animation_step) % 6;
      
      switch(phase) {
        case 0:
          strip.setPixelColor(i, maxBrightness, 0, 0);
          break;
        case 1:
          strip.setPixelColor(i, maxBrightness, maxBrightness, 0);
          break;
        case 2:
          strip.setPixelColor(i, 0, maxBrightness, 0);
          break;
        case 3:
          strip.setPixelColor(i, 0, maxBrightness, maxBrightness);
          break;
        case 4:
          strip.setPixelColor(i, 0, 0, maxBrightness);
          break;
        case 5:
          strip.setPixelColor(i, maxBrightness, 0, maxBrightness);
          break;
      }
    }
    strip.show();
    delay(50);
    animation_step++;
  }

  RT_END(rt);
}

int animation2(struct rt *rt) {
  RT_BEGIN(rt);

  while(mode == (uint8_t)11) {
    strip.setPixelColor(0, maxBrightness, 0, 0);
    strip.setPixelColor(2, maxBrightness, 0, 0);
    strip.setPixelColor(4, maxBrightness, 0, 0);
    strip.setPixelColor(6, maxBrightness, 0, 0);
    strip.setPixelColor(8, maxBrightness, 0, 0);
    strip.setPixelColor(9, maxBrightness, 0, 0);
    strip.setPixelColor(10, maxBrightness, 0, 0);
    strip.setPixelColor(13, maxBrightness, 0, 0);
    strip.setPixelColor(15, maxBrightness, 0, 0);
    
    strip.setPixelColor(1, COLOR_OFF);
    strip.setPixelColor(3, COLOR_OFF);
    strip.setPixelColor(5, COLOR_OFF);
    strip.setPixelColor(7, COLOR_OFF);
    strip.setPixelColor(11, COLOR_OFF);
    strip.setPixelColor(12, COLOR_OFF);
    strip.setPixelColor(14, COLOR_OFF);

    strip.show();
    delay(1000);

    strip.setPixelColor(0, COLOR_OFF);
    strip.setPixelColor(2, COLOR_OFF);
    strip.setPixelColor(4, COLOR_OFF);
    strip.setPixelColor(6, COLOR_OFF);
    strip.setPixelColor(8, COLOR_OFF);
    strip.setPixelColor(9, COLOR_OFF);
    strip.setPixelColor(10, COLOR_OFF);
    strip.setPixelColor(13, COLOR_OFF);
    strip.setPixelColor(15, COLOR_OFF);
    
    strip.setPixelColor(1, 0, maxBrightness, 0);
    strip.setPixelColor(3, 0, maxBrightness, 0);
    strip.setPixelColor(5, 0, maxBrightness, 0);
    strip.setPixelColor(7, 0, maxBrightness, 0);
    strip.setPixelColor(11, 0, maxBrightness, 0);
    strip.setPixelColor(12, 0, maxBrightness, 0);
    strip.setPixelColor(14, 0, maxBrightness, 0);

    strip.show();
    delay(1000);
  }

  RT_END(rt);
}

bool delayNotBlockingWithModeCheck(uint32_t ms_time, int _mode) {
  long start = millis();
  long end = start + ms_time;

  while(millis() < end) {
    interrupts();
    if((int)mode != mode) {
      return true;
    }
  }

  return false;
}

int animation3(struct rt *rt) {
  RT_BEGIN(rt);

  uint16_t animation_step = 0;
  while(mode == (uint8_t)12) {
    strip.clear();

    strip.setPixelColor(8, COLOR_AMBER_WITH_BRIGHTNESS);
    strip.setPixelColor(9, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    strip.setPixelColor(7, COLOR_AMBER_WITH_BRIGHTNESS);
    strip.setPixelColor(11, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    strip.setPixelColor(6, COLOR_AMBER_WITH_BRIGHTNESS);
    strip.setPixelColor(10, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    strip.setPixelColor(5, COLOR_AMBER_WITH_BRIGHTNESS);
    strip.setPixelColor(12, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    strip.setPixelColor(4, COLOR_AMBER_WITH_BRIGHTNESS);
    strip.setPixelColor(13, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    strip.setPixelColor(3, COLOR_AMBER_WITH_BRIGHTNESS);
    strip.setPixelColor(14, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    strip.setPixelColor(2, COLOR_AMBER_WITH_BRIGHTNESS);
    strip.setPixelColor(15, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    strip.setPixelColor(1, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    strip.setPixelColor(0, COLOR_AMBER_WITH_BRIGHTNESS);

    strip.show();
    delay(1000);

    if((int)mode != 12) {
      break;
    }

    for(int i=0; i<25; i++) {
      if((int)mode != 12) {
        break;
      }

      strip.setPixelColor(0, 0, 0, 0);
      strip.setPixelColor(3, 0, 0, 0);
      strip.setPixelColor(7, 0, 0, 0);
      strip.setPixelColor(11, 0, 0, 0);
      strip.setPixelColor(15, 0, 0, 0);

      strip.show();

      delay(100);

      if((int)mode != 12) {
        break;
      }

      strip.setPixelColor(0, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(3, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(7, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(11, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(15, COLOR_AMBER_WITH_BRIGHTNESS);

      strip.show();

      delay(100);

      if((int)mode != 12) {
        break;
      }

      strip.setPixelColor(1, 0, 0, 0);
      strip.setPixelColor(4, 0, 0, 0);
      strip.setPixelColor(8, 0, 0, 0);
      strip.setPixelColor(12, 0, 0, 0);

      strip.show();

      delay(100);

      if((int)mode != 12) {
        break;
      }

      strip.setPixelColor(1, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(4, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(8, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(12, COLOR_AMBER_WITH_BRIGHTNESS);

      strip.show();

      delay(100);

      if((int)mode != 12) {
        break;
      }

      strip.setPixelColor(2, 0, 0, 0);
      strip.setPixelColor(5, 0, 0, 0);
      strip.setPixelColor(9, 0, 0, 0);
      strip.setPixelColor(13, 0, 0, 0);

      strip.show();

      delay(100);

      if((int)mode != 12) {
        break;
      }

      strip.setPixelColor(2, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(5, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(9, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(13, COLOR_AMBER_WITH_BRIGHTNESS);

      strip.show();

      delay(100);

      if((int)mode != 12) {
        break;
      }

      strip.setPixelColor(3, 0, 0, 0);
      strip.setPixelColor(6, 0, 0, 0);
      strip.setPixelColor(10, 0, 0, 0);
      strip.setPixelColor(14, 0, 0, 0);

      strip.show();

      delay(100);

      if((int)mode != 12) {
        break;
      }

      strip.setPixelColor(3, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(6, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(10, COLOR_AMBER_WITH_BRIGHTNESS);
      strip.setPixelColor(14, COLOR_AMBER_WITH_BRIGHTNESS);

      strip.show();

      delay(100);

      if((int)mode != 12) {
        break;
      }
    }

    for(int i=0; i<NUM_PIXELS; i++) {
      strip.setPixelColor(i, 0, 0, 0);
    }

    strip.show();
  }

  RT_END(rt);
}

void setStaticColor(uint16_t r, uint16_t g, uint16_t b) {
  strip.clear();

  for(int i=0; i<NUM_PIXELS; i++) {
    strip.setPixelColor(i, r, g, b);
  }
  strip.show();
  delay(10);
  strip.show();
}

void checkMode() {
  Serial.print("Applying mode: ");
  Serial.println((int)mode);
  switch ((int)mode) {
    case 0:
      loadNeopixelState(pixelState01, FLASH_OFFSET_PIXELSTATE01);
      if((int)lastMode == 12){
        delay(100);
        loadNeopixelState(pixelState01, FLASH_OFFSET_PIXELSTATE01);
      }
      break;
    
    case 1:
      loadNeopixelState(pixelState02, FLASH_OFFSET_PIXELSTATE02);
      break;
    
    case 2:
      loadCustomAnimation(FLASH_OFFSET_PIXELSTATE03);
      break;
    
    case 3:
      Serial.print("Setting color to red.");
      setStaticColor(maxBrightness, 0, 0); // Red
      break;

    case 4:
      setStaticColor(0, maxBrightness, 0); // Green
      break;
    
    case 5:
      setStaticColor(0, 0, maxBrightness); // Blue
      break;
    
    case 6:
      setStaticColor(maxBrightness, maxBrightness, 0); // Yellow
      break;
    
    case 7:
      setStaticColor(COLOR_AMBER_WITH_BRIGHTNESS); // Amber
      break;
    
    case 8:
      setStaticColor(maxBrightness, 0, maxBrightness); // Magenta
      break;
    
    case 9:
      setStaticColor(0, maxBrightness, maxBrightness); // Cyan
      break;
    
    case 10:
      RT_INIT(&rtAnimation1);
      break;
    
    case 11:
      RT_INIT(&rtAnimation2);
      break;
    
    case 12:
      RT_INIT(&rtAnimation3);
      break;
  }
}

void loadSettings() {
    uint8_t stored_interval[4] = {0};
    uint32_t buffer_stored_interval = 0;
    if (api.system.flash.get(FLASH_OFFSET_TX_INTERVAL, stored_interval, sizeof(stored_interval))) {
        buffer_stored_interval |= (uint32_t)(stored_interval[0] << 0);
        buffer_stored_interval |= (uint32_t)(stored_interval[1] << 8);
        buffer_stored_interval |= (uint32_t)(stored_interval[2] << 16);
        buffer_stored_interval |= (uint32_t)(stored_interval[3] << 24);
        if (buffer_stored_interval >= 60000 && buffer_stored_interval <= 3600000) {
            TX_INTERVAL = buffer_stored_interval;
            Serial.print("Loaded stored TX interval: ");
            Serial.println(TX_INTERVAL);
        }
    } else {
        saveInterval(TX_INTERVAL);
    }

    if (!api.system.flash.get(FLASH_OFFSET_MODE, &mode, sizeof(mode))) {
      mode = 0;
      saveMode();
    }

    Serial.print("Loaded mode: ");
    Serial.println((int)mode);

    uint8_t maxBrightnessToLoad[2] = {0};
    if (api.system.flash.get(FLASH_OFFSET_MAX_BRIGHNTNESS, maxBrightnessToLoad, sizeof(maxBrightnessToLoad))) {
      maxBrightness |= (uint16_t)(maxBrightnessToLoad[0] << 0);
      maxBrightness |= (uint16_t)(maxBrightnessToLoad[1] << 8);
    } else {
      saveMaxBrightness(maxBrightness);
    }

    checkMode();
}

void saveInterval(uint32_t interval) {
  uint8_t interval_to_store[4] = {0};
  interval_to_store[0] = (uint8_t)(interval >> 0);
  interval_to_store[1] = (uint8_t)(interval >> 8);
  interval_to_store[2] = (uint8_t)(interval >> 16);
  interval_to_store[3] = (uint8_t)(interval >> 24);
  api.system.flash.set(FLASH_OFFSET_TX_INTERVAL, interval_to_store, sizeof(interval_to_store));
}

void saveMaxBrightness(uint16_t _maxBrightness) {
  uint8_t maxBrightnessToStore[2] = {0};
  maxBrightnessToStore[0] = (uint8_t)(_maxBrightness >> 0);
  maxBrightnessToStore[1] = (uint8_t)(_maxBrightness >> 8);
  api.system.flash.set(FLASH_OFFSET_MAX_BRIGHNTNESS, maxBrightnessToStore, sizeof(maxBrightnessToStore));
}

void saveMode() {
  api.system.flash.set(FLASH_OFFSET_MODE, &mode, sizeof(mode));
}

void userBTNInterruptHandler() {
    // Get current time
    unsigned long currentTime = millis();
    
    // Check if enough time has passed since the last interrupt
    if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
        // Update debounce time
        lastDebounceTime = currentTime;
        
        // Only proceed if button is pressed (LOW with pull-up)
        if (digitalRead(USR_BTN) == LOW) {
            unsigned long startTime = millis();
            
            do {
                if (millis() - startTime >= 5*1000) {
                    // Rejoin
                    api.lorawan.join();
                } else if (digitalRead(USR_BTN) == HIGH) {  // Button released
                    lastMode = mode;
                    if (mode <= (uint8_t)11) {
                        mode++;
                    } else {
                        mode = 0;
                    }
                    Serial.print("In mode: ");
                    Serial.println((int)mode);
                    checkMode();
                    saveMode();
                }
            } while (digitalRead(USR_BTN) == LOW);  // Continue while button is held
        }
    }
}

void fadeToColor(int fadeTime, uint8_t* animationData, int frameStartIndex) {
    int steps = 50;
    int fadeDelay = (fadeTime * 1000) / steps;
    
    // Store initial colors
    struct {
        byte r, g, b;
    } startColors[NUM_PIXELS];
    
    // Get starting colors for all pixels
    for (int i = 0; i < NUM_PIXELS; i++) {
        uint32_t color = strip.getPixelColor(i);
        startColors[i].r = (color >> 16) & 0xFF;
        startColors[i].g = (color >> 8) & 0xFF;
        startColors[i].b = color & 0xFF;
    }
    
    // Fade loop
    for (int step = 0; step <= steps; step++) {
        float fadeOutRatio = 1.0 - ((float)step / steps);  // Starts at 1.0 and goes to 0.0
        float fadeInRatio = (float)step / steps;           // Starts at 0.0 and goes to 1.0
        
        for (int i = 0; i < NUM_PIXELS; i++) {
            // Get target colors from animation data
            byte targetR = animationData[frameStartIndex + 2 + (i * 3)];
            byte targetG = animationData[frameStartIndex + 3 + (i * 3)];
            byte targetB = animationData[frameStartIndex + 4 + (i * 3)];
            
            // Calculate crossfaded colors
            byte currentR = (byte)((startColors[i].r * fadeOutRatio) + (targetR * fadeInRatio));
            byte currentG = (byte)((startColors[i].g * fadeOutRatio) + (targetG * fadeInRatio));
            byte currentB = (byte)((startColors[i].b * fadeOutRatio) + (targetB * fadeInRatio));
            
            // Apply brightness mapping and set pixel
            strip.setPixelColor(i, 
                map(currentR, 0, 255, 0, maxBrightness),
                map(currentG, 0, 255, 0, maxBrightness),
                map(currentB, 0, 255, 0, maxBrightness));
        }
        strip.show();
        delay(fadeDelay);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("LoRaWAN-LED-Christmas-Tree");

    // Configure button and interrupt handler
    pinMode(USR_BTN, INPUT_PULLUP);
    attachInterrupt(USR_BTN, userBTNInterruptHandler, FALLING);
    
    // Initialize Neopixels
    strip.begin();
    
    // Initialize SHT45
    Wire.begin();
    sensor.begin(Wire, SHT45_I2C_ADDR_44);

    sensor.softReset();
    delay(10);
    
    
    // Load saved settings
    loadSettings();

    delay(100);
    
    // // Read the pre-flashed credentials
    api.lorawan.deui.get(node_device_eui, 8);
    api.lorawan.appeui.get(node_join_eui, 8);
    api.lorawan.appkey.get(node_app_key, 16);

    Serial.printf("Set the low power mode %s\n\r", api.system.lpm.set(1) ? "Success" : "Fail");

    // Enable ADR
    api.lorawan.adr.set(true);
    
    Serial.println("Pre-flashed Credentials:");
    Serial.print("Device EUI: ");
    printHex(node_device_eui, 8);
    Serial.print("Join EUI: ");
    printHex(node_join_eui, 8);
    Serial.print("App Key: ");
    printHex(node_app_key, 16);

    if (!api.lorawan.band.set(RAK_REGION_EU868)) {
        Serial.printf("LoRaWan OTAA - set band is incorrect! \r\n");
        return;
    }
    
    if (!api.lorawan.deviceClass.set(RAK_LORA_CLASS_C)) {
        Serial.println("Class setting error");
        return;
    }

    if (!api.lorawan.njm.set(RAK_LORA_OTAA)) {
        Serial.println("Join mode error");
        return;
    }
    
    if (!api.lorawan.dr.set(5)) {
        Serial.println("DR error");
        return;
    }

    if (!api.lorawan.adr.set(true)) {
        Serial.printf("LoRaWan OTAA - set adaptive data rate is incorrect! \r\n");
        return;
    }
    
    Serial.println("Starting join...");
    if (!api.lorawan.join()) {
        Serial.println("Join failed");
        return;
    }

    api.lorawan.registerRecvCallback(processDownlink);
    api.lorawan.registerJoinCallback(joinCallback);

    api.system.timer.create((RAK_TIMER_ID)0, (RAK_TIMER_HANDLER)periodicUplink, RAK_TIMER_PERIODIC);
    api.system.timer.start((RAK_TIMER_ID)0, TX_INTERVAL, NULL);

    RT_INIT(&rtCustomAnimation);
}

void sendFirstMessage() {
  if(firstUplink == false && api.lorawan.njs.get() == 1) {
    periodicUplink(NULL);
    firstUplink = true;
  }
}

void fadeToColor(int fadeTime, uint8_t* animationData, int frameStartIndex, RawColor* previousColors, int numFrames) {
    int steps = 50;
    int fadeDelay = (fadeTime * 1000) / steps;
    
    // Store target colors
    RawColor* targetColors = new RawColor[NUM_PIXELS];
    
    // Read and store target colors
    for (int i = 0; i < NUM_PIXELS; i++) {
        if((int)mode != 2) {
          break;
        }

        if(numFrames != animationBuffer[1]) {
          break;
        }

        targetColors[i].r = animationData[frameStartIndex + 2 + (i * 3)];
        targetColors[i].g = animationData[frameStartIndex + 3 + (i * 3)];
        targetColors[i].b = animationData[frameStartIndex + 4 + (i * 3)];
    }
    
    // Fade loop
    for (int step = 0; step <= steps; step++) {
        if((int)mode != 2) {
          break;
        }

        if(numFrames != animationBuffer[1]) {
          break;
        }

        float fadeOutRatio = 1.0 - ((float)step / steps);
        float fadeInRatio = (float)step / steps;
        
        for (int i = 0; i < NUM_PIXELS; i++) {
            if((int)mode != 2) {
              break;
            }

            if(numFrames != animationBuffer[1]) {
              break;
            }

            // Calculate crossfaded colors
            byte currentR = (byte)((previousColors[i].r * fadeOutRatio) + (targetColors[i].r * fadeInRatio));
            byte currentG = (byte)((previousColors[i].g * fadeOutRatio) + (targetColors[i].g * fadeInRatio));
            byte currentB = (byte)((previousColors[i].b * fadeOutRatio) + (targetColors[i].b * fadeInRatio));
            
            strip.setPixelColor(i, 
                map(currentR, 0, 255, 0, maxBrightness),
                map(currentG, 0, 255, 0, maxBrightness),
                map(currentB, 0, 255, 0, maxBrightness));
        }
        
        strip.show();
        delay(fadeDelay);
    }
    
    // Update previous colors with final values
    for (int i = 0; i < NUM_PIXELS; i++) {
        if((int)mode != 2) {
          break;
        }

        if(numFrames != animationBuffer[1]) {
          break;
        }

        previousColors[i] = targetColors[i];
    }
    
    delete[] targetColors;
}

int customAnimation(struct rt *rt) {
  RT_BEGIN(rt);
  
  // Allocate storage for color tracking
  RawColor* currentColors = new RawColor[NUM_PIXELS];
  
  // Initialize with zeros
  for (int i = 0; i < NUM_PIXELS; i++) {
      currentColors[i].r = 0;
      currentColors[i].g = 0;
      currentColors[i].b = 0;
  }
  
  while((int)mode == 2) {
      sendFirstMessage();
      const int numFrames = animationBuffer[1];
      for (int frame = 0; frame < numFrames; frame++) {
          if((int)mode != 2) {
            break;
          }

          if(numFrames != animationBuffer[1]) {
            break;
          }
          
          int frameStartIndex = (frame * 50) + 2;
          int frameDelay = animationBuffer[frameStartIndex];
          int fadeTime = animationBuffer[frameStartIndex + 1];
          
          if (fadeTime == 0) {
              // Set colors immediately
              for (int i = 0; i < NUM_PIXELS; i++) {
                  if((int)mode != 2) {
                    break;
                  }

                  if(numFrames != animationBuffer[1]) {
                    break;
                  }

                  int pixelIndex = frameStartIndex + 2 + (i * 3);
                  
                  // Store raw colors
                  currentColors[i].r = animationBuffer[pixelIndex];
                  currentColors[i].g = animationBuffer[pixelIndex + 1];
                  currentColors[i].b = animationBuffer[pixelIndex + 2];
                  
                  // Apply brightness mapping and set LED
                  strip.setPixelColor(i, 
                      map(currentColors[i].r, 0, 255, 0, maxBrightness),
                      map(currentColors[i].g, 0, 255, 0, maxBrightness),
                      map(currentColors[i].b, 0, 255, 0, maxBrightness));
              }
              strip.show();
          } else {
              // Perform fade
              fadeToColor(fadeTime, animationBuffer, frameStartIndex, currentColors, numFrames);
          }

          if(numFrames == 1) {
            break;
          }
          
          delay(frameDelay * 1000);
      }
  }
  
  delete[] currentColors;

  RT_END(rt);
}

void loop()
{
  sendFirstMessage();

  if((int)mode == 2) {
    RT_SCHEDULE(customAnimation(&rtCustomAnimation));
  } else if((int)mode == 10) {
    RT_SCHEDULE(animation1(&rtAnimation1));
  } else if((int)mode == 11) {
    RT_SCHEDULE(animation2(&rtAnimation2));
  } else if((int)mode == 12) {
    RT_SCHEDULE(animation3(&rtAnimation3));
  }
}

void periodicUplink(void *data)
{
    int dataOffset = 9;

    data_buffer[1] = 0x02;
    data_buffer[2] = (FW_VERSION >> 8) & 0xFF;
    data_buffer[3] = FW_VERSION & 0xFF;

    if (readSHT45()) {
        int16_t temp_fixed = (int16_t)(sensorData.temperature * 100);
        uint16_t hum_fixed = (uint16_t)(sensorData.humidity * 100);
        
        data_buffer[0] = 0x01;  // Packet type: ok
        data_buffer[4] = 0x03;
        data_buffer[5] = (temp_fixed >> 8) & 0xFF;
        data_buffer[6] = temp_fixed & 0xFF;
        data_buffer[7] = 0x04;
        data_buffer[8] = (hum_fixed >> 8) & 0xFF;
        data_buffer[9] = hum_fixed & 0xFF;
        
        Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", 
                     sensorData.temperature, 
                     sensorData.humidity);
    } else {
        data_buffer[0] = 0xFF;  // Packet type: error
        dataOffset = 3;
        Serial.println("Error reading SHT45 sensor");
    }

    float batteryVoltage;
    batteryVoltage = api.system.bat.get();

    Serial.print("Battery voltage: ");
    Serial.println(batteryVoltage);

    uint8_t *batteryVoltageBuffer;

    batteryVoltageBuffer = reinterpret_cast<uint8_t*>(&batteryVoltage);

    data_buffer[dataOffset+1] = 0x05;
    data_buffer[dataOffset+2] = batteryVoltageBuffer[0];
    data_buffer[dataOffset+3] = batteryVoltageBuffer[1];
    data_buffer[dataOffset+4] = batteryVoltageBuffer[2];
    data_buffer[dataOffset+5] = batteryVoltageBuffer[3];

    data_buffer[dataOffset+6] = 0x06;
    data_buffer[dataOffset+7] = (TX_INTERVAL >> 24) & 0xFF;
    data_buffer[dataOffset+8] = (TX_INTERVAL >> 16) & 0xFF;
    data_buffer[dataOffset+9] = (TX_INTERVAL >> 8) & 0xFF;
    data_buffer[dataOffset+10] = TX_INTERVAL & 0xFF;

    data_buffer[dataOffset+11] = 0x07;
    data_buffer[dataOffset+12] = mode;

    data_buffer[dataOffset+13] = 0x08;
    data_buffer[dataOffset+14] = (maxBrightness >> 8) & 0xFF;
    data_buffer[dataOffset+15] = maxBrightness & 0xFF;

    if(data_buffer[0] == 0x01) {
      data_len = 25;
    } else {
      data_len = 19;
    }
    
    
    if (api.lorawan.send(data_len, data_buffer, 2, false, 1)) {
        Serial.println("Uplink sent successfully");
    } else {
        Serial.println("Uplink failed");
    }
}

void processDownlink(SERVICE_LORA_RECEIVE_T * data)
{
  Serial.println("Something received!");
  for (int i = 0; i < data->BufferSize; i++) {
      Serial.printf("%x", data->Buffer[i]);
  }
  Serial.println();
  Serial.printf("%x", data->Port);
  if(data->Port != 1) {
    return;
  }

  uint8_t length = data->BufferSize;
  uint8_t* buffer = data->Buffer;
  int index = 0;
    if (length > 0) {
      while(index < length) {
        switch (buffer[index+0]) {
            case 0x00:  // Configuration command for interval
                if (length >= 3) {
                    uint16_t new_interval = (buffer[index+1] << 8) | buffer[index+2];
                    uint32_t new_interval_ms = new_interval * 1000;
                    
                    if (new_interval_ms >= 60000 && new_interval_ms <= 3600000) {
                        TX_INTERVAL = new_interval_ms;
                        api.system.timer.stop((RAK_TIMER_ID)0);
                        api.system.timer.start((RAK_TIMER_ID)0, TX_INTERVAL, NULL);
                        saveInterval(TX_INTERVAL);
                        Serial.print("New TX interval set and saved: ");
                        Serial.println(TX_INTERVAL);
                    }
                }
                
                index += 3;
                break;
            
            case 0x01:  // Configuration command for max brightness
                if (length >= 3) {
                    uint16_t newMaxBrightness = (buffer[index+1] << 8) | buffer[index+2];
                    
                    if (newMaxBrightness >= 1 && newMaxBrightness <= 65535) {
                        maxBrightness = newMaxBrightness;
                        saveMaxBrightness(maxBrightness);
                        Serial.print("New max brightness set and saved: ");
                        Serial.println(maxBrightness);
                    }
                }
                
                index += 3;
                break;
            
            case 0x02:  // Configuration command for changing the mode
                if (length >= 2) {
                    uint16_t newMode = buffer[index+1];
                    
                    if (newMode >= 0 && newMode <= 12) {
                        mode = newMode;
                        saveMode();
                        Serial.print("New mode set and saved: ");
                        Serial.println(mode);
                        checkMode();
                    }
                }
                
                index += 2;
                break;
            
            case 0x03:  // Set one neopixel to an individual color
                if (length >= 5) {
                    Serial.print("Last mode: ");
                    Serial.println((int)lastMode);

                    if((int)lastMode == 1) {
                      pixelState01 = pixelState02;
                    }
                    mode = 0;
                    lastMode = mode;
                    saveMode();
                    uint8_t led_index = buffer[index+1];
                    if (led_index < NUM_PIXELS) {
                        strip.setPixelColor(led_index, map(buffer[index+2], 0, 255, 0, maxBrightness), map(buffer[index+3], 0, 255, 0, maxBrightness), map(buffer[index+4], 0, 255, 0, maxBrightness));
                        strip.show();

                        strip.setPixelColor(led_index, map(buffer[index+2], 0, 255, 0, maxBrightness), map(buffer[index+3], 0, 255, 0, maxBrightness), map(buffer[index+4], 0, 255, 0, maxBrightness));
                        strip.show();
                        
                        // Save state
                        pixelState01.colors[(led_index*3)] = buffer[index+2];
                        pixelState01.colors[(led_index*3)+1] = buffer[index+3];
                        pixelState01.colors[(led_index*3)+2] = buffer[index+4];
                        saveNeopixelState(pixelState01, FLASH_OFFSET_PIXELSTATE01);
                        
                        Serial.printf("Set LED %d to R:%d G:%d B:%d\n", 
                                    led_index, buffer[index+2], buffer[index+3], buffer[index+4]);
                    }

                    Serial.println("Saved colors:");
                    for(int i=0; i<sizeof(pixelState01.colors); i++) {
                      Serial.print(pixelState01.colors[i]);
                    }
                    Serial.println();
                }

                index += 5;
                break;
                
            case 0x04:  // Set all Neopixels to a specific color
                if (length >= 4) {
                    mode = (uint8_t)1;
                    lastMode = mode;
                    saveMode();
                    int storageCounter = 0;
                    for (int i = 0; i < NUM_PIXELS; i++) {
                        strip.setPixelColor(i, map(buffer[index+1], 0, 255, 0, maxBrightness), map(buffer[index+2], 0, 255, 0, maxBrightness), map(buffer[index+3], 0, 255, 0, maxBrightness));
                        pixelState02.colors[storageCounter] = buffer[index+1];
                        pixelState02.colors[storageCounter+1] = buffer[index+2];
                        pixelState02.colors[storageCounter+2] = buffer[index+3];

                        storageCounter += 3;
                    }
                    strip.show();

                    for (int i = 0; i < NUM_PIXELS; i++) {
                        strip.setPixelColor(i, map(buffer[index+1], 0, 255, 0, maxBrightness), map(buffer[index+2], 0, 255, 0, maxBrightness), map(buffer[index+3], 0, 255, 0, maxBrightness));
                    }
                    strip.show();
                    
                    saveNeopixelState(pixelState02, FLASH_OFFSET_PIXELSTATE02);
                    
                    Serial.printf("Set all LEDs to R:%d G:%d B:%d\n", 
                                buffer[index+1], buffer[index+2], buffer[index+3]);
                }

                index += 4;
                break;
            
            case 0x05: // Set a custom animation
              if (length >= 48) {
                  const int numFrames = buffer[1];
                  const int numberOfSentFrames = ((length-2)/50);

                  if((numFrames > numberOfSavedFrames) && (numFrames != numberOfSentFrames)) {
                    if(numberOfSavedFrames < 1) {
                      memcpy(animationRecieveBuffer, buffer, (numberOfSentFrames*50)+2);
                    } else {
                      memcpy(animationRecieveBuffer+((numberOfSavedFrames*50)+2), buffer+2, (numberOfSentFrames*50));
                    }

                    numberOfSavedFrames += numberOfSentFrames;
                  } else {
                    if(numberOfSavedFrames > 0) {
                      memcpy(animationRecieveBuffer+((numberOfSavedFrames*50)+2), buffer+2, (numberOfSentFrames*50));
                      memcpy(animationBuffer, animationRecieveBuffer, sizeof(animationRecieveBuffer));
                    } else {
                      memcpy(animationBuffer, buffer, (numberOfSentFrames*50)+2);
                    }

                    saveCustomAnimation(FLASH_OFFSET_PIXELSTATE03);

                    lastMode = mode;
                    mode = (uint8_t)2;
                    saveMode();

                    animationBufferLength = length;
                    numberOfSavedFrames = 0;
                  }
                  
              }
              index = length;
              break;
        }
      }
    }
}