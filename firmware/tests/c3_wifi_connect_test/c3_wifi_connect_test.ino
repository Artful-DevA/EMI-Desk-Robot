#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

// ============================================================
// EMI - ESP32-C3 Super Mini Wi-Fi TX-power diagnostic
// Board: ESP32C3 Dev Module
//
// Uses the existing private secrets.h.
// Nothing here hardcodes the user's SSID or password.
//
// Why this test exists:
// Some ESP32-C3 Super Mini boards can scan Wi-Fi normally but fail
// authentication with AUTH_EXPIRE at the default TX power.
// This sketch finds the exact 2.4 GHz AP and automatically tries
// several lower transmit-power settings.
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
  const unsigned long started = millis();

  while (millis() - started < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }

    delay(100);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool findTarget(
  int32_t &channel,
  uint8_t bssid[6],
  int32_t &rssi
) {
  Serial.println();
  Serial.println("Scanning for configured SSID...");

  int count = WiFi.scanNetworks(false, true);

  if (count <= 0) {
    Serial.print("Scan failed or found nothing. result=");
    Serial.println(count);
    return false;
  }

  int bestIndex = -1;
  int32_t bestRssi = -1000;

  for (int i = 0; i < count; i++) {
    if (
      WiFi.SSID(i) == EMI_WIFI_SSID &&
      WiFi.RSSI(i) > bestRssi
    ) {
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

void printSuccess(float requestedDbm) {
  Serial.println();
  Serial.println("========================================");
  Serial.println("SUCCESS: ESP32-C3 IS CONNECTED TO WI-FI");
  Serial.println("========================================");

  Serial.print("Working TX power: ");
  Serial.print(requestedDbm, 1);
  Serial.println(" dBm");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  Serial.print("Channel: ");
  Serial.println(WiFi.channel());

  Serial.println();
  Serial.println("LEAVE THE BOARD RUNNING.");
}

bool tryPower(
  wifi_power_t power,
  float requestedDbm,
  int32_t channel,
  uint8_t bssid[6]
) {
  WiFi.disconnect(false, false);
  delay(700);

  lastDisconnectReason = -1;

  bool powerSet = WiFi.setTxPower(power);

  Serial.println();
  Serial.println("----------------------------------------");
  Serial.print("Trying TX power: ");
  Serial.print(requestedDbm, 1);
  Serial.println(" dBm");

  Serial.print("WiFi.setTxPower() result: ");
  Serial.println(powerSet ? "OK" : "FAILED");

  Serial.println("Connecting to the exact scanned 2.4 GHz AP...");

  WiFi.begin(
    EMI_WIFI_SSID,
    EMI_WIFI_PASSWORD,
    channel,
    bssid,
    true
  );

  if (waitForConnection(12000)) {
    printSuccess(requestedDbm);
    return true;
  }

  Serial.print("FAILED at ");
  Serial.print(requestedDbm, 1);
  Serial.println(" dBm");

  Serial.print("WiFi.status()=");
  Serial.println((int)WiFi.status());

  Serial.print("Last disconnect reason=");
  Serial.print(lastDisconnectReason);
  Serial.print(" (");
  Serial.print(reasonName(lastDisconnectReason));
  Serial.println(")");

  return false;
}

void setup() {
  Serial.begin(115200);

  unsigned long serialWaitStarted = millis();

  while (!Serial && millis() - serialWaitStarted < 5000) {
    delay(10);
  }

  delay(300);

  Serial.println();
  Serial.println("EMI C3 SUPER MINI WI-FI TX-POWER TEST");
  Serial.println("====================================");

  WiFi.onEvent(onWiFiEvent);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  WiFi.disconnect(false, true);
  delay(500);

  int32_t channel = 0;
  int32_t rssi = -1000;
  uint8_t bssid[6] = {0, 0, 0, 0, 0, 0};

  if (!findTarget(channel, bssid, rssi)) {
    Serial.println();
    Serial.println("STOPPED: configured SSID was not found.");
    return;
  }

  // 8.5 dBm is first because this value is known to help some
  // problematic ESP32-C3 Super Mini boards.
  if (tryPower(WIFI_POWER_8_5dBm, 8.5f, channel, bssid)) {
    return;
  }

  if (tryPower(WIFI_POWER_11dBm, 11.0f, channel, bssid)) {
    return;
  }

  if (tryPower(WIFI_POWER_5dBm, 5.0f, channel, bssid)) {
    return;
  }

  if (tryPower(WIFI_POWER_13dBm, 13.0f, channel, bssid)) {
    return;
  }

  Serial.println();
  Serial.println("========================================");
  Serial.println("ALL TX-POWER ATTEMPTS FAILED");
  Serial.println("========================================");
  Serial.println();
  Serial.println("The next step is hardware/antenna diagnosis, not another password test.");
}

void loop() {
  delay(1000);
}
