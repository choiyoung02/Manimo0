#include "WiFiEsp.h"

#include "SoftwareSerial.h"



#define ssid  "5G_LGWIFI_DD04"

#define pass  "8001002441"

#define RX    2

#define TX    3

#define LED   8



SoftwareSerial esp(RX, TX);

int status = WL_IDLE_STATUS;

WiFiEspServer server(80);



void setup() {

  pinMode(LED, OUTPUT);

  Serial.begin(9600);

  esp.begin(9600);

  WiFi.init(&esp);



  if (WiFi.status() == WL_NO_SHIELD) {

    Serial.println("WiFi shield not present");

    while (true);

  }



  while (status != WL_CONNECTED) {

    Serial.print("Attempting to connect to WPA SSID: ");

    Serial.println(ssid);

    status = WiFi.begin(ssid, pass);

  }



  Serial.println("You're connected to the network");

  printWifiStatus();



  server.begin();

}



void printWifiStatus() {

  Serial.print("SSID: ");

  Serial.println(WiFi.SSID());



  IPAddress ip = WiFi.localIP();

  Serial.print("IP Address: ");

  Serial.println(ip);



  Serial.println();

  Serial.print("To see this page in action, open a browser to http://");

  Serial.println(ip);

  Serial.println();

}



const char HTTP_HEAD[] PROGMEM = R"=====(

<!doctype html>

<html lang="ko">

  <head>

    <link rel="icon" href="data:,">

    <meta name="viewport"content="width=device-width,initial-scale=1,user-scalable=no"/>

    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">

    <title>WiFi 컨트롤</title>

    <script src="https://code.jquery.com/jquery-1.12.4.min.js"></script>

    <style>

      button {border: 0; border-radius: 0.3rem; background: #858585; color: #faffff; line-height: 2.4rem; font-size: 1.2rem; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; cursor: pointer;}

      button.off {background: #1fa3ec;}

      button.off:hover {background: #0e70a4;}

      button.on {background: gold;}

      button.on:hover {background:#ddba01;}

      CENTER > * {width: 250px;}

    </style>

  </head>

)=====";



const char HTTP_BODY[] PROGMEM = R"=====(

  <body>

    <CENTER>

      <h2>WiFi 컨트롤</h2>

      <p>스위치</p>

)=====";



const char SWITCH_ON[] PROGMEM = R"=====(     <button id = "switch" class="on" onClick="$.post(`/switch/${$(this).hasClass('off')?'on':'off'}`,(data)=>{data.status?$('#switch').addClass('on').removeClass('off').text('ON'):$('#switch').addClass('off').removeClass('on').text('OFF')})">ON</button>)=====";

const char SWITCH_OFF[] PROGMEM = R"=====(      <button id = "switch" class="off" onClick="$.post(`/switch/${$(this).hasClass('off')?'on':'off'}`,(data)=>{data.status?$('#switch').addClass('on').removeClass('off').text('ON'):$('#switch').addClass('off').removeClass('on').text('OFF')})">OFF</button>)=====";

const char HTTP_END[] PROGMEM = R"=====(

    </CENTER>

  </body>

</html>

)=====";



RingBuffer buf(16);



void loop() {

  WiFiEspClient client = server.available();

  if (client) {

    Serial.println("New client"); 

    buf.init();

    while (client.connected()) {

      if (client.available()) {

        char c = client.read();

        buf.push(c);



        if (buf.endsWith("GET /")) {

          client.flush();

          client.println(F("HTTP/1.1 200 OK"));

          client.println(F("Content-type:text/html; charset=utf-8"));

          client.println(F("Connection: close"));

          client.println();

        

          client.print((const __FlashStringHelper *)HTTP_HEAD);

          client.print((const __FlashStringHelper *)HTTP_BODY);

          if (digitalRead(LED) == HIGH) {

            client.print((const __FlashStringHelper *)SWITCH_ON);

          } else {

            client.print((const __FlashStringHelper *)SWITCH_OFF);

          }

          client.print((const __FlashStringHelper *)HTTP_END);

          client.println();

          delay(1);

          break;

        }

        

        else if (buf.endsWith("POST /switch/on") || buf.endsWith("POST /switch/off")) {

          client.flush();

          client.println(F("HTTP/1.1 200 OK"));

          client.println(F("Content-type:application/json; charset=utf-8"));

          client.println(F("Connection: close"));          

          client.println();

          if (buf.endsWith("POST /switch/on")) {

            Serial.println(F("Turn led ON"));

            digitalWrite(LED, HIGH);

            client.println(F("{\"status\":1}"));

          } else {

            Serial.println(F("Turn led OFF"));

            digitalWrite(LED, LOW);

            client.println("{\"status\":0}");           

          }

          delay(1);

          break;

        }

      }

    }

    

    client.stop();

    Serial.println("Client disconnected");

  }

}