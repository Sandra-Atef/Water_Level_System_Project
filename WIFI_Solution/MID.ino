#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <HTTPClient.h>  // Add this for HTTP requests
#include <Adafruit_SSD1306.h>
#include <HardwareSerial.h>

// WiFi Credentials
const char* ssid = "RTR";
const char* password = "1234567890";

// Receiver ESP32 IP Address (Change this to the actual IP of the receiver ESP32)
const char* receiverIP = "172.20.10.2";  // Replace with actual receiver ESP32 IP

// Server settings
WiFiServer server(80);
WiFiClient client;

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Ultrasonic Sensor using Serial
HardwareSerial mySerial(1);
#define TX 17  
#define RX 16  
// Define the interrupt pin (Connect to a signal source)
#define INTERRUPT_PIN 4  
volatile float middleWaterLevel = 0.0;
volatile bool newData = false;
volatile bool sendDataFlag = false;

// Interrupt Service Routine (ISR)
void IRAM_ATTR triggerSendData() {
    sendDataFlag = true;  // Set flag when new data is available
}
// Function to update the OLED display
void updateDisplay() {
display.clearDisplay();
display.setTextSize(2); // smallest font size
display.setTextColor(WHITE); // this is all you need

// Green area (top of screen, physically green)
display.setCursor(0, 0); // Top area will display as green
display.print("MID Level:");

// White area (middle or bottom of screen)
display.setCursor(0, 20); // Lower = white area
display.print(middleWaterLevel);
display.print(" cm");

display.display();


}

// Function to send data to another ESP32 via HTTP
void sendDataToReceiver() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠ WiFi Disconnected! Reconnecting...");
        WiFi.disconnect();
        WiFi.reconnect();
        return;
    }

    HTTPClient http;
    String serverPath = "http://" + String(receiverIP) + "/update?level=" + String(middleWaterLevel);

    Serial.print("Sending data to receiver: ");
    Serial.println(serverPath);

    http.begin(serverPath);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        Serial.print("✅ Data sent successfully! Response code: ");
        Serial.println(httpResponseCode);
    } else {
        Serial.print("❌ Error sending data: ");
        Serial.println(httpResponseCode);
    }

    http.end();
}

// Function to process HTTP GET requests
void processRequest(String request) {
    Serial.println("Processing request...");
    Serial.println("Raw Request: " + request);

    if (request.indexOf("GET /update?level=") >= 0) {
        int startIndex = request.indexOf("=") + 1;
        int endIndex = request.indexOf(" ", startIndex);

        if (startIndex > 0 && endIndex > startIndex) {
            String levelStr = request.substring(startIndex, endIndex);
            middleWaterLevel = levelStr.toFloat();
            newData = true;
            Serial.println("Extracted Water Level: " + String(middleWaterLevel));
        } else {
            Serial.println("⚠ Invalid request format!");
        }
    }
}


void setup() {
    Serial.begin(115200);
    mySerial.begin(9600, SERIAL_8N1, RX, TX);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
    Serial.print("Server ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
  
    // Start server
    server.begin();

    
    // Configure external interrupt
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), triggerSendData, FALLING);
    
    // Initialize OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("❌ SSD1306 allocation failed!");
        for (;;);
    }
    display.clearDisplay();
    display.display();
}

void loop() {
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
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        client.println();
        client.println("<html><body><h1>Water Level Updated</h1></body></html>");
        client.flush();
        client.stop();
        
          // If an interrupt was triggered, send data
    if (sendDataFlag) {
        sendDataFlag = false;  // Reset flag
        sendDataToReceiver();  // Execute function safely
    }
    }
    
    while (mySerial.available() >= 4) {
    uint8_t buffer[4];
    mySerial.readBytes(buffer, 4);

    if (buffer[0] == 0xFF) {  
        int highByte = buffer[1];
        int lowByte = buffer[2];
        float newReading = (highByte << 8) + lowByte;
        newReading /= 10.0; 

        if (newReading > 2.0 && newReading < 400.0) { 
            middleWaterLevel = (middleWaterLevel * 0.7) + (newReading * 0.3);
            newData = true;
        } else {
            Serial.println("⚠ Out of range reading ignored!");
        }
    } else {
        Serial.println("❌ Invalid Packet - Flushing Buffer...");
        while (mySerial.available()) {
            mySerial.read();  // Clear bad data
        }
    }
}
middleWaterLevel=11.5-middleWaterLevel;

                Serial.print("Filtered Distance: ");
                Serial.print(middleWaterLevel);
                Serial.println(" cm");
    
    if (newData) {
        newData = false;
        updateDisplay();
        sendDataToReceiver();  // Send data to receiver ESP32
    }

    delay(4000);
}