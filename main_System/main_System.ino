//library
#include <esp_task_wdt.h>
#include <IOXhop_FirebaseESP32.h>
#include <TridentTD_LineNotify.h>
#include <BH1750FVI.h>
#include <PMS.h>
#include <Adafruit_Si7021.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <time.h>

#define swithRS 14
#define open__ 13
#define control_sensor 12
#define FAN 4
//Time
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  60*120
char ntp_server1[20] = "pool.ntp.org";
char ntp_server2[20] = "time.nist.gov";
char ntp_server3[20] = "time.uni.net.th";
int timezone = 7 * 3600;
int dst = 0;

//LED_M
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define DATA_PIN    25
#define CLK_PIN     27
#define CS_PIN      26
#define MAX_DEVICES 4
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

//connecting wifi, line, firebase
//#define FIREBASE_HOST "https://datatest-2a7ca-default-rtdb.asia-southeast1.firebasedatabase.app/"
//#define FIREBASE_AUTH "qCTycWImPn5CHQJXwwcwnJZP3lNiL1p59Hzt7IzP"
#define FIREBASE_HOST "https://weathermonitoring-c0205-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "RpTTnS8WRVxaWVYHw7dwFvHrkV0MwPgLdQTEmm6t"
//#define WIFI_SSID "@RMUTI-One-IoT"
//#define WIFI_PASSWORD "IoT@RMUTI"
#define WIFI_SSID "atsada"
#define WIFI_PASSWORD "0840377915"
//#define LINE_TOKEN  "57Wy4feqIdWEtDhvNSHF1CIMA2XQ2Ef5XSqGvJzGlbZ"
#define LINE_TOKEN "ur7NPHnywUg9LmEWmUnGRHYIOykWgar19s9pX1qRl7h"
//Si7001>>tem, hum
#define SDA_2 33
#define SCL_2 32
TwoWire I2Ctwo = TwoWire(1);
Adafruit_Si7021 sensor = Adafruit_Si7021();
Adafruit_Si7021 sensor2 = Adafruit_Si7021(&I2Ctwo);

//BH1750FVI>>Lux
BH1750FVI LightSensor(BH1750FVI::k_DevModeContLowRes);

//MQ-7
float Co_calibration_one = 10, Co_calibration_two = 10;

//PMS7003>>PM2.5
#define RXD2 16
#define TXD2 17
#define RXD0 18
#define TXD0 5
int pm1 = 0, pm2 = 0;
HardwareSerial SerialPMS_1(1);
HardwareSerial SerialPMS_2(2);
PMS pms(SerialPMS_1);
PMS pms2(SerialPMS_2);
PMS::DATA data;

//multitask
TaskHandle_t Task1;
TaskHandle_t Task2;

//data
String time_da[48], Day_da[48], sLEDM_1, sLEDM_2, sLEDM_3, time__d, sLine_m;
float tem_s, hum_s, dataTem[48], dataHum[48];
int nSum = 0, dCO = 0, dPM25 = 0, dLux = 0, dataPM[48], dataCO[48], dataLux[48];
uint8_t sTime_m;
float tem_, hum_;
int co_, lux_;

void setup() {
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.begin(115200);

  //Control
  pinMode(FAN, OUTPUT);
  pinMode(open__, OUTPUT);
  pinMode(control_sensor, OUTPUT);
  pinMode(swithRS, OUTPUT);
  digitalWrite(open__, 1);
  digitalWrite(control_sensor, LOW);

  // LEDM Setup
  P.begin();
  P.setIntensity(0);

  //WiFi setup
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  //Time setup
  configTime(timezone, dst, ntp_server1, ntp_server2, ntp_server3);
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }

  //Line&Firebast setup
  LINE.setToken(LINE_TOKEN);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  //Sensor setup
  LightSensor.begin();
  SerialPMS_2.begin(9600, SERIAL_8N1, RXD0, TXD0);
  SerialPMS_1.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pms.passiveMode();
  pms2.passiveMode();
  pms2.sleep();
  Co_calibration_one = MQCalibration(A7);

  for (int i = 0; i < 3; i++) {
    Firebase.setBool("getData/request", true);
    delay(500);
  }

  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);

  // multitask
  xTaskCreatePinnedToCore(LED_M, "Task1", 10240, NULL, 1, &Task1, 1);
  xTaskCreatePinnedToCore(Firebase_data, "Task2", 10240 , NULL, 1, &Task2, 0);

}
// 00:00:00
String ti(int i) {
  String txt = "0";
  if (i <= 9) {
    txt += String(i) ;
  } else {
    txt = String(i) ;
  } return (txt);
}
//Time
String time_n(String txt) {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  String timeNow = "";
  if (txt == "h") {
    timeNow += ti(p_tm->tm_hour);
  } else if (txt == "m") {
    timeNow += ti(p_tm->tm_min);
  } else if (txt == "s") {
    timeNow += ti(p_tm->tm_sec);
  } else if (txt == "d") {
    timeNow += ti(p_tm->tm_mday);
  } else if (txt == "M") {
    timeNow += ti(p_tm->tm_mon + 1);
  } else if (txt == "Y") {
    timeNow += String(p_tm->tm_year + 1900);
  } return timeNow;
}
//MQ7
float MQResistanceCalculation(int raw_adc) {
  return (((float)5.0 * (4095 - raw_adc) / raw_adc));
}
float MQCalibration(int mq_pin) {
  int i;
  float val = 0;
  for (i = 0; i < 10; i++) {
    val += MQResistanceCalculation(analogRead(mq_pin));
    delay(500);
  }
  val = (val / 10) / 9.83;
  return val;
}
int MQRead(int mq_pin, float kohm) {
  int i;
  float rs = 0;
  for (i = 0; i < 5; i++) {
    rs += MQResistanceCalculation(analogRead(mq_pin));
    delay(50);
  }
  rs = (rs / 5) / kohm;
  return (pow(10, (((log(rs) - 0.72) / (-0.34)) + 2.3)));
}
// เช็คการทำงานของเซ็นเซอร์ อุณหภูมิ ความชื้น pm2.5 และคาบอร์นมอน
void check_s(String txt, int i1, int i2, uint8_t i3) {
  if ((i1 + i3) >= i2 && (i1 - i3) <= i2) {
    Firebase.setString(txt, "1");
  } else {
    Firebase.setString(txt, "0");
  } return;
}

// ส่งข้อมูลข้น Firebase แบบ push
void firebase_data(float tem_f, float hum_f, int PM_f, int Co_f, int lux_f, String time1, String day1) {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["tem"] = String(tem_f).toFloat();
  root["hum"] = String(hum_f).toFloat();
  root["PM25"] = PM_f;
  root["Co"] = Co_f;
  root["lux"] = lux_f;
  root["time_"] = time1;
  root["Day_"] = day1;
  Firebase.push("Data/", root);
}

// set time
String time_Data(int i, bool b) {
  String txt;
  static int time__ = 0, time__h = 0, time__m = 0;
  if (i != time__ || b == false) {
    time__ =  i;
    time__m = time_n("m").toInt() + time__;
    time__h = time_n("h").toInt();
    while (time__m > 59) {
      time__h = (time__h + 1) % 24;
      time__m = time__m - 60;
    }
  }
  txt = ti(time__h) + ":" + ti(time__m);
  return (txt);
}
void loop() {

  // Sleep
  if (time_n("h") == "03") {
    digitalWrite(open__, LOW); delay(100);
    esp_deep_sleep_start();
  }

  // Check sensor
  static bool cs__ = true;
  if (time_n("h") == "11" || time_n("h") == "23") {
    if (time_n("m") == "59" && cs__ == true) {
      digitalWrite(control_sensor, HIGH);
      Serial.println("Check_sensor_Star");
      Co_calibration_two = MQCalibration(A6);
      I2Ctwo.begin(SDA_2, SCL_2, 100000);
      pms2.wakeUp();
      cs__ = false;
    }
  } else if ((time_n("h") == "12" || time_n("h") == "00") && cs__ == false) {
    pms2.requestRead();
    if (pms2.readUntil(data)) {
      pm2 = data.PM_AE_UG_2_5;
    }
    static uint8_t cs_ = 0;
    static float t_2 = 0, h_2 = 0;
    t_2 = sensor2.readTemperature();
    h_2 = sensor2.readHumidity();
    if (!isnan(tem_) && !isnan(hum_) && !isnan(t_2) && !isnan(h_2)) {
      check_s("Check/_tem", tem_, t_2, 5);
      check_s("Check/_hum", hum_, h_2, 5);
    }
    check_s("Check/_PM", pm1, pm2, 15);
    check_s("Check/_CO", co_, MQRead(A6, Co_calibration_two), 15);
    if (time_n("m") == "03") {
      digitalWrite(control_sensor, LOW);
      pms2.sleep();
      cs__ = true;
    }
  }

  //Line Notify
  if (!isnan(tem_) && !isnan(hum_)) {
    static bool coo = true;
    if (time_n("h") + ":" + time_n("m") == sLine_m && coo == true) {
      coo = false;
      LINE.notify("อนุหภูมิ " + String(tem_) + " C ความชื้น " + String(hum_) + " %" );
      LINE.notify("ฝุ่นละออง PM 2.5 " + String(pm1) + " มคก./ลบ.ม");
      LINE.notify("คาร์บอนมอนอกไซด์ " + String(co_) + " ppm");
      LINE.notify("ความเข้มแสง " + String(lux_) + " lux");
    } else {
      static bool pmCheck = true, coCheck = true;
      if (pm1 > 50 && pmCheck == true) {
        pmCheck = false;
        LINE.notify("ฝุ่นละออง PM 2.5 " + String(pm1) + " มคก./ลบ.ม");
        LINE.notify("ฝุ่นละออง PM 2.5 สูงกว่าค่ามาตราฐานอาจเป็นอันตรายได้หากไม่สวมเครื่องป้องกัน");
      }
      if (co_ > 200 && coCheck == true) {
        coCheck = false;
        LINE.notify("คาร์บอนมอนอกไซด์ " + String(co_) + " ppm");
        LINE.notify("คาร์บอนมอนอกไซด์สูงกว่าค่ามาตราฐานอาจเป็นอันตรายได้หากไม่สวมเครื่องป้องกัน");
      }
      if (0 == time_n("m").toInt() % 5) {
        coCheck = true;
        pmCheck = true;
        coo = true;
      }
    }
  }

  //Reset
  if (digitalRead(swithRS) == HIGH) {
    ESP.restart();
  }

  esp_task_wdt_reset();

  delay(10);
}
