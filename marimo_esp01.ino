#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiEsp.h>
#include <SoftwareSerial.h>

// WiFi 정보 설정 
#define rxPin 3 
#define txPin 2 
SoftwareSerial mySerial(txPin, rxPin);

const char ssid[] = "나의 와이파이";
const char pass[] = "와이파이 비번";
int status = WL_IDLE_STATUS;

// 웹 서버 주소 및 포트 번호 설정
const char* serverAddress = "3.39.175.221";
const int serverPort = 8080;
WiFiEspClient client;

// 조도센서(CDS) 관련 설정
const int CDS_PIN = A0;

// 탁도센서(AZDM01) 관련 설정
const int TURBIDITY_PIN = A1;

// DS18B20 온도센서 핀 번호
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(9600);
  mySerial.begin(9600);

  // WiFi 모듈 초기화
  WiFi.init(&mySerial);

  // WiFi에 연결 시도
  while ( status != WL_CONNECTED) {
    Serial.println("Attempting to connect to WiFi network...");
    status = WiFi.begin(ssid, pass);
    delay(5000);  // 5초마다 연결 시도
  }

  // WiFi 연결 성공 시 정보 출력
  Serial.println("Connected to WiFi");

  // DS18B20 온도센서 초기화
  sensors.begin();
}

void loop() {
  // 조도 값을 읽어옴
  int cdsValue = analogRead(CDS_PIN);
  Serial.print("CDS 조도 값: ");
  Serial.println(cdsValue);

  // 탁도 값을 읽어옴
  int turbidityValue = analogRead(TURBIDITY_PIN);
  Serial.print("탁도 값: ");
  Serial.println(turbidityValue);

  // DS18B20 온도 값을 읽어옴
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  Serial.print("Temp: ");
  Serial.print(tempC);
  Serial.println("'C");

  // 웹 서버로 JSON 데이터 전송
  sendJSONDataToServer(cdsValue, turbidityValue, tempC);

  // 잠시 딜레이
  delay(5000);
}

void sendJSONDataToServer(int cdsValue, int turbidityValue, float tempC) {
  if (!client.connected()) {
    // 클라이언트가 연결되어 있지 않으면 연결 시도
    if (client.connect(serverAddress, serverPort)) {
      Serial.println("Connected to server");
    } else {
      Serial.println("Connection to server failed");
      return; // 연결 실패 시 함수 종료
    }
  }

  // JSON 데이터 형식 구성
  String jsonData = "{\"marimoId\": 0, \"stat1\": " + String(cdsValue) + ", \"stat2\": " + String(turbidityValue) + ", \"stat3\": " + String(tempC) + "}";

  // HTTP POST 요청 준비
  String url = "/marimoData/sensor";
  String payload = jsonData;

  // 웹 서버로 HTTP POST 요청 전송
  client.print("POST " + url + " HTTP/1.1\r\n");
  client.print("Host: " + String(serverAddress) + ":" + String(serverPort) + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: ");
  client.print(payload.length());
  client.print("\r\n\r\n");
  client.print(payload);

  Serial.println("Data Sent to Server");
}
