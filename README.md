# ESP32 OpenWiFi Scanner

This sketch scans for nearby Wi-Fi networks with an ESP32 and displays the results on a 0.96" 128x64 SSD1306 screen using the Adafruit SSD1306 library.

## Usage

Upload `OpenWifiScanner.ino` to an ESP32 board. Connect momentary buttons to GPIOs 32, 33, 25 and 26 to select between all, open, web-accessible and vulnerable network lists. An LED on GPIO2 lights while scanning. The display shows the scan state, the first few networks found and a count of web-accessible, open, vulnerable and closed networks.

## Disclaimer

This tool is intended solely for educational and security purposes. It is designed to help users understand and assess the security of Wi-Fi networks. Do not use this tool to attempt unauthorized access to private networks. Unauthorized access to networks is illegal and unethical. Always obtain proper permission from the network owner before performing any scans or security assessments.
