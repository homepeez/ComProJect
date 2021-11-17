void Firebase_data(void *pvParameters) {
  for (;;) {
    digitalWrite(FAN,HIGH);
    tem_ = sensor.readTemperature();
    hum_ = sensor.readHumidity();
    co_ = MQRead(A7,Co_calibration_one);
    lux_ = LightSensor.GetLightIntensity();
    pms.requestRead();
    if (pms.readUntil(data)) {
      pm1 = data.PM_AE_UG_2_5;
    }

    // Debog data
    if (!isnan(tem_) && !isnan(hum_)) {
      Serial.println("----------------------------------------");
      Serial.print(tem_); Serial.print(" C Tem1 ");
      Serial.print(hum_); Serial.print(" % Hum1 PM2.5 :");
      Serial.print(pm1); Serial.print(" ug/m3 ");
      Serial.print("Co :"); Serial.print(co_);
      Serial.print(" Lux:"); Serial.println(lux_);
      Serial.println("----------------------------------------\n");
      // average
      tem_s += tem_;
      hum_s += hum_;
      dCO += co_;
      dPM25 += pm1;
      dLux += lux_;
      nSum++;
    }
    static uint8_t i_data, nn;
    String time__d = time_Data(sTime_m, true);
    if (time_n("h") + ":" + time_n("m") == time__d) {
      String time__ = time_n("h") + ":" + time_n("m");
      String Day__ = time_n("d") + "-" + time_n("M") + "-" + time_n("Y");
      tem_s /= nSum;
      hum_s /= nSum;
      dPM25 /= nSum;
      dLux /= nSum;
      dCO /= nSum;
      if (WiFi.status() != WL_CONNECTED) {
        dataLux[i_data] = dLux;
        dataCO[i_data] = dCO;
        dataTem[i_data] = tem_s;
        dataHum[i_data] = hum_s;
        dataPM[i_data] = dPM25;
        time_da[i_data] = time__;
        Day_da[i_data] = Day__;
        i_data++;
      } else {
        Serial.println("Firebase Data");
        firebase_data(tem_s, hum_s, dPM25, dCO, dLux, time__, Day__);
      } tem_s = 0; hum_s = 0; dPM25 = 0; dLux = 0; dCO = 0;
      nSum = 0; time__d = time_Data(sTime_m, false);
    }  else {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi connection error!");
        WiFi.reconnect();
      } else {
        Firebase.setFloat("Realtime/tem", String(tem_).toFloat());
        Firebase.setFloat("Realtime/hum", String(hum_).toFloat());
        Firebase.setInt("Realtime/pm", pm1);
        Firebase.setInt("Realtime/CO", co_);
        Firebase.setInt("Realtime/lux", lux_);
        if (Firebase.getBool("getData/request") == true) {
          sLEDM_1 = Firebase.getString("getData/sLEDM_1");
          sLEDM_2 = Firebase.getString("getData/sLEDM_2");
          sLEDM_3 = Firebase.getString("getData/sLEDM_3");
          sLine_m = Firebase.getString("getData/sLine_m");
          sTime_m = Firebase.getInt("getData/sTime_m");
          Firebase.setBool("getData/request", false);
        }
      }

      // เช็คข้อมูลตกค้างช่วง Wi-Fi หลุด
      while (i_data != 0) {
        firebase_data(dataTem[nn], dataHum[nn], dataPM[nn], dataCO[nn], dataLux[nn], time_da[nn], Day_da[nn]);
        i_data--;
        nn++;
        delay(100);
      } nn = 0;

      // Debug Time Hz
      Serial.println("Time Data " + time_n("h") + ":" + time_n("m") + " -> " + time__d );
      
      digitalWrite(FAN,LOW);
      vTaskDelay(1111);
    }
  }
}
