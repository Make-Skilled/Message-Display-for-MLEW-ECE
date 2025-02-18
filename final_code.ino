#include <Arduino.h>
#include <DMD32.h>
#include "fonts/Arial14.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define DISPLAYS_ACROSS 4
#define DISPLAYS_DOWN 1

// WiFi Credentials
const char* ssid = "Purna";
const char* password = "Galla4446";
const char* apiUrl = "http://62.72.58.116/fetchMessages.php";

// Message Storage
const int MAX_MESSAGES = 5;
String messages[MAX_MESSAGES];
volatile int messageCount = 0;
volatile int currentMessage = 0;
String currentDisplayMessage = "Initializing...";

// Mutex for protecting shared resources
SemaphoreHandle_t xMutex = NULL;

DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);
hw_timer_t *disp_timer = NULL;
volatile bool displayRefreshFlag = false;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR triggerScan() {
    portENTER_CRITICAL_ISR(&timerMux);
    displayRefreshFlag = true;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void wifiTask(void * parameter) {
    while(1) {
        Serial.println("Checking WiFi connection...");
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi not connected. Attempting to reconnect...");
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, password);
            
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                vTaskDelay(pdMS_TO_TICKS(500));
                attempts++;
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected to WiFi. Fetching messages...");
            HTTPClient http;
            http.begin(apiUrl);
            int httpResponseCode = http.GET();

            if (httpResponseCode > 0) {
                String response = http.getString();
                Serial.println("Response received: " + response);
                StaticJsonDocument<1024> doc;
                DeserializationError error = deserializeJson(doc, response);

                if (!error && doc.containsKey("success") && doc["success"].as<bool>()) {
                    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        Serial.println("Parsing JSON and updating messages...");
                        
                        for (int i = 0; i < MAX_MESSAGES; i++) {
                            messages[i] = "";
                        }
                        messageCount = 0;

                        JsonArray msgArray = doc["messages"];
                        messageCount = min((int)msgArray.size(), MAX_MESSAGES);
                        for (int i = 0; i < messageCount; i++) {
                            messages[i] = msgArray[i]["message"].as<String>();
                            Serial.println("New Message Stored: " + messages[i]);
                        }

                        xSemaphoreGive(xMutex);
                    }
                }
            }
            http.end();
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void displayTask(void * parameter) {
    while(1) {
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (messageCount > 0) {
                currentDisplayMessage = messages[currentMessage];
                currentMessage = (currentMessage + 1) % messageCount;
            } else {
                currentDisplayMessage = "Waiting for Messages...";
            }
            xSemaphoreGive(xMutex);
        }

        dmd.clearScreen(true);
        dmd.selectFont(Arial_14);
        
        int startX = (32 * DISPLAYS_ACROSS) - 1;
        dmd.drawMarquee(currentDisplayMessage.c_str(), currentDisplayMessage.length(), startX, 1);
        
        boolean ret = false;
        long scrollTimer = millis();
        
        while (!ret) {
            if ((scrollTimer + 60) < millis()) {  // Ultra-smooth scrolling
                ret = dmd.stepMarquee(-1, 0);
                scrollTimer = millis();
                vTaskDelay(pdMS_TO_TICKS(8));  // Smooth transition delay
            }
            vTaskDelay(3);  // Consistent timing yield
        }

        vTaskDelay(pdMS_TO_TICKS(1000));  // Pause between messages
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting ESP32 P10 Display");

    xMutex = xSemaphoreCreateMutex();
    if (xMutex == NULL) {
        Serial.println("Mutex creation failed!");
        return;
    }

    Serial.println("Initializing display...");
    SPI.begin();

    disp_timer = timerBegin(1, 80, true);
    timerAttachInterrupt(disp_timer, &triggerScan, true);
    timerAlarmWrite(disp_timer, 150, true);  // Ultra-smooth refresh rate
    timerAlarmEnable(disp_timer);

    dmd.clearScreen(true);
    Serial.println("Display initialized");

    xTaskCreatePinnedToCore(
        wifiTask,
        "WiFiTask",
        8192,
        NULL,
        1,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
        displayTask,
        "DisplayTask",
        8192,
        NULL,
        2,  // Higher priority for display task
        NULL,
        0
    );

    Serial.println("Tasks created successfully");
}

void loop() {
    if (displayRefreshFlag) {
        portENTER_CRITICAL(&timerMux);
        displayRefreshFlag = false;
        portEXIT_CRITICAL(&timerMux);
        dmd.scanDisplayBySPI();
    }
    vTaskDelay(1);
}
