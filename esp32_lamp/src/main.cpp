#include "SerialHandler.h"
#include "config.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>

SerialHandler SH;

int red;
int green;
int blue;
String command;

// Replace with your network credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

WebServer server(80); // Web server running on port 80

IPAddress local_IP(192, 168, 1, 150);
IPAddress gateway(192, 168, 1, 1);    // Usually your router's IP
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress primaryDNS(192, 168, 1, 1); // Optional: Primary DNS (usually your router)

String lastColor = "#ff0000"; // Default color (Red)
bool alarmEnabled = false;
String alarmTime = "";
int currentBrightness = 100;

String alarmFilePath = "/alarm.txt";
String brightnessFilePath = "/brightness.txt";

// Function to save the color to SPIFFS
void saveColor(String color) {
    File file = SPIFFS.open("/color.txt", "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(color); // Save the color as a hex string
    file.close();
    lastColor = color;
}

// Function to load the last saved color from SPIFFS
String loadColor() {
    if (SPIFFS.exists("/color.txt")) {
        File file = SPIFFS.open("/color.txt", "r");
        if (!file) {
            Serial.println("Failed to open file for reading");
            return lastColor; // Return default color if loading fails
        }
        String color = file.readString();
        file.close();
        return color;
    }
    return lastColor; // Return default if file doesn't exist
}

void saveAlarm() {
    File file = SPIFFS.open(alarmFilePath, "w");
    if (!file) {
        Serial.println("Failed to open alarm file for writing");
        return;
    }
    file.println(alarmEnabled ? "enabled=1" : "enabled=0");
    file.println("time=" + alarmTime);
    file.close();
}

void loadAlarm() {
    if (!SPIFFS.exists(alarmFilePath)) {
        return;
    }
    File file = SPIFFS.open(alarmFilePath, "r");
    if (!file) {
        Serial.println("Failed to open alarm file for reading");
        return;
    }
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.startsWith("enabled=")) {
            String v = line.substring(8);
            alarmEnabled = (v == "1");
        } else if (line.startsWith("time=")) {
            alarmTime = line.substring(5);
        }
        file.close();
    }
}

void saveBrightness() {
    File file = SPIFFS.open(brightnessFilePath, "w");
    if (!file) {
        Serial.println("Failed to open brightness file for writing");
        return;
    }
    file.println(String(currentBrightness));
    file.close();
}

void loadBrightness() {
    if (!SPIFFS.exists(brightnessFilePath)) {
        currentBrightness = 100;
        return;
    }
    File file = SPIFFS.open(brightnessFilePath, "r");
    if (!file) {
        Serial.println("Failed to open brightness file for reading");
        return;
    }
    String line = file.readStringUntil('\n');
    line.trim();
    currentBrightness = constrain(line.toInt(), 1, 100);
    file.close();
}
void setup() {
    // Start all serial ports
    Serial.begin(115200);
    Serial1.begin(115200);

    SH.setSerial(Serial1);

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
        Serial.println("STA Failed to configure");
    }

    // Connect to Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount file system");
        return;
    }

    // Load the last saved color from SPIFFS
    lastColor = loadColor();
    loadAlarm();
    loadBrightness();

    // !! This is not tested
    red = strtol(&lastColor[1], NULL, 16);
    green = strtol(&lastColor[3], NULL, 16);
    blue = strtol(&lastColor[5], NULL, 16);

    // OTA Setup
    ArduinoOTA.setHostname("ESP32-OTA");
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });
    ArduinoOTA.begin(); // Initialize OTA

    // Serve the index.html file
    server.on("/", HTTP_GET, []() {
        File file = SPIFFS.open("/index.html", "r");
        if (!file) {
            server.send(404, "text/plain", "File Not Found");
            return;
        }
        server.streamFile(file, "text/html");
        file.close();
    });

    // Serve the CSS file
    server.on("/styles.css", HTTP_GET, []() {
        File file = SPIFFS.open("/styles.css", "r");
        if (!file) {
            server.send(404, "text/plain", "File Not Found");
            return;
        }
        server.streamFile(file, "text/css");
        file.close();
    });

    // Serve the JS file
    server.on("/scripts.js", HTTP_GET, []() {
        File file = SPIFFS.open("/scripts.js", "r");
        if (!file) {
            server.send(404, "text/plain", "File Not Found");
            return;
        }
        server.streamFile(file, "application/javascript");
        file.close();
    });

    // Serve the last saved color
    server.on("/getColor", HTTP_GET, []() {
        server.send(200, "text/plain", lastColor); // Send the saved color to the client
    });

    // Serve the saved alarm settings as JSON
    server.on("/getAlarm", HTTP_GET, []() {
        String json = "{";
        json += "\"enabled\":" + String(alarmEnabled ? "true" : "false") + ",";
        json += "\"time\":\"" + alarmTime + "\",";
        json += "\"brightness\":100";
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/getBrightness", HTTP_GET, []() {
        server.send(200, "text/plain", String(currentBrightness));
    });

    // Update alarm settings
    server.on("/setAlarm", HTTP_GET, []() {
        if (server.hasArg("enabled")) {
            String v = server.arg("enabled");
            alarmEnabled = (v == "1" || v == "true");
        }
        if (server.hasArg("time")) {
            alarmTime = server.arg("time");
        }
        saveAlarm();
        server.send(200, "text/plain", "Alarm saved");
    });

    // Generic command handler (for commands 1, 2, 3, 4)
    server.on("/command", HTTP_GET, []() {
        if (server.hasArg("value")) {
            command = server.arg("value");

            SH.p("<").p("M" + command).pln(">");
            Serial.println("Command " + command + " activated.");

            // Send a response back to the client
            server.send(200, "text/plain", "Command " + command + " activated.");
        } else {
            server.send(400, "text/plain", "Bad Request: No command specified.");
        }
    });

    server.on("/color", HTTP_GET, []() {
        if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
            String rString = server.arg("r");
            String gString = server.arg("g");
            String bString = server.arg("b");

            red = rString.toInt();
            green = gString.toInt();
            blue = bString.toInt();

            int maxRGB = max(red, max(green, blue));
            currentBrightness = constrain(map(maxRGB, 0, 255, 0, 100), 1, 100);
            saveBrightness();

            // Save the color as a hex string
            String colorHex = "#" +
                              (red < 16 ? "0" + String(red, HEX) : String(red, HEX)) +
                              (green < 16 ? "0" + String(green, HEX) : String(green, HEX)) +
                              (blue < 16 ? "0" + String(blue, HEX) : String(blue, HEX));

            saveColor(colorHex);

            // Send the color to the Teensy
            SH.p("<").p("R").p(red).pln(">");
            SH.p("<").p("G").p(green).pln(">");
            SH.p("<").p("B").p(blue).pln(">");

            // Handle the RGB values as needed (send to LEDs, etc.)
            Serial.print("Color changed to: ");
            Serial.print("R: ");
            Serial.print(red);
            Serial.print(", G: ");
            Serial.print(green);
            Serial.print(", B: ");
            Serial.println(blue);

            // Send a response back to the client
            server.send(200, "text/plain", "Color updated successfully");
        } else {
            // If required arguments are missing, return an error
            server.send(400, "text/plain", "Bad Request: Missing color arguments");
        }
    });

    // Brightness handler
    server.on("/brightness", HTTP_GET, []() {
        if (server.hasArg("value")) {
            String brightnessStr = server.arg("value");
            int brightness = brightnessStr.toInt(); // Convert the brightness argument to an integer

            brightness = map(constrain(brightness, 0, 100), 0, 100, 0, 255); // Map the brightness value to 0-255
            currentBrightness = constrain(brightnessStr.toInt(), 1, 100);
            saveBrightness();

            int colors[3] = {red, green, blue}; // Store the current RGB values in an array

            // Find index of the maximum value in the colors array
            int maxIndex = 0;

            for (int i = 1; i < 3; i++) {
                if (colors[i] > colors[maxIndex]) {
                    maxIndex = i;
                }
            }
            colors[maxIndex] = brightness; // Set the maximum value to the brightness value

            // Scale the other values to maintain the same ratio
            for (int i = 0; i < 3; i++) {
                if (i != maxIndex) {
                    colors[i] = constrain(map(colors[i], 0, colors[maxIndex], 0, brightness), 0, 255);
                }
            }

            red = colors[0];
            green = colors[1];
            blue = colors[2];

            // Save the new color as a hex string
            String colorHex = "#" +
                              (red < 16 ? "0" + String(red, HEX) : String(red, HEX)) +
                              (green < 16 ? "0" + String(green, HEX) : String(green, HEX)) +
                              (blue < 16 ? "0" + String(blue, HEX) : String(blue, HEX));

            saveColor(colorHex);

            // Send the color to the Teensy
            SH.p("<").p("R").p(red).pln(">");
            SH.p("<").p("G").p(green).pln(">");
            SH.p("<").p("B").p(blue).pln(">");

            // Handle the RGB values as needed (send to LEDs, etc.)
            Serial.print("Color changed to: ");
            Serial.print("R: ");
            Serial.print(red);
            Serial.print(", G: ");
            Serial.print(green);
            Serial.print(", B: ");
            Serial.println(blue);

            server.send(200, "text/plain", "Brightness set to " + String(brightness) + ". Color is" + String(red) + " " + String(green) + " " + String(blue));
        } else {
            server.send(400, "text/plain", "Bad Request: No command specified.");
        }
    });

    // Start the server
    server.begin();
    Serial.print("Server started on ");
    Serial.println(WiFi.localIP());
}
void loop() {
    ArduinoOTA.handle(); // Listen for OTA updates

    server.handleClient();
    static long sendTimer = 0;
    if (millis() - sendTimer > 1000) {
        sendTimer = millis();
        SH.p("<").p("R").p(red).pln(">");
        SH.p("<").p("G").p(green).pln(">");
        SH.p("<").p("B").p(blue).pln(">");
        SH.p("<").p("M").p(command).pln(">");
    }
    SH.update();
    static long printTimer = 0;
    if (millis() - printTimer > 2000) {
        printTimer = millis();
        Serial.print("Server running on ");
        Serial.println(WiFi.localIP());
    }
}
