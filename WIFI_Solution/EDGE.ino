#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>

// WiFi Credentials
const char* ssid = "RTR";
const char* password = "1234567890";

// Server settings
WiFiServer server(80);
WiFiClient client;

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Ultrasonic Sensor Pins92.168
const int trigPin = 18;
const int echoPin = 19;
volatile long startTime = 0;
volatile long duration = 0;
volatile bool newMeasurement = false;

const char* dataServerURL = "https://robir.pythonanywhere.com/success";

// Water level variables
volatile float edgeWaterLevel = 0.0;  // Measured by this ESP32
volatile float middleWaterLevel = 0.0; // Received from another ESP32
volatile bool newHTTPDataReceived = false; // Flag for new HTTP data

// Interrupt Service Routine (ISR) for Echo Pin
void IRAM_ATTR echoISR() {
    if (digitalRead(echoPin) == HIGH) {
        startTime = micros();  // Store start time
    } else {
        duration = micros() - startTime;  // Measure pulse width
        newMeasurement = true;
    }
}

// Function to send data to the server when new data is received
void IRAM_ATTR sendDataInterrupt() {
    newHTTPDataReceived = true; // Set flag when new HTTP data is received
}

// Function to send edgeWaterLevel to the server
void sendDataToURL(const char* serverURL, float edgeLevel, float receivedLevel) {
    HTTPClient http;
    String requestURL = String(serverURL) + "?edge_level=" + String(edgeLevel) + "&received_level=" + String(receivedLevel);

    Serial.print("Sending data to: ");
    Serial.println(requestURL);

    http.begin(requestURL); // Initialize request
    int httpResponseCode = http.GET(); // Perform GET request

    if (httpResponseCode > 0) {
        Serial.print("✅ Data sent successfully! Response code: ");
        Serial.println(httpResponseCode);
    } else {
        Serial.print("❌ Error sending data: ");
        Serial.println(httpResponseCode);
    }

    http.end(); // Close connection
}

// Function to trigger the ultrasonic sensor
void triggerSensor() {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
}

// Function to update the OLED display
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

// Function to process HTTP GET requests and extract middleWaterLevel
void processRequest(String request) {
    if (request.indexOf("GET /update?level=") >= 0) {
        int startIndex = request.indexOf("=") + 1;
        int endIndex = request.indexOf(" ", startIndex);

        if (startIndex > 0 && endIndex > startIndex) {
            String levelStr = request.substring(startIndex, endIndex);
            middleWaterLevel = levelStr.toFloat();
            sendDataInterrupt();  // Trigger data sending when new HTTP data is received
            Serial.println("Received middle_data: " + String(middleWaterLevel));
        } else {
            Serial.println("⚠ Invalid request format!");
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ Connected to WiFi!");

    // Print Server IP Address
    Serial.print("Server ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

    // Start server
    server.begin();

    // Initialize OLED display
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
        edgeWaterLevel = 11.5 - distance;
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
            if (c == '\n') break;  // Stop at newline
        }

        Serial.println("Request received: " + request);
        processRequest(request);  // This will trigger data sending if new middle level data is received

        // Send response to client
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<html><body><h1>Water Level Updated</h1></body></html>");

        client.flush();
        client.stop();
    }

    // Send data when new middleWaterLevel data is received
    if (newHTTPDataReceived) {
        newHTTPDataReceived = false;  // Reset flag
        sendDataToURL(dataServerURL, edgeWaterLevel, middleWaterLevel);
    }

    // Update OLED display only when necessary
    static float lastEdgeLevel = 0.0;
    static float lastMiddleLevel = 0.0;

    if (edgeWaterLevel != lastEdgeLevel || middleWaterLevel != lastMiddleLevel) {
        lastEdgeLevel = edgeWaterLevel;
        lastMiddleLevel = middleWaterLevel;
        
    }
}