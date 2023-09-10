#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

// WiFi 정보 설정
const char ssid[] = "LGWiFi_DD04";
const char pass[] = "-------";
int status = WL_IDLE_STATUS;

// 웹 서버 주소 및 포트 번호 설정
const char* serverAddress = "3.39.175.221";
const int serverPort = 8080;
WiFiEspClient client;

// 핀번호 설정
#define rxPin 3
#define txPin 2
#define ONE_WIRE_BUS 4
#define NEOPIXEL_PIN 6
#define BUTTON_pin 7 // 버튼을 7번 핀에 연결
#define NEOPIXEL_NUM_LEDS 300
const int CDS_PIN = A0;
const int TURBIDITY_PIN = A1;

// 여러가지 관련 설정
SoftwareSerial mySerial(txPin,rxPin); // 통신 관련 설정
Adafruit_NeoPixel neopixels = Adafruit_NeoPixel(NEOPIXEL_NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800); // 네오픽셀 관련 설정
OneWire oneWire(ONE_WIRE_BUS); // DS18B20 온도센서 관련 설정
DallasTemperature sensors(&oneWire);

// 이전 온도 값을 저장할 변수
float prevTempC = 0;
float prevTemperature = 0;
unsigned long lastLoggedDayTurb = 0;
unsigned long lastLoggedDayTemp = 0;
int turbidityErrorLogCount = 0;
int temperatureChangeLogCount = 0;
bool neopixelState = false; // Neopixel 상태 저장 변수
bool lastNeopixelState = false; // 이전 Neopixel 상태 저장 변수
bool LED_state = true; // LED 상태 저장 변수 (초기에 켜진 상태)

// 시간 간격 (1시간)
const unsigned long interval = 3600000; // milliseconds
unsigned long previousMillis = 0;

// 탁도센서 오류를 로깅하는 시간 간격 (5초)
const unsigned long turbidityLogInterval = 5000; // milliseconds
unsigned long turbidityLogPreviousMillis = 0;

// 온도 변화를 관찰하는 시간 간격 (1분)
const unsigned long temperatureChangeInterval = 60000; // milliseconds
unsigned long temperatureChangePreviousMillis = 0;

// 하루에 허용되는 탁도 최대 오류 로그 횟수
const int maxErrorLogsPerDay = 2;

// 하루에 허용되는 최대 온도 변화 로그 횟수
const int maxTemperatureChangeLogsPerDay = 2;


// 조도, 탁도, 온도 임계값 설정
const int thresholdLight = 200; // 조도 임계값
const int thresholdTurbidity = 350; // 탁도 임계값
const float thresholdTemperature = 30.0; // 온도 임계값


// 네오픽셀 LED 색상 설정
uint32_t sadColor = neopixels.Color(255, 0, 0); // 빨간색 (RGB)
uint32_t happyColor = neopixels.Color(0, 255, 0); // 녹색 (RGB)

void setup() {
  // 시리얼 통신 초기화
  Serial.begin(9600);
  mySerial.begin(9600);
  // WiFi 모듈 초기화
  WiFi.init(&mySerial);
  // WiFi에 연결 시도
  while (status != WL_CONNECTED) {
    Serial.println("Attempting to connect to WiFi network...");
    status = WiFi.begin(ssid, pass);
    delay(5000); // 5초마다 연결 시도
  }
  // WiFi 연결 성공 시 정보 출력
  Serial.println("Connected to WiFi");

  // DS18B20 온도센서 초기화
  sensors.begin();

  // Neopixel 관련 코드
  pinMode(BUTTON_pin, INPUT); // 입력으로 설정
  neopixels.begin();
  if (LED_state) {
    // 초록색으로 LED 초기화
    for (int i = 0; i < NEOPIXEL_NUM_LEDS; i++) {
      neopixels.setPixelColor(i, 0, 255, 0);
    }
    neopixels.show(); // 초기에 NeoPixel 표시 활성화
  }
}

void loop() {
  // 현재 시간 계산
  unsigned long currentMillis = millis();
  // 탁도 값 읽기
  int turbidityValue = analogRead(TURBIDITY_PIN);
  Serial.print("탁도 값: ");
  Serial.println(turbidityValue);
  // 현재 날짜 구하기
  unsigned long currentDay = currentMillis / 86400000; // 1일은 86,400,000 밀리초
  // 온도 값
  float currentTemperature = sensors.getTempCByIndex(0);
  // 표정 선택
  bool isSad = (analogRead(CDS_PIN) < thresholdLight) || (turbidityValue > thresholdTurbidity) || (currentTemperature >= thresholdTemperature);

  // 표정 표시
  if (isSad) {

    // 네오픽셀 LED를 빨간색으로 설정
    for (int i = 0; i < NEOPIXEL_NUM_LEDS; i++) {
      neopixels.setPixelColor(i, sadColor);
    }
    neopixels.show();
  } else {
    // 네오픽셀 LED를 기본 색상으로 설정 (예: 녹색)
    for (int i = 0; i < NEOPIXEL_NUM_LEDS; i++) {
      neopixels.setPixelColor(i, happyColor);
    }
    neopixels.show();
  }
  delay(1000);

  // 온도 변화를 관찰하여 온도가 3도 이상 내려갈 때 로그를 보냄
  if (currentMillis - temperatureChangePreviousMillis >= temperatureChangeInterval) {
    temperatureChangePreviousMillis = currentMillis;
    sensors.requestTemperatures();
    

    if (currentTemperature - prevTemperature >= 3) {
      if (temperatureChangeLogCount < maxTemperatureChangeLogsPerDay) {
        logTemperatureChange(prevTemperature, currentTemperature);
        temperatureChangeLogCount++;
      }
      prevTemperature = currentTemperature;
    }
  }

  // 스위치를 이용하여 Neopixel 조명 상태 확인
  neopixelState = digitalRead(NEOPIXEL_PIN) == HIGH;

  // Neopixel 상태가 변경되었을 때 로그 전송
  if (neopixelState != lastNeopixelState) {
    if (neopixelState) {
      // 조명이 켜진 경우 로그 전송
      logNeopixelOn();
    } else {
      // 조명이 꺼진 경우 로그 전송
      logNeopixelOff();
    }
    lastNeopixelState = neopixelState; // 상태 업데이트
  }

  // 1시간마다 전체 마리모 데이터를 전송
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    // DS18B20 온도 값을 읽어옴
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    Serial.print("온도 값(C): ");
    Serial.println(tempC);
    Serial.print("Temp: ");
    Serial.print(tempC);
    Serial.println("'C");
    // JSON 데이터 형식 구성
    String jsonData = "{\"marimoId\": 1, \"stat1\": " + String(analogRead(CDS_PIN)) + ", \"stat2\": " + String(turbidityValue) + ", \"stat3\": " + String(tempC) + "}";
    // HTTP POST 요청 준비
    String url = "/marimoData/sensor"; // Use the appropriate endpoint for marimo data
    String payload = jsonData;
    // 웹 서버로 HTTP POST 요청 전송
    if (client.connect(serverAddress, serverPort)) {
      client.print("POST " + url + " HTTP/1.1\r\n");
      client.print("Host: " + String(serverAddress) + ":" + String(serverPort) + "\r\n");
      client.print("Content-Type: application/json\r\n");
      client.print("Content-Length: ");
      client.print(payload.length());
      client.print("\r\n\r\n");
      client.print(payload);
      Serial.println("Data Sent to Server");
    } else {
      Serial.println("Connection to server failed");
    }
  }
}



void logTemperatureChange(float prevTemp, float currentTemp) {
  // 온도 변화 로그를 서버로 전송하는 코드를 작성
  // ...
}

void logNeopixelOn() {
  // Neopixel 켜짐 로그를 서버로 전송하는 코드를 작성
  // ...
}

void logNeopixelOff() {
  // Neopixel 꺼짐 로그를 서버로 전송하는 코드를 작성
  // ...
}
