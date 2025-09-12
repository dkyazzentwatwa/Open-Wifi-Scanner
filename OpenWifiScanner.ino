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
std::vector<WiFiNetwork> getOpenWifiNetworks(std::vector<WiFiNetwork>& networks);
std::vector<WiFiNetwork> getClosedWifiNetworks(const std::vector<WiFiNetwork>& networks);
std::vector<WiFiNetwork> getVulnerableWifiNetworks(std::vector<WiFiNetwork>& networks);
std::vector<WiFiNetwork> getValidatedOpenWifiNetworks(std::vector<WiFiNetwork>& networks);
bool testOpenWifiConnection(const WiFiNetwork& network);
String encryptionTypeToString(wifi_auth_mode_t encryption);
std::vector<WiFiNetwork> mergeAndOrderWifiNetworks(
    std::vector<WiFiNetwork>& closedNetworks,
    std::vector<WiFiNetwork>& openNetworks,
    std::vector<WiFiNetwork>& webAccessNetworks,
    std::vector<WiFiNetwork>& vulnerableNetworks);

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

std::vector<WiFiNetwork> getOpenWifiNetworks(std::vector<WiFiNetwork>& networks) {
  std::vector<WiFiNetwork> openNetworks;
  for (auto& network : networks) {
    if (network.encryption == WIFI_AUTH_OPEN) {
      network.open = true;
      openNetworks.push_back(network);
    }
  }
  return openNetworks;
}

std::vector<WiFiNetwork> getClosedWifiNetworks(const std::vector<WiFiNetwork>& networks) {
  std::vector<WiFiNetwork> closedNetworks;
  for (const auto& network : networks) {
    if (network.encryption != WIFI_AUTH_OPEN) {
      closedNetworks.push_back(network);
    }
  }
  return closedNetworks;
}

std::vector<WiFiNetwork> getVulnerableWifiNetworks(std::vector<WiFiNetwork>& networks) {
  std::vector<WiFiNetwork> vulnerableNetworks;
  for (auto& network : networks) {
    if (network.encryption == WIFI_AUTH_WEP ||
        network.encryption == WIFI_AUTH_WPA_PSK) {
      network.vulnerable = true;
      vulnerableNetworks.push_back(network);
    }
  }
  return vulnerableNetworks;
}

std::vector<WiFiNetwork> getValidatedOpenWifiNetworks(std::vector<WiFiNetwork>& networks) {
  std::vector<WiFiNetwork> validatedNetworks;
  for (auto& network : networks) {
    if (network.encryption == WIFI_AUTH_OPEN) {
      if (testOpenWifiConnection(network)) {
        network.webAccess = true;
        validatedNetworks.push_back(network);
      }
    }
  }
  return validatedNetworks;
}

bool testOpenWifiConnection(const WiFiNetwork& network) {
  WiFi.begin(network.ssid.c_str());
  int maxRetries = 3;
  while (WiFi.status() != WL_CONNECTED && maxRetries > 0) {
    delay(300);
    maxRetries--;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  HTTPClient http;
  http.begin("http://example.com");
  int httpCode = http.GET();
  http.end();
  WiFi.disconnect();
  return httpCode > 0;
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

std::vector<WiFiNetwork> mergeAndOrderWifiNetworks(
    std::vector<WiFiNetwork>& closedNetworks,
    std::vector<WiFiNetwork>& openNetworks,
    std::vector<WiFiNetwork>& webAccessNetworks,
    std::vector<WiFiNetwork>& vulnerableNetworks) {
  std::vector<WiFiNetwork> mergedNetworks;
  mergedNetworks.insert(mergedNetworks.end(), webAccessNetworks.begin(), webAccessNetworks.end());

  openNetworks.erase(std::remove_if(openNetworks.begin(), openNetworks.end(),
                                    [&webAccessNetworks](const WiFiNetwork& network) {
                                      return std::any_of(webAccessNetworks.begin(), webAccessNetworks.end(),
                                                         [&network](const WiFiNetwork& webNetwork) {
                                                           return webNetwork.ssid == network.ssid;
                                                         });
                                    }),
                     openNetworks.end());
  mergedNetworks.insert(mergedNetworks.end(), openNetworks.begin(), openNetworks.end());

  mergedNetworks.insert(mergedNetworks.end(), vulnerableNetworks.begin(), vulnerableNetworks.end());

  closedNetworks.erase(std::remove_if(closedNetworks.begin(), closedNetworks.end(),
                                      [&vulnerableNetworks](const WiFiNetwork& network) {
                                        return std::any_of(vulnerableNetworks.begin(), vulnerableNetworks.end(),
                                                           [&network](const WiFiNetwork& vulnNetwork) {
                                                             return vulnNetwork.ssid == network.ssid;
                                                           });
                                      }),
                        closedNetworks.end());
  mergedNetworks.insert(mergedNetworks.end(), closedNetworks.begin(), closedNetworks.end());
  return mergedNetworks;
}

//-------------------------------------------------------------
// Main application
//-------------------------------------------------------------

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

  if (millis() - lastScan > scanInterval) {
    lastScan = millis();
    displayLoading();
    auto allNetworks = getWifiNetworks();
    auto closed = getClosedWifiNetworks(allNetworks);
    auto open = getOpenWifiNetworks(allNetworks);
    auto vulnerable = getVulnerableWifiNetworks(closed);
    auto web = getValidatedOpenWifiNetworks(open);
    auto finalNetworks = mergeAndOrderWifiNetworks(closed, open, web, vulnerable);

    displayTopBar(true);
    displayList(finalNetworks);
    displayWifiCount(finalNetworks);
  }
}

