// ============================================================
// PHASE 1 - ATTACK 1: PASSIVE PACKET SNIFFER
// Upload to the 5th (attack) ESP32
// Open Serial Monitor at 115200 baud to see captured packets
// ============================================================

#include <WiFi.h>
#include "esp_wifi.h"

long packetCount = 0;

// This function runs automatically for every packet in the air
void sniffer_callback(void* buf, wifi_promiscuous_pkt_type_t type) {

    // Only process management frames (where ESP-NOW packets appear)
    if (type != WIFI_PKT_MGMT) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;

    int  rssi      = pkt->rx_ctrl.rssi;
    int  length    = pkt->rx_ctrl.sig_len;
    long timestamp = millis();
    packetCount++;

    Serial.print("[PACKET CAPTURED]");
    Serial.print("  #");        Serial.print(packetCount);
    Serial.print("  Time: ");   Serial.print(timestamp);  Serial.print(" ms");
    Serial.print("  RSSI: ");   Serial.print(rssi);       Serial.print(" dBm");
    Serial.print("  Length: "); Serial.print(length);     Serial.println(" bytes");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("");
    Serial.println("============================================");
    Serial.println("  ESP-NOW PASSIVE SNIFFER");
    Serial.println("  Attack Demo - Phase 1");
    Serial.println("  Observing all packets on channel 1");
    Serial.println("============================================");
    Serial.println("");

    // No network association needed
    WiFi.mode(WIFI_MODE_NULL);

    // Enable promiscuous mode - capture ALL frames in air
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(sniffer_callback);

    Serial.println("[*] Sniffer active. Waiting for packets...");
    Serial.println("[*] Run your sensor nodes to see packets appear below.");
    Serial.println("");
}

void loop() {
    // All work is done in the interrupt callback
    // Nothing needed here
}
