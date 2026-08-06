#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

#define BTN 4
#define DEVICE_ID 1   // Set to 1 for Sensor 1, 2 for Sensor 2

Adafruit_BMP280 bmp;

typedef struct {
    int senderID;
    float temp;
    float pressure;
    int button;
    int command;
} Message;

Message msg;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int alertLatch = 0;

void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t * data, int len) {
    Message incoming;
    memcpy(&incoming, data, sizeof(incoming));
    if(incoming.command == 1) alertLatch = 0; 
}

void setup() {
    pinMode(BTN, INPUT_PULLUP);
    Wire.begin(21, 22);
    WiFi.mode(WIFI_STA);
    esp_now_init();
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcastAddress, 6);
    esp_now_add_peer(&peer);

    if (!bmp.begin(0x76)) {
        while (1); // Halt if sensor not found
    }
    msg.senderID = DEVICE_ID;
}

void loop() {
    if (digitalRead(BTN) == LOW) alertLatch = 1;

    msg.temp = bmp.readTemperature();
    msg.pressure = bmp.readPressure() / 100.0F;
    msg.button = alertLatch;
    msg.command = 0;

    esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
    delay(1000); 
}
