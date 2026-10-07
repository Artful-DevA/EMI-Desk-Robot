#include <Arduino.h>
#include <WiFi.h>

// ============================================================
// EMI - ESP32-C3 Wi-Fi scan diagnostic
// Board: ESP32C3 Dev Module
//
// This sketch does NOT need secrets.h and does NOT connect to Wi-Fi.
// It repeatedly scans for nearby 2.4 GHz networks so the output
// cannot be missed if Serial Monitor is opened after boot.
// ============================================================

const unsigned long RESCAN_INTERVAL_MS = 10000;

void runWifiScan() {
  Serial.println();
  Serial.println("Scanning for 2.4 GHz Wi-Fi networks...");
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(300);

  int count = WiFi.scanNetworks(false, true);

  if (count < 0) {
    Serial.print("Scan failed. code=");
    Serial.println(count);
    return;
  }

  if (count == 0) {
    Serial.println("No Wi-Fi networks found.");
  } else {
    Serial.print("Found ");
    Serial.print(count);
    Serial.println(" network(s):");
    Serial.println();

    for (int i = 0; i < count; i++) {
      Serial.print("[");
      Serial.print(i);
      Serial.print("] SSID=\"");
      Serial.print(WiFi.SSID(i));
      Serial.print("\"  RSSI=");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm  channel=");
      Serial.print(WiFi.channel(i));
      Serial.print("  encryption=");
      Serial.println((int)WiFi.encryptionType(i));
    }
  }

  WiFi.scanDelete();

  Serial.println();
  Serial.println("Scan complete.");
  Serial.println("Next scan in 10 seconds...");
}

void setup() {
  Serial.begin(115200);

  unsigned long waitStarted = millis();

  while (!Serial && millis() - waitStarted < 5000) {
    delay(10);
  }

  delay(250);

  Serial.println();
  Serial.println("EMI C3 Wi-Fi diagnostic ready.");

  runWifiScan();
}

void loop() {
  delay(RESCAN_INTERVAL_MS);
  runWifiScan();
}
