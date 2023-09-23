#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

// WiFi 정보 설정
const char ssid[] = "Galaxy zflip5 young";
const char pass[] = "dodrmfl02";
int status = WL_IDLE_STATUS;

// 웹 서버 주소 및 포트 번호 설정
const char* serverAddress = "3.39.175.221";
const int serverPort = 8080;
WiFiEspClient client;

// 핀번호 설정
#define rxPin 3
#define txPin 2
#define ONE_WIRE_BUS 4
#define NEOPIXEL_PIN1 6 // 상태 알려주는 네오픽셀
#define NEOPIXEL_PIN2 5 // 어항 밑 네오픽셀
#define BUTTON_pin 8 // 버튼을 8번 핀에 연결
#define NEOPIXEL_NUM_LEDS1 16
#define NEOPIXEL_NUM_LEDS2 16
#define NEOPIXEL_TEMPERATURE_LED 4
#define NEOPIXEL_LIGHT_LED 8
#define NEOPIXEL_TURBIDITY_LED 12
const int CDS_PIN = A0;
const int TURBIDITY_PIN = A1;

// Define NeoPixel colors
#define RED_COLOR 0xFF0000  // Red color
#define YELLOW_COLOR 0xFFFF00  // Yellow color
#define BLUE_COLOR 0x0000FF  // Blue color
#define GREEN_COLOR 0x00FF00  // Green color

// 여러가지 관련 설정
SoftwareSerial mySerial(txPin, rxPin); // 통신 관련 설정
Adafruit_NeoPixel neopixels1 = Adafruit_NeoPixel(NEOPIXEL_NUM_LEDS1, NEOPIXEL_PIN1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel neopixels2 = Adafruit_NeoPixel(NEOPIXEL_NUM_LEDS2, NEOPIXEL_PIN2, NEO_GRB + NEO_KHZ800);
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
bool isNeopixelOn = true; // 조명 상태를 나타내는 변수
int lastButtonState = LOW;
int buttonState = LOW;

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
const float thresholdTemperature = 26.0; // 온도 임계값

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
  pinMode(BUTTON_pin, INPUT_PULLUP); // 입력으로 설정
  // Neopixel 초기화
  neopixels1.begin();
  neopixels2.begin();
  if (LED_state) {
    // 각각의 네오픽셀을 초기화하고 무색으로 설정
    for (int i = 0; i < NEOPIXEL_NUM_LEDS1; i++) {
      neopixels1.setPixelColor(i, 0, 0, 0);
    }
    for (int i = 0; i < NEOPIXEL_NUM_LEDS2; i++) {
      neopixels2.setPixelColor(i, 0, 0, 0);
    }

    neopixels1.show();
    neopixels2.show();
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

  bool isTemperatureSad = (currentTemperature >= thresholdTemperature);
  bool isLightSad = (analogRead(CDS_PIN) < thresholdLight);
  bool isTurbiditySad = (turbidityValue > thresholdTurbidity);

  // 상태 나타내는 네오픽셀 설정
  // 온도 (빨강), 조도 (노랑), 탁도 (파랑)
  setColor(neopixels1, NEOPIXEL_TEMPERATURE_LED, isTemperatureSad ? RED_COLOR : 0);
  setColor(neopixels1, NEOPIXEL_LIGHT_LED, isLightSad ? YELLOW_COLOR : 0);
  setColor(neopixels1, NEOPIXEL_TURBIDITY_LED, isTurbiditySad ? BLUE_COLOR : 0);
  // 어항네오픽셀 (빨강,초록)

  // 네오픽셀 표시 업데이트
  neopixels1.show();
  neopixels2.show();

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


  int newButtonState = digitalRead(BUTTON_pin);

   // 버튼 상태가 바뀌면 동작 수행
  if (newButtonState != lastButtonState) {
    // 버튼이 눌린 경우 (HIGH)
    if (newButtonState == HIGH) {
      // LED 상태를 토글 (반전)합니다.
      isNeopixelOn = !isNeopixelOn;

      // LED 상태에 따라 어항 밑 LED를 설정합니다.
      if (isNeopixelOn) {
        // LED 켜기 (빨간색)
  setColor(neopixels2, 1, isTemperatureSad || isLightSad || isTurbiditySad ? RED_COLOR : GREEN_COLOR);
  setColor(neopixels2, 2, isTemperatureSad || isLightSad || isTurbiditySad ? RED_COLOR : GREEN_COLOR);
  setColor(neopixels2, 3, isTemperatureSad || isLightSad || isTurbiditySad ? RED_COLOR : GREEN_COLOR);
        logNeopixelOn(); // 조명이 켜진 상태를 로그로 전송
      } else {
        // LED 끄기 (검정색)
        setColor(neopixels2, 1, 0);
        setColor(neopixels2, 2, 0);
        setColor(neopixels2, 3, 0);
        logNeopixelOff(); // 조명이 꺼진 상태를 로그로 전송
      }

      // 네오픽셀 표시 업데이트
      neopixels2.show();
    }
    // 버튼 상태를 저장
    lastButtonState = newButtonState;
  }

  delay(1000); // 이 부분은 중복 로그를 방지하기 위한 딜레이입니다.
}

void logTemperatureChange(float prevTemp, float currentTemp) {
  // 온도 변화 로그를 서버로 전송하는 코드를 작성
  // JSON 데이터 형식으로 로그 데이터 생성
  String jsonData = "{\"eventType\": \"temperatureChange\", \"prevTemp\": " + String(prevTemp) + ", \"currentTemp\": " + String(currentTemp) + "}";
  sendLogToServer(jsonData);
}

void logNeopixelOn() {
  // Neopixel 켜짐 로그를 서버로 전송하는 코드를 작성
  // JSON 데이터 형식으로 로그 데이터 생성
  String jsonData = "{\"eventType\": \"neopixelOn\"}";
  sendLogToServer(jsonData);
}

void logNeopixelOff() {
  // Neopixel 꺼짐 로그를 서버로 전송하는 코드를 작성
  // JSON 데이터 형식으로 로그 데이터 생성
  String jsonData = "{\"eventType\": \"neopixelOff\"}";
  sendLogToServer(jsonData);
}

// 로그 데이터를 서버로 전송하는 함수
void sendLogToServer(String logData) {
  if (client.connect(serverAddress, serverPort)) {
    // HTTP POST 요청 준비
    String url = "/log"; // 로그 엔드포인트 URL
    String payload = logData;

    client.print("POST " + url + " HTTP/1.1\r\n");
    client.print("Host: " + String(serverAddress) + ":" + String(serverPort) + "\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Content-Length: ");
    client.print(payload.length());
    client.print("\r\n\r\n");
    client.print(payload);

    Serial.println("Log Data Sent to Server");
  } else {
    Serial.println("Failed to connect to server");
  }
}

void setColor(Adafruit_NeoPixel& strip, int pixelIndex, uint32_t color) {
  if (pixelIndex >= 0 && pixelIndex < strip.numPixels()) {
    strip.setPixelColor(pixelIndex, color);
  }
}
