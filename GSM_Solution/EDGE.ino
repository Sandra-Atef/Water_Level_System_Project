#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Ultrasonic Sensor Pins
const int trigPin = 18;
const int echoPin = 19;
volatile long startTime = 0;
volatile long duration = 0;
volatile bool newMeasurement = false;

// Water level variables
volatile float edgeWaterLevel = 0.0;  // Measured by this ESP32
volatile float middleWaterLevel = 0.0; // Received from another ESP32
volatile bool newHTTPDataReceived = false; // Flag for new data

// SIM800L Serial (Using UART1 on ESP32)
#define SIM800_TX 33
#define SIM800_RX 27
SoftwareSerial sim800(SIM800_TX, SIM800_RX);

// Function to send SMS
void sendSMS(String number, String message) {
    Serial.println("Sending SMS...");

    sim800.println("AT");
    delay(1000);
    sim800.println("AT+CMGF=1"); // Set SMS mode to text
    delay(1000);
    sim800.print("AT+CMGS=\"");
    sim800.print(number);
    sim800.println("\"");
    delay(1000);
    sim800.print(message);
    delay(500);
    sim800.write(26); // ASCII for CTRL+Z
    delay(3000);

    Serial.println("SMS Sent!");
}

// Send data via SMS instead of HTTP
void IRAM_ATTR sendDataInterrupt() {
    newHTTPDataReceived = true;
}

// Trigger ultrasonic sensor
void triggerSensor() {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
}

// Echo pin ISR
void IRAM_ATTR echoISR() {
    if (digitalRead(echoPin) == HIGH) {
        startTime = micros();
    } else {
        duration = micros() - startTime;
        newMeasurement = true;
    }
}

// Update OLED display
void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);

    display.setCursor(0, 0);
    display.print("EDGE LEVEL:");
    display.setCursor(0, 20);
    display.print(edgeWaterLevel);
    display.print("cm");

    display.display();
}

// Extract middleWaterLevel from HTTP GET request
void processRequest(String request) {
    if (request.indexOf("GET /update?level=") >= 0) {
        int startIndex = request.indexOf("=") + 1;
        int endIndex = request.indexOf(" ", startIndex);

        if (startIndex > 0 && endIndex > startIndex) {
            String levelStr = request.substring(startIndex, endIndex);
            middleWaterLevel = levelStr.toFloat();
            sendDataInterrupt();  // Set flag to send SMS
            Serial.println("Received middle_data: " + String(middleWaterLevel));
        } else {
            Serial.println("⚠ Invalid request format!");
        }
    }
}

// WiFi AP settings
const char* apSSID = "ESP32_HOTSPOT";
const char* apPassword = "12345678";

// Server settings
WiFiServer server(80);
WiFiClient client;

void setup() {
    Serial.begin(115200);

    // Start SIM800L
    sim800.begin(9600);
    Serial.println("📡 SIM800L Initialized");

    // Create Hotspot
    WiFi.softAP(apSSID, apPassword);
    Serial.println("✅ Hotspot Created!");
    Serial.print("Hotspot IP: ");
    Serial.println(WiFi.softAPIP());

    // Start server
    server.begin();

    // Initialize OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("❌ SSD1306 allocation failed!");
        for (;;);
    }
    display.clearDisplay();
    display.display();

    // Configure ultrasonic sensor pins
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);
}

void loop() {
    // Trigger ultrasonic sensor every 5 seconds
    static unsigned long lastTrigger = 0;
    if (millis() - lastTrigger >= 5000) {
        lastTrigger = millis();
        triggerSensor();
    }

    // If new ultrasonic data is available, calculate distance
    if (newMeasurement) {
        newMeasurement = false;
        float distance = duration * 0.034 / 2.0;
        edgeWaterLevel = distance;
        Serial.print("Edge Level: ");
        Serial.print(edgeWaterLevel);
        Serial.println(" cm");

        updateDisplay();
    }

    // Handle HTTP requests
    client = server.available();
    if (client) {
        Serial.println("📡 Client connected");
        String request = "";

        while (client.available()) {
            char c = client.read();
            request += c;
            if (c == '\n') break;
        }

        Serial.println("Request received: " + request);
        processRequest(request);

        // Response
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<html><body><h1>Water Level Updated</h1></body></html>");

        client.flush();
        client.stop();
    }

    // Send SMS when new middleWaterLevel data is received
    if (newHTTPDataReceived) {
        newHTTPDataReceived = false;

        String message = "Edge: " + String(edgeWaterLevel, 2) + " cm | Middle: " + String(middleWaterLevel, 2) + " cm";
        sendSMS("+201270880603", message);
    }

    // Track previous values (optional optimization)
    static float lastEdgeLevel = 0.0;
    static float lastMiddleLevel = 0.0;
    if (edgeWaterLevel != lastEdgeLevel || middleWaterLevel != lastMiddleLevel) {
        lastEdgeLevel = edgeWaterLevel;
        lastMiddleLevel = middleWaterLevel;
    }
}
