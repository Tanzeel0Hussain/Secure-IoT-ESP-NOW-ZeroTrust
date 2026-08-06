#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define BTN            32
#define DEVICE_ID       3   // Change to 4 for second display node

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

typedef struct {
    int   senderID;
    float temp;
    float pressure;
    int   button;
    int   command;
} Message;

float  temp1 = 0, pres1 = 0;
float  temp2 = 0, pres2 = 0;
int    rssi1 = 0, rssi2 = 0;
long   pktCount1 = 0, pktCount2 = 0;
bool   node1Online = false;
bool   node2Online = false;
bool   alertFlag   = false;
unsigned long lastSeen1 = 0;
unsigned long lastSeen2 = 0;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void OnDataRecv(const esp_now_recv_info_t * info, const uint8_t * data, int len) {
    Message incoming;
    memcpy(&incoming, data, sizeof(incoming));

    int rssi = info->rx_ctrl->rssi;

    if (incoming.senderID == 1) {
        temp1       = incoming.temp;
        pres1       = incoming.pressure;
        rssi1       = rssi;
        pktCount1++;
        lastSeen1   = millis();
        node1Online = true;
        if (incoming.button == 1) alertFlag = true;
    }

    if (incoming.senderID == 2) {
        temp2       = incoming.temp;
        pres2       = incoming.pressure;
        rssi2       = rssi;
        pktCount2++;
        lastSeen2   = millis();
        node2Online = true;
        if (incoming.button == 1) alertFlag = true;
    }

    if (incoming.command == 1) {
        alertFlag = false;
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BTN, INPUT_PULLUP);

    Wire.begin(21, 22);
    if (!display.begin(0x3C, true)) {
        Serial.println("OLED not found!");
        while (1);
    }
    display.clearDisplay();
    display.display();

    WiFi.mode(WIFI_STA);
    esp_now_init();
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcastAddress, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void loop() {

    if (millis() - lastSeen1 > 6000) node1Online = false;
    if (millis() - lastSeen2 > 6000) node2Online = false;

    if (digitalRead(BTN) == LOW) {
        Message resetMsg;
        resetMsg.senderID = DEVICE_ID;
        resetMsg.temp     = 0;
        resetMsg.pressure = 0;
        resetMsg.button   = 0;
        resetMsg.command  = 1;
        esp_now_send(broadcastAddress, (uint8_t *)&resetMsg, sizeof(resetMsg));
        alertFlag = false;
        delay(300);
    }

    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("N1:");
    display.print(node1Online ? "ON  " : "OFF ");
    display.print("N2:");
    display.println(node2Online ? "ON" : "OFF");

    display.setCursor(0, 10);
    display.print("T:");
    display.print(temp1, 1);
    display.print("C P:");
    display.print(pres1, 0);
    display.println("hPa");

    display.setCursor(0, 20);
    display.print("T:");
    display.print(temp2, 1);
    display.print("C P:");
    display.print(pres2, 0);
    display.println("hPa");

    display.drawLine(0, 30, 127, 30, SH110X_WHITE);

    display.setCursor(0, 33);
    display.print("R1:");
    display.print(rssi1);
    display.print(" N:");
    display.println(pktCount1);

    display.setCursor(0, 43);
    display.print("R2:");
    display.print(rssi2);
    display.print(" N:");
    display.println(pktCount2);

    display.setCursor(0, 54);
    if (alertFlag) {
        display.println("*** ALERT ***");
    } else {
        display.println("Status: OK");
    }

    display.display();
    delay(1000);
}
