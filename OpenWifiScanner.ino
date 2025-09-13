#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include <algorithm>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

struct WiFiNetwork {
  String ssid;
  wifi_auth_mode_t encryption;
  int32_t signalStrength;
  bool open = false;
  bool webAccess = false;
  bool vulnerable = false;
};

void displayInit();
void displayWelcome();
void displayLoading();
void displayTopBar(bool state);
void displayList(const std::vector<WiFiNetwork>& list);
void displayWifiCount(const std::vector<WiFiNetwork>& list);

void wifiInit();
std::vector<WiFiNetwork> getWifiNetworks();
void processWifiNetworks(std::vector<WiFiNetwork>& networks);
String encryptionTypeToString(wifi_auth_mode_t encryption);

//-------------------------------------------------------------
// Display helpers
//-------------------------------------------------------------

void displayInit() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;)
      ; // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void displayWelcome() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("OPEN WIFI");
  display.println("SCANNER");
  display.display();
}

void displayLoading() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Scanning...");
  display.display();
}

void displayTopBar(bool state) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("State: ");
  display.println(state ? "Scanning" : "Stopped");
}

void displayList(const std::vector<WiFiNetwork>& list) {
  display.setTextSize(1);
  uint8_t y = 10; // Start below top bar
  uint8_t maxItems = 4;
  for (size_t i = 0; i < list.size() && i < maxItems; ++i) {
    const WiFiNetwork& network = list[i];
    display.setCursor(0, y);
    char prefix = 'x';
    if (network.webAccess)
      prefix = 'v';
    else if (network.open)
      prefix = '-';
    else if (network.vulnerable)
      prefix = '!';
    display.print(prefix);
    display.print(' ');
    String name = network.ssid;
    if (name.length() > 14) {
      name = name.substring(0, 14);
    }
    display.print(name);
    display.setCursor(90, y);
    String enc = encryptionTypeToString(network.encryption);
    if (enc.length() > 4) enc = enc.substring(0, 4);
    display.print(enc);
    y += 12;
  }
  display.display();
}

void displayWifiCount(const std::vector<WiFiNetwork>& list) {
  uint8_t webAccessCount = 0;
  uint8_t openCount = 0;
  uint8_t vulnerableCount = 0;
  uint8_t closedCount = 0;

  for (const auto& n : list) {
    if (n.webAccess)
      webAccessCount++;
    else if (n.open)
      openCount++;
    else if (n.vulnerable)
      vulnerableCount++;
    else
      closedCount++;
  }

  display.setCursor(0, 58);
  display.setTextSize(1);
  display.print("W:");
  display.print(webAccessCount);
  display.print(" O:");
  display.print(openCount);
  display.print(" V:");
  display.print(vulnerableCount);
  display.print(" C:");
  display.print(closedCount);
  display.display();
}

//-------------------------------------------------------------
// WiFi helpers
//-------------------------------------------------------------

void wifiInit() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
}

std::vector<WiFiNetwork> getWifiNetworks() {
  std::vector<WiFiNetwork> networks;
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    WiFiNetwork network;
    network.ssid = WiFi.SSID(i);
    if (network.ssid.length() == 0) network.ssid = "Hidden";
    network.encryption = WiFi.encryptionType(i);
    network.signalStrength = WiFi.RSSI(i);
    networks.push_back(network);
  }
  return networks;
}

void processWifiNetworks(std::vector<WiFiNetwork>& networks) {
  for (auto& network : networks) {
    if (network.encryption == WIFI_AUTH_OPEN) {
      network.open = true;
    } else if (network.encryption == WIFI_AUTH_WEP) {
      network.vulnerable = true;
    }
  }
}


String encryptionTypeToString(wifi_auth_mode_t encryption) {
  switch (encryption) {
  case WIFI_AUTH_OPEN:
    return "OPEN";
  case WIFI_AUTH_WEP:
    return "WEP";
  case WIFI_AUTH_WPA_PSK:
    return "WPA";
  case WIFI_AUTH_WPA2_PSK:
    return "WPA2";
  case WIFI_AUTH_WPA_WPA2_PSK:
    return "WPA+";
  case WIFI_AUTH_WPA2_ENTERPRISE:
    return "WPA2";
  case WIFI_AUTH_WPA3_PSK:
    return "WPA3";
  case WIFI_AUTH_WPA2_WPA3_PSK:
    return "WPA*";
  case WIFI_AUTH_WAPI_PSK:
    return "WAPI";
  default:
    return "UNK";
  }
}

//-------------------------------------------------------------
// Main application
//-------------------------------------------------------------

std::vector<WiFiNetwork> networks;
int webTestIndex = 0;
unsigned long webTestStartTime = 0;
bool testingConnection = false;
const unsigned long connectionTimeout = 5000; // 5 seconds

bool running = true;
unsigned long lastScan = 0;
const unsigned long scanInterval = 10000; // 10 seconds

void setup() {
  Serial.begin(115200);
  displayInit();
  wifiInit();
  displayWelcome();
  delay(2000);
}

void loop() {
  if (!running) {
    delay(100);
    return;
  }

  // Scan for networks periodically
  if (millis() - lastScan > scanInterval) {
    lastScan = millis();
    displayLoading();

    if (testingConnection) {
      WiFi.disconnect();
      testingConnection = false;
    }

    networks = getWifiNetworks();
    processWifiNetworks(networks);
    webTestIndex = 0; // Reset test index for the new list
  }

  // Asynchronously test open networks for web access
  if (!testingConnection && webTestIndex < networks.size()) {
    // Find the next open network to test
    while (webTestIndex < networks.size() && !networks[webTestIndex].open) {
      webTestIndex++;
    }

    if (webTestIndex < networks.size()) {
      WiFi.begin(networks[webTestIndex].ssid.c_str());
      webTestStartTime = millis();
      testingConnection = true;
    }
  }

  if (testingConnection) {
    // If connection is successful, check for web access
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin("http://example.com");
      int httpCode = http.GET();
      http.end();
      networks[webTestIndex].webAccess = httpCode > 0;

      WiFi.disconnect();
      testingConnection = false;
      webTestIndex++;
    }
    // If connection timed out, mark as failed
    else if (millis() - webTestStartTime > connectionTimeout) {
      networks[webTestIndex].webAccess = false;

      WiFi.disconnect();
      testingConnection = false;
      webTestIndex++;
    }
  }

  // Sort networks for display based on category and signal strength
  std::sort(networks.begin(), networks.end(), [](const WiFiNetwork& a, const WiFiNetwork& b) {
    if (a.webAccess != b.webAccess) return a.webAccess;
    if (a.open != b.open) return a.open;
    if (a.vulnerable != b.vulnerable) return a.vulnerable;
    return a.signalStrength > b.signalStrength; // Stronger signal is a larger number
  });

  // Update display continuously
  displayTopBar(true);
  displayList(networks);
  displayWifiCount(networks);
  delay(10); // Small delay to prevent busy-looping and improve stability
}

