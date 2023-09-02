#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <LCD5110_Graph.h> // LCD5110 라이브러리를 설치하고 포함시켜야 함
#include <Adafruit_NeoPixel.h> // Neopixel 라이브러리를 설치하고 포함시켜야 함
#define NEOPIXEL_PIN 6
#define NEOPIXEL_NUM_LEDS 8
Adafruit_NeoPixel neopixels = Adafruit_NeoPixel(NEOPIXEL_NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
// WiFi 정보 설정 
#define rxPin 3 
#define txPin 2 
SoftwareSerial mySerial(txPin, rxPin);
const char ssid[] = "Wifi id";
const char pass[] = "password";
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
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
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
// 시간 간격 (1시간)
const unsigned long interval = 3600000; // milliseconds
unsigned long previousMillis = 0;
// 탁도센서 오류를 로깅하는 시간 간격 (5초)
const unsigned long turbidityLogInterval = 5000; // milliseconds
unsigned long turbidityLogPreviousMillis = 0;
// 온도 변화를 관찰하는 시간 간격 (1분)
const unsigned long temperatureChangeInterval = 60000; // milliseconds
unsigned long temperatureChangePreviousMillis = 0;
// 하루에 허용되는 최대 오류 로그 횟수
const int maxErrorLogsPerDay = 2;
// 하루에 허용되는 최대 온도 변화 로그 횟수
const int maxTemperatureChangeLogsPerDay = 2;

// LCD5110 라이브러리 설정
LCD5110 myLCD(8, 9, 10, 11, 12);

// 조도, 탁도, 온도 임계값 설정
const int thresholdLight = 200;   // 조도 임계값
const int thresholdTurbidity = 350; // 탁도 임계값
const float thresholdTemperature = 30.0; // 온도 임계값

// 표정 이미지 데이터
const byte sadFace[5] = {
    B00111100,
    B01000010,
    B10100101,
    B10000001,
    B10000001
};
const byte smileFace[5] = {
    B00111100,
    B01000010,
    B10100101,
    B10000001,
    B01111110
};

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
  // Neopixel 초기화
  neopixels.begin();
  neopixels.clear();
  neopixels.show();
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
   // 오류 로그를 찍을지 결정
  bool shouldLogTurbidityError = false;
  if (currentDay != lastLoggedDayTurb) {
    // 새로운 날이 시작될 때, 오류 로그 횟수와 마지막 로깅된 날짜 초기화
    turbidityErrorLogCount = 0;
    lastLoggedDayTurb = currentDay;
  }
  if (turbidityErrorLogCount < maxErrorLogsPerDay) {
    // 허용된 최대 횟수 내에서만 오류 로그를 찍음
    shouldLogTurbidityError = true;
  }
  // 5초마다 탁도 오류 로그를 전송
  if (currentMillis - turbidityLogPreviousMillis >= turbidityLogInterval) {
    turbidityLogPreviousMillis = currentMillis;
    if (shouldLogTurbidityError && (turbidityValue < 50 || turbidityValue > 900)) {
      logTurbidityError(turbidityValue);
      turbidityErrorLogCount++; // 오류 로그 횟수 증가
    }
  }
  // 온도 변화를 관찰하여 온도가 3도 이상 내려갈 때 로그를 보냄
  if (currentMillis - temperatureChangePreviousMillis >= temperatureChangeInterval) {
    temperatureChangePreviousMillis = currentMillis;
    sensors.requestTemperatures();
    float currentTemperature = sensors.getTempCByIndex(0);
    
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

    // 표정 선택
  bool isSad = (lightValue < thresholdLight) || (turbidityValue > thresholdTurbidity) || (temperature >= thresholdTemperature);

  // 화면 지우기
  myLCD.clear();

  // 표정 표시
  if (isSad) {
    displayImage(sadFace);
  } else {
    // 기본 표정 표시 (웃는 얼굴 등)
    displayImage(smileFace);
  }


  // 잠시 딜레이
  delay(1000);
}
// 표정 이미지 출력 함수
void displayImage(const byte* image) {
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 8; col++) {
      myLCD.drawBitmap(col * 8, row * 8, image, 8, 8);
    }
  }
}

void sendLog(const String& logType, const String& logValue = "") {
  // JSON 데이터 형식 구성
  String logData = "{\"marimoId\": 1, \"stat\": \"" + logType + "\"";
  if (!logValue.isEmpty()) {
    logData += ", \"value\": " + logValue;
  }
  logData += "}";
  // HTTP POST 요청 준비
  String url;
  if (logType == "neopixel_on" || logType == "neopixel_off") {
    url = "/log/1"; // Neopixel 로그
  } else if (logType == "turbidity_error") {
    if (turbidityErrorLogCount < maxErrorLogsPerDay) {
      url = "/log/2"; // Turbidity 오류 로그
      turbidityErrorLogCount++;
    } else {
      return; // maxErrorLogsPerDay를 초과하면 로그를 보내지 않음
    }
  } else if (logType == "temperature_change") {
    if (temperatureChangeLogCount < maxTemperatureChangeLogsPerDay) {
      url = "/log/3"; // 온도 변화 로그
      temperatureChangeLogCount++;
    } else {
      return; // maxTemperatureChangeLogsPerDay를 초과하면 로그를 보내지 않음
    }
  } else {
    // 알 수 없는 로그 유형인 경우 처리
    return;
  }
  // 로그 전송
  // 웹 서버로 HTTP POST 요청 전송
  if (client.connect(serverAddress, serverPort)) {
    client.print("POST " + url + " HTTP/1.1\r\n");
    client.print("Host: " + String(serverAddress) + ":" + String(serverPort) + "\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: ");
    client.print(logData.length());
    client.print("\r\n\r\n");
    client.print(logData);
    Serial.println(logType + " Log Sent to Server");
  } else {
    Serial.println("Connection to server failed");
  }
}