// 스마트폰으로 esp01이랑 연결 후 station연결로 아두이노랑 eso01이랑 통신하는 툴
// 원격 제어할 때 쓰이는 코드

#include "WiFiEsp.h"

#include <SoftwareSerial.h> 
#define rxPin 3 
#define txPin 2 
SoftwareSerial esp01(txPin, rxPin); // SoftwareSerial NAME(TX, RX);

const char ssid[] = "LGWiFi_DD04";    // your network SSID (name)
const char pass[] = "8001002441";         // your network password
int status = WL_IDLE_STATUS;        // the Wifi radio's status

const char ssid_AP[] = "esp01_AP";    // your network SSID (name)
const char pass_AP[] = "1234test";         // your network password

#define ledPin 13
#define digiPin 7

WiFiEspServer server(80);

String stIp = "000.000.000.000";

String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(digiPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(digiPin, LOW);
  Serial.begin(9600);   // initialize serial for debugging
  esp01.begin(9600);   //와이파이 시리얼
  WiFi.init(&esp01);   // initialize ESP module
  while ( status != WL_CONNECTED) {   // 약 10초동안 wifi 연결 시도
    Serial.print(F("Attempting to connect to WPA SSID: "));
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);    // Connect to WPA/WPA2 network
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    stIp = toStringIp(ip);
    Serial.println(stIp);
  }
//  IPAddress localIp(192, 168, 111, 111); // softAP 접속 IP 설정
//  WiFi.configAP(localIp);
  WiFi.beginAP(ssid_AP, 10, pass_AP, ENC_TYPE_WPA2_PSK, false); // 10:채널, false: softAP와 station 동시 사용모드
  IPAddress ap = WiFi.localIP();
  Serial.print("AP Address: ");
  Serial.println(ap);
  server.begin();
}

const char HTTP_HEAD[] PROGMEM     = "<!DOCTYPE html><html lang=\"en\"><head>"
                                     "<meta name=\"viewport\"content=\"width=device-width,initial-scale=1,user-scalable=no\"/>"
                                     "<link rel=\"icon\" href=\"data:,\">";
const char HTTP_STYLE[] PROGMEM    = "<style>"
                                     "body{text-align:center;font-family:verdana;}"  
                                     "button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%} "
                                     "</style>";
const char HTTP_HEAD_END[] PROGMEM = "</head><body><div style=\"text-align:center;display:inline-block;min-width:260px;\">"
                                     "<h3>ESP01 Digital Pin Control</h3>";
const char BUTTON_TYPE[] PROGMEM   = "<p><button style=\"width:40%;background-color:#12cb3d;\">ON</button>"
                                     "<button style=\"margin-left:10%;width:40%;background-color:#1fa3ec;\">OFF</button></a></p>";
const char BUTTON_A_ON[] PROGMEM   = "<p>Button A</p><a href=\"/A/0\"><button style=\"background-color:#12cb3d;\">ON</button></a></p>";
const char BUTTON_A_OFF[] PROGMEM  = "<p>Button A</p><a href=\"/A/1\"><button style=\"background-color:#1fa3ec;\">OFF</button></a></p>";
const char BUTTON_B_ON[] PROGMEM   = "<p>Button B</p><a href=\"/B/0\"><button style=\"background-color:#12cb3d;\">ON</button></a></p>";
const char BUTTON_B_OFF[] PROGMEM  = "<p>Button B</p><a href=\"/B/1\"><button style=\"background-color:#1fa3ec;\">OFF</button></a></p>";
const char HTTP_END[] PROGMEM      = "</div></body></html>";

bool button_a = LOW; // off
bool button_b = LOW; // off

void loop() {
  WiFiEspClient client = server.available();  // listen for incoming clients
  if (client) {                               // if you get a client,
    while (client.connected()) {              // loop while the client's connected
      if (client.available()) {               // if there's bytes to read from the client,
        String income_AP = client.readStringUntil('\n');
        if (income_AP.indexOf(F("A/1")) != -1) {
          Serial.println(F("button_A on"));
          button_a = HIGH;
          digitalWrite(ledPin, button_a);
        } else if (income_AP.indexOf(F("A/0")) != -1) {
          Serial.println(F("button_A off"));
          button_a = LOW;
          digitalWrite(ledPin, button_a);
        } else if (income_AP.indexOf(F("B/1")) != -1) {
          Serial.println(F("button_B on"));
          button_b = HIGH;
          digitalWrite(digiPin, button_b);
        } else if (income_AP.indexOf(F("B/0")) != -1) {
          Serial.println(F("button_B off"));
          button_b = LOW;
          digitalWrite(digiPin, button_b);
        }
        client.flush();
        client.println(F("HTTP/1.1 200 OK"));
        client.println(F("Content-type:text/html"));
        client.println(F("Connection: close"));
        client.println();
        String page;
        page  = (const __FlashStringHelper *)HTTP_HEAD;
        page += (const __FlashStringHelper *)HTTP_STYLE;
        page += (const __FlashStringHelper *)HTTP_HEAD_END;
        if (income_AP.indexOf(F("ip")) != -1) {
          if (status == WL_CONNECTED) {
            page += F("<p>IP Address: ");
            page += stIp;
            page += F("</p>");
          } else page += F("<p>Connection failed</p>");
        }else {
          page += (const __FlashStringHelper *)BUTTON_TYPE;
          if (button_a == HIGH) page += (const __FlashStringHelper *)BUTTON_A_ON;
          else page += (const __FlashStringHelper *)BUTTON_A_OFF;
          if (button_b == HIGH) page += (const __FlashStringHelper *)BUTTON_B_ON;
          else page += (const __FlashStringHelper *)BUTTON_B_OFF;
        }
        page += (const __FlashStringHelper *)HTTP_END;
        client.print(page);
        client.println();
        delay(1);
        break;
      }
    }
    client.stop();
    Serial.println(F("Client disconnected"));
  }
}