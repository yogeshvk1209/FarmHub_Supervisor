#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

// --- Global Objects ---
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// --- Global Constants & State ---
const int BAT_PIN = 2;    // D0
const int LDR_PIN = 3;    // D1
const int ROUTER_PIN = 4; // D2
const float VOLTAGE_DIVIDER_RATIO = 5.48;
const char* AWS_IOT_TOPIC = "farm/telemetry";
// --- Sending interval
unsigned long lastMillis = 0;
const long interval = 300000; // 15 Min (900000 ms) ; 10 Min (600000 ms) ; 5 Min (300000 ms)
// --- LFP Specific Thresholds ---
const float LFP_CRITICAL = 12.5; 
const float LFP_RECOVERY = 13.1; // "Full enough" to handle the Router's 4G startup spike
// Logic for cooldown
unsigned long lastStateChange = 0;
const long cooldown = 300000; // 5 minute "Anti-Seesaw" timer


// --- Helper: Read Battery ---
float readBattery() {
    long batSum = 0;
    for(int i=0; i<64; i++) { 
        batSum += analogRead(BAT_PIN); 
        delay(1); 
    }
    // Convert ADC to Voltage
    return ((batSum / 64.0) * 3.3 / 4095.0) * VOLTAGE_DIVIDER_RATIO;
}

// --- Helper: Publish to AWS ---
void publishTelemetry(float vbat, int ldr, bool routerOn) {
    StaticJsonDocument<200> doc;
    doc["device_id"] = "FarmHub_XIAO_01";
    doc["v_bat"] = serialized(String(vbat, 2)); 
    doc["ldr_raw"] = ldr;
    doc["router_status"] = routerOn ? 1 : 0;
    doc["uptime"] = millis() / 1000;

    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);

    if (client.publish(AWS_IOT_TOPIC, jsonBuffer)) {
        Serial.println("[SUCCESS] Telemetry Sent to AWS!");
    } else {
        Serial.println("[ERROR] Publish failed!");
    }
}

// --- Network: Connect & Handshake ---
void connectToAWS() {
    // 1. Force a clean slate
    net.stop(); 
    WiFi.disconnect(true);
    delay(1000);
    
    Serial.println("\n--- Starting Network Stack ---");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retryCount = 0;
    const int maxRetries = 120; // 90 seconds timeout

    // 2. The Guarded Connection Loop
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        retryCount++;

        // If the router is slow to boot, don't stay stuck forever
        if (retryCount >= maxRetries) {
            Serial.println("\n[TIMEOUT] Router not ready or SSID not found.");
            Serial.println("[SYSTEM] Rebooting to retry bootstrap sequence...");
            delay(500);
            ESP.restart();
        }
    }

    Serial.println("\n[SUCCESS] WiFi Connected!");

    // 3. DNS & Internet Check
    Serial.println("Verifying Internet Access...");
    int dnsTries = 0;
    while (dnsTries < 10) {
        IPAddress result;
        if (WiFi.hostByName("google.com", result)) {
            Serial.println("[SUCCESS] Internet Reachable.");
            break;
        }
        dnsTries++;
        delay(2000);
        if (dnsTries == 10) {
             Serial.println("[ERROR] DNS Failed. Rebooting...");
             ESP.restart();
        }
    }

    // 4. Time Sync (Critical for SSL)
    configTime(5.5 * 3600, 0, "pool.ntp.org", "time.google.com");
    time_t now = time(nullptr);
    int timeRetry = 0;
    while (now < 10000 && timeRetry < 20) {
        delay(500);
        now = time(nullptr);
        timeRetry++;
    }

    // 5. SSL Handshake
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);
    client.setServer(AWS_IOT_ENDPOINT, 8883);

    if (client.connect("FarmHub_XIAO_01")) {
        Serial.println("[SUCCESS] AWS IoT Connected!");
    } else {
        Serial.printf("[ERROR] AWS IoT Failed. State: %d. Rebooting...\n", client.state());
        delay(2000);
        ESP.restart();
    }
}

// --- Main Setup ---
void setup() {
    Serial.begin(115200);
    delay(2000); 
    Serial.println("\n\n=== FARM HUB BOOTING ===");

    // 1. Power on Router immediately
    pinMode(ROUTER_PIN, OUTPUT);
    digitalWrite(ROUTER_PIN, HIGH); 
    Serial.println("[SYSTEM] Router Power: ENABLED");

    // 2. Buffer for Router Boot
    for(int i = 60; i > 0; i--) {
        if(i % 10 == 0) Serial.printf("Initializing Router... %ds remaining\n", i);
        delay(1000);
    }

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    
    connectToAWS();
}

// --- Main Loop ---
void loop() {
    // 1. Maintain MQTT heartbeat if connected
    if (client.connected()) {
        client.loop();
    }

    // 2. Read Sensors
    float currentBatV = readBattery();
    int currentLDR = analogRead(LDR_PIN);
    bool isDaylight = (currentLDR > 1000); 
    bool currentRouterState = digitalRead(ROUTER_PIN);

    // 3. Decision Engine
    bool shouldBeOn = currentRouterState; // Default to staying in current state

    if (currentBatV < LFP_CRITICAL) {
        shouldBeOn = false; // Emergency Shutdown
    } 
    else if (currentBatV > LFP_RECOVERY) {
        shouldBeOn = isDaylight; // Follow the sun
    } 
    else if (!isDaylight) {
        shouldBeOn = false; // Night time shutdown
    }

    // 4. Execution with Anti-Flap Guard
    if (shouldBeOn != currentRouterState) {
        unsigned long timeSinceLastChange = millis() - lastStateChange;

        if (timeSinceLastChange > cooldown) {
            
            if (shouldBeOn == false) {
                // --- GRACEFUL SHUTDOWN ---
                Serial.println("[SYSTEM] Sending final telemetry before power-cut...");
                publishTelemetry(currentBatV, currentLDR, false);
                delay(3000); // Give WiFi radio time to flush the final packet
                
                digitalWrite(ROUTER_PIN, LOW);
                Serial.println("[LFP LOGIC] Router Powered OFF.");
            } 
            else {
                // --- POWER ON SEQUENCE ---
                digitalWrite(ROUTER_PIN, HIGH);
                Serial.println("[LFP LOGIC] Router Powered ON. Initializing...");
                
                // Wait for router boot before trying to connect
                delay(10000); 
                connectToAWS();
                publishTelemetry(currentBatV, currentLDR, true);
            }

            lastStateChange = millis(); // Reset the cooldown timer
        } else {
            Serial.printf("[GUARD] State change requested but blocked by cooldown. %lus remaining.\n", 
                          (cooldown - timeSinceLastChange) / 1000);
        }
    }

    // 5. Regular Scheduled Telemetry (15 min interval)
    unsigned long now = millis();
    if (now - lastMillis >= interval) {
        lastMillis = now;
        
        // Only attempt to send if the router is actually ON
        if (currentRouterState) {
            if (!client.connected()) {
                connectToAWS();
            }
            publishTelemetry(currentBatV, currentLDR, currentRouterState);
        } else {
            Serial.println("[SYSTEM] Scheduled interval reached, but router is OFF. Skipping telemetry.");
        }
    }
}