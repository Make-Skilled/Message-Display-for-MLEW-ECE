#include <Arduino.h>
#include <DMD32.h>
#include "fonts/Arial_black_16.h"

#define DISPLAYS_ACROSS 4
#define DISPLAYS_DOWN 1

DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);
hw_timer_t *disp_timer = NULL;

void IRAM_ATTR triggerScan() {
    dmd.scanDisplayBySPI();
}

void initTimer() {
    disp_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(disp_timer, &triggerScan, true);
    timerAlarmWrite(disp_timer, 500, true);
    timerAlarmEnable(disp_timer);
}

void setup() {
    Serial.begin(115200);
    SPI.begin();
    initTimer();
    dmd.clearScreen(true);
    Serial.println("DMD Display Initialized");
}

void loop() {
    String message = "Hello, Welcome to ESP32 DMD Display! ";
    dmd.clearScreen(true);
    dmd.selectFont(Arial_Black_16);
    
    int startX = (32 * DISPLAYS_ACROSS) - 1;
    
    // Adjusted Y position to 1 to leave one line empty at bottom
    // Arial_14 is 14 pixels high, and P10 is 16 pixels high
    // Setting Y to 1 will leave 1 pixel at top and bottom
    dmd.drawMarquee(message.c_str(), message.length(), startX, 1);
    
    long timer = millis();
    boolean ret = false;
    
    while (!ret) {
        if ((timer + 40) < millis()) {
            ret = dmd.stepMarquee(-1, 0);
            timer = millis();
            delay(2);
        }
    }
    
    delay(100);
}
