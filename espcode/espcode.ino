#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// WiFi credentials
const char* ssid = "Xiaomi 11i";  
const char* password = "12345670";  

// Flask server URL
const char* serverUrl = "http://192.168.139.97:5000/upload";

// Camera pin configuration for AI-THINKER ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Timer variables
unsigned long previousMillis = 0;
const long interval = 5000;  // Capture & send every 5 seconds

void setupCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // Initialize the camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera Init Failed! Error: 0x%x\n", err);
        return;
    }
    
    Serial.println("Camera Initialized Successfully!");
    
    // Set lower resolution if needed for better stability
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, FRAMESIZE_VGA);  // Lower resolution
    }
}

// Function to upload image to server
void uploadImage(camera_fb_t * fb) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return;
    }
    
    HTTPClient http;
    
    Serial.println("Uploading image...");
    http.begin(serverUrl);
    
    // Create a simple header for the multipart form
    http.addHeader("Content-Type", "multipart/form-data; boundary=ESP32CAM");
    
    // Create multipart form data body manually
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    
    // We'll build our request with the right multipart boundaries
    const char* head_template = 
      "--ESP32CAM\r\n"
      "Content-Disposition: form-data; name=\"image\"; filename=\"esp32cam.jpg\"\r\n"
      "Content-Type: image/jpeg\r\n\r\n";
    
    const char* tail = "\r\n--ESP32CAM--\r\n";
    
    // Calculate total length
    uint32_t imageLen = fbLen;
    uint32_t headLen = strlen(head_template);
    uint32_t tailLen = strlen(tail);
    uint32_t totalLen = headLen + imageLen + tailLen;
    
    // Allocate memory for our complete request body
    uint8_t *buffer = (uint8_t *)malloc(totalLen);
    if (!buffer) {
        Serial.println("Not enough memory to allocate buffer");
        http.end();
        return;
    }
    
    // Build the complete request body
    uint32_t pos = 0;
    
    // Copy head
    memcpy(buffer, head_template, headLen);
    pos += headLen;
    
    // Copy image data
    memcpy(buffer + pos, fbBuf, fbLen);
    pos += fbLen;
    
    // Copy tail
    memcpy(buffer + pos, tail, tailLen);
    
    // Send the request
    int httpResponseCode = http.POST(buffer, totalLen);
    
    // Free memory
    free(buffer);
    
    // Process response
    String payload = http.getString();
    
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode > 0) {
        Serial.println("Image uploaded successfully");
        Serial.println(payload);
    } else {
        Serial.print("Error on sending POST: ");
        Serial.println(http.errorToString(httpResponseCode));
    }
    
    http.end();
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();
    
    // Give serial time to settle
    delay(100);
    
    // Initialize camera first
    setupCamera();
    
    // Connect to WiFi second
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
        delay(500);
        Serial.print(".");
        attempt++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect to WiFi!");
    }
}

void loop() {
    unsigned long currentMillis = millis();
    
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        // Capture image
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed!");
            return;
        }
        
        Serial.println("Camera capture success!");
        Serial.printf("Image size: %d bytes\n", fb->len);
        
        // Upload image
        uploadImage(fb);
        
        // Return the frame buffer to be reused
        esp_camera_fb_return(fb);
    }
}