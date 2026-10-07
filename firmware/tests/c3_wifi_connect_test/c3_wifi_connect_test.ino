#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

// ============================================================
// EMI - ESP32-C3 Wi-Fi connection diagnostic
// Board: ESP32C3 Dev Module
//
// IMPORTANT:
// - Keep your existing private secrets.h beside this sketch.
// - Do not share or commit secrets.h.
// - This test tries a normal connection first.
// - If that fails, it scans for the configured SSID and retries
//   using the exact 2.4 GHz channel + BSSID that the C3 can see.
// ============================================================

volatile int lastDisconnectReason = -1;

const char* reasonName(int reason) {
  switch (reason) {
    case 2:   return "AUTH_EXPIRE";
    case 3:   return "AUTH_LEAVE";
    case 4:   return "ASSOC_EXPIRE";
    case 5:   return "ASSOC_TOOMANY";
    case 6:   return "NOT_AUTHED";
    case 7:   return "NOT_ASSOCED";
    case 8:   return "ASSOC_LEAVE";
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND";
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    default:  return "OTHER";
  }
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    Serial.println("EVENT: associated with access point");
    return;
  }

  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.print("EVENT: got IP ");
    Serial.println(WiFi.localIP());
    return;
  }

  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastDisconnectReason = info.wifi_sta_disconnected.reason;

    Serial.print("EVENT: disconnected reason=");
    Serial.print(lastDisconnectReason);
    Serial.print(" (");
    Serial.print(reasonName(lastDisconnectReason));
    Serial.println(")");
  }
}

bool waitForConnection(unsigned long timeoutMs) {
  unsigned long started = millis();

  while (millis() - started < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }

    delay(100);
  }

  return WiFi.status() == WL_CONNECTED;
}

void printSuccess() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("SUCCESS: ESP32-C3 IS CONNECTED TO WI-FI");
  Serial.println("========================================");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  Serial.print("Channel: ");
  Serial.println(WiFi.channel());

  Serial.println();
}

bool findBestTarget(
  int32_t &channel,
  uint8_t bssid[6],
  int32_t &rssi
) {
  Serial.println();
  Serial.println("Scanning for the configured SSID...");

  int count = WiFi.scanNetworks(false, true);

  if (count <= 0) {
    Serial.print("Scan failed or found nothing. result=");
    Serial.println(count);
    return false;
  }

  int bestIndex = -1;
  int32_t bestRssi = -1000;

  for (int i = 0; i < count; i++) {
    if (WiFi.SSID(i) == EMI_WIFI_SSID && WiFi.RSSI(i) > bestRssi) {
      bestIndex = i;
      bestRssi = WiFi.RSSI(i);
    }
  }

  if (bestIndex < 0) {
    WiFi.scanDelete();
    Serial.println("Configured SSID was NOT found.");
    return false;
  }

  channel = WiFi.channel(bestIndex);
  rssi = WiFi.RSSI(bestIndex);

  const uint8_t *sourceBssid = WiFi.BSSID(bestIndex);
  memcpy(bssid, sourceBssid, 6);

  Serial.print("Target found. RSSI=");
  Serial.print(rssi);
  Serial.print(" dBm channel=");
  Serial.print(channel);
  Serial.print(" BSSID=");

  for (int i = 0; i < 6; i++) {
    if (i > 0) {
      Serial.print(':');
    }

    if (bssid[i] < 16) {
      Serial.print('0');
    }

    Serial.print(bssid[i], HEX);
  }

  Serial.println();

  WiFi.scanDelete();
  return true;
}

void setup() {
  Serial.begin(115200);

  unsigned long serialWaitStarted = millis();

  while (!Serial && millis() - serialWaitStarted < 5000) {
    delay(10);
  }

  delay(300);

  Serial.println();
  Serial.println("EMI C3 WI-FI CONNECTION TEST");
  Serial.println("============================");

  WiFi.onEvent(onWiFiEvent);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  WiFi.disconnect(false, true);
  delay(500);

  // ----------------------------------------------------------
  // ATTEMPT 1: normal connection
  // ----------------------------------------------------------

  lastDisconnectReason = -1;

  Serial.println();
  Serial.println("ATTEMPT 1: normal Wi-Fi connection");
  Serial.print("SSID: ");
  Serial.println(EMI_WIFI_SSID);

  WiFi.begin(
    EMI_WIFI_SSID,
    EMI_WIFI_PASSWORD
  );

  if (waitForConnection(15000)) {
    printSuccess();
    return;
  }

  Serial.println();
  Serial.println("ATTEMPT 1 FAILED.");

  Serial.print("WiFi.status()=");
  Serial.println((int)WiFi.status());

  Serial.print("Last disconnect reason=");
  Serial.print(lastDisconnectReason);
  Serial.print(" (");
  Serial.print(reasonName(lastDisconnectReason));
  Serial.println(")");

  WiFi.disconnect(false, false);
  delay(1000);

  // ----------------------------------------------------------
  // ATTEMPT 2: scan, then connect to exact visible 2.4 GHz AP
  // ----------------------------------------------------------

  int32_t channel = 0;
  int32_t rssi = -1000;
  uint8_t bssid[6] = {0, 0, 0, 0, 0, 0};

  if (!findBestTarget(channel, bssid, rssi)) {
    Serial.println();
    Serial.println("TEST STOPPED: target SSID could not be found.");
    return;
  }

  lastDisconnectReason = -1;

  Serial.println();
  Serial.println("ATTEMPT 2: direct connection to scanned 2.4 GHz AP");

  WiFi.begin(
    EMI_WIFI_SSID,
    EMI_WIFI_PASSWORD,
    channel,
    bssid,
    true
  );

  if (waitForConnection(20000)) {
    printSuccess();
    return;
  }

  Serial.println();
  Serial.println("ATTEMPT 2 FAILED.");

  Serial.print("WiFi.status()=");
  Serial.println((int)WiFi.status());

  Serial.print("Last disconnect reason=");
  Serial.print(lastDisconnectReason);
  Serial.print(" (");
  Serial.print(reasonName(lastDisconnectReason));
  Serial.println(")");

  Serial.println();
  Serial.println("========================================");
  Serial.println("CONNECTION TEST FINISHED WITHOUT WI-FI");
  Serial.println("========================================");
}

void loop() {
  delay(1000);
}
