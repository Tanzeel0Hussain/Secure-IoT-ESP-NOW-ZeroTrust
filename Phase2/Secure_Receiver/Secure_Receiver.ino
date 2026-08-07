// ============================================================
// PHASE 2 - SECURE DISPLAY NODE (Zero Trust Implementation)
// Upload to Display Node 3 & 4
// Change DEVICE_ID to 3 or 4 before uploading
//
// Zero Trust Checks on every received packet:
//   CHECK 1: HMAC-SHA256 verification  → reject if invalid
//   CHECK 2: Timestamp freshness       → reject if > 5 seconds old
//   CHECK 3: Sequence number           → reject if replay detected
//   Only packets passing ALL 3 checks are processed
// ============================================================

#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "mbedtls/aes.h"
#include "mbedtls/md.h"

// ── PIN DEFINITIONS ──────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define BTN            32
#define DEVICE_ID       3   // Change to 4 for second display node

// ── OLED DISPLAY ─────────────────────────────────────────────────────────────
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── SECURE MESSAGE STRUCTURE (must match Secure_Sender.ino exactly) ──────────
typedef struct {
    int      senderID;
    uint32_t timestamp;
    uint32_t seqNumber;
    float    temp;
    float    pressure;
    int      button;
    int      command;
    uint8_t  hmac[32];
} SecureMessage;

// ── PRE-SHARED KEY (must be identical to Secure_Sender.ino) ─────────────────
const uint8_t AES_KEY[16] = {
    0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F, 0xA0, 0xB1,
    0xC2, 0xD3, 0xE4, 0xF5, 0x96, 0x87, 0x78, 0x69
};

// ── NETWORK STATE ─────────────────────────────────────────────────────────────
float  temp1 = 0, pres1 = 0;
float  temp2 = 0, pres2 = 0;
int    rssi1 = 0, rssi2 = 0;
long   pktCount1  = 0, pktCount2 = 0;
long   rejCount   = 0;   // Rejected packet counter
bool   node1Online = false, node2Online = false;
bool   alertFlag   = false;
unsigned long lastSeen1 = 0, lastSeen2 = 0;
uint32_t      lastSeq1  = 0, lastSeq2  = 0;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t nodeStartTime = 0;

// ── FUNCTION: Verify HMAC-SHA256 ─────────────────────────────────────────────
bool verifyHMAC(SecureMessage* msg) {
    uint8_t computed[32];

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, AES_KEY, 16);
    mbedtls_md_hmac_update(&ctx, (uint8_t*)msg, sizeof(SecureMessage) - 32);
    mbedtls_md_hmac_finish(&ctx, computed);
    mbedtls_md_free(&ctx);

    // Constant-time comparison (prevents timing attacks)
    int diff = 0;
    for (int i = 0; i < 32; i++) {
        diff |= (computed[i] ^ msg->hmac[i]);
    }
    return diff == 0;
}

// ── FUNCTION: AES-128-CBC Decrypt payload ────────────────────────────────────
void decryptPayload(SecureMessage* msg) {
    // Derive same IV used during encryption
    uint8_t iv[16] = {0};
    memcpy(iv,     &msg->timestamp, 4);
    memcpy(iv + 4, &msg->seqNumber, 4);

    // Pack encrypted fields
    uint8_t cipher[16];
    memcpy(cipher,      &msg->temp,     4);
    memcpy(cipher + 4,  &msg->pressure, 4);
    memcpy(cipher + 8,  &msg->button,   4);
    memcpy(cipher + 12, &msg->command,  4);

    // Decrypt
    uint8_t plain[16];
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv, cipher, plain);
    mbedtls_aes_free(&aes);

    // Put decrypted values back
    memcpy(&msg->temp,     plain,      4);
    memcpy(&msg->pressure, plain + 4,  4);
    memcpy(&msg->button,   plain + 8,  4);
    memcpy(&msg->command,  plain + 12, 4);
}

// ── ESP-NOW RECEIVE CALLBACK ─────────────────────────────────────────────────
void OnDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {

    // Wrong packet size - reject immediately
    if (len != sizeof(SecureMessage)) {
        rejCount++;
        return;
    }

    SecureMessage msg;
    memcpy(&msg, data, sizeof(msg));
    int rssi = info->rx_ctrl->rssi;

    // ── ZERO TRUST CHECK 1: HMAC Verification ────────────────────────────
    if (!verifyHMAC(&msg)) {
        rejCount++;
        Serial.print("[ZT REJECT] HMAC failed  senderID=");
        Serial.println(msg.senderID);
        return;  // DROP - could be injection or tampering
    }

    // ── ZERO TRUST CHECK 2: Timestamp Freshness (max 5 seconds old) ──────
    uint32_t currentTime = (millis() / 1000) + nodeStartTime;
    int32_t  timeDelta   = (int32_t)(currentTime - msg.timestamp);
    if (timeDelta > 5 || timeDelta < -5) {
        rejCount++;
        Serial.print("[ZT REJECT] Timestamp out of range  delta=");
        Serial.print(timeDelta);
        Serial.println("s  (possible replay)");
        return;  // DROP - replay attack
    }

    // ── ZERO TRUST CHECK 3: Sequence Number (anti-replay) ─────────────────
    if (msg.senderID == 1 && msg.seqNumber <= lastSeq1) {
        rejCount++;
        Serial.println("[ZT REJECT] Sequence replay from Node 1");
        return;  // DROP - replayed packet
    }
    if (msg.senderID == 2 && msg.seqNumber <= lastSeq2) {
        rejCount++;
        Serial.println("[ZT REJECT] Sequence replay from Node 2");
        return;  // DROP - replayed packet
    }

    // ── ALL CHECKS PASSED - Decrypt and Process ───────────────────────────
    decryptPayload(&msg);

    if (msg.senderID == 1) {
        temp1       = msg.temp;
        pres1       = msg.pressure;
        rssi1       = rssi;
        pktCount1++;
        lastSeen1   = millis();
        lastSeq1    = msg.seqNumber;
        node1Online = true;
        if (msg.button == 1) alertFlag = true;
        Serial.print("[ZT PASS] Node1  T="); Serial.print(temp1,1);
        Serial.print("  P="); Serial.println(pres1,0);
    }

    if (msg.senderID == 2) {
        temp2       = msg.temp;
        pres2       = msg.pressure;
        rssi2       = rssi;
        pktCount2++;
        lastSeen2   = millis();
        lastSeq2    = msg.seqNumber;
        node2Online = true;
        if (msg.button == 1) alertFlag = true;
        Serial.print("[ZT PASS] Node2  T="); Serial.print(temp2,1);
        Serial.print("  P="); Serial.println(pres2,0);
    }

    if (msg.command == 1) alertFlag = false;
}

// ── SETUP ────────────────────────────────────────────────────────────────────
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

    nodeStartTime = millis() / 1000;

    Serial.println("================================================");
    Serial.println("  Phase 2 Secure Display Node Ready");
    Serial.print("  DEVICE_ID = "); Serial.println(DEVICE_ID);
    Serial.println("  Zero Trust: HMAC + Timestamp + SeqNum Active");
    Serial.println("================================================");
}

// ── MAIN LOOP ────────────────────────────────────────────────────────────────
void loop() {

    // ── Offline detection ─────────────────────────────────────────────────
    if (millis() - lastSeen1 > 6000) node1Online = false;
    if (millis() - lastSeen2 > 6000) node2Online = false;

    // ── Reset button ──────────────────────────────────────────────────────
    if (digitalRead(BTN) == LOW) {
        alertFlag = false;
        delay(300);
    }

    // ── Draw Display ──────────────────────────────────────────────────────
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // Row 1: ZT mode + node status
    display.setCursor(0, 0);
    display.print("[ZT] N1:");
    display.print(node1Online ? "OK " : "-- ");
    display.print("N2:");
    display.println(node2Online ? "OK" : "--");

    // Row 2: Sensor Node 1 data
    display.setCursor(0, 10);
    display.print("T:");
    display.print(temp1, 1);
    display.print("C P:");
    display.print(pres1, 0);
    display.println("hPa");

    // Row 3: Sensor Node 2 data
    display.setCursor(0, 20);
    display.print("T:");
    display.print(temp2, 1);
    display.print("C P:");
    display.print(pres2, 0);
    display.println("hPa");

    // Divider
    display.drawLine(0, 30, 127, 30, SH110X_WHITE);

    // Row 4: Accepted packets + rejected packets
    display.setCursor(0, 33);
    display.print("OK:");
    display.print(pktCount1 + pktCount2);
    display.print(" REJ:");
    display.println(rejCount);

    // Row 5: RSSI values
    display.setCursor(0, 43);
    display.print("R1:");
    display.print(rssi1);
    display.print(" R2:");
    display.println(rssi2);

    // Row 6: Alert or Secure OK
    display.setCursor(0, 54);
    if (alertFlag) {
        display.println("*** ALERT ***");
    } else {
        display.println("ZTA: SECURE OK");
    }

    display.display();
    delay(1000);
}
