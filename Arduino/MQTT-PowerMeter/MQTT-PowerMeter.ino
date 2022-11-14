/*
  Arduino IR-D0 to Mqtt Client

  The circuit:
  - Arduino ESP8266

  IR-D0 Interface
  http://www.mikrocontroller.net/attachment/89888/Q3Dx_D0_Spezifikation_v11.pdf

  PC (config) MQTT client:
  https://www.hivemq.com/blog/mqtt-cli/

  Enable Arduino OTA:
  mqtt pub -h hm-ccu3.rktech.home -V 3 -t home/energy/EZ1/cmd -m 'OTA'
  
  Toggle UDP Traces
  mqtt pub -h hm-ccu3.rktech.home -V 3 -t home/energy/EZ1/cmd -m 'TRACE_UDP'

*/



#include "mqtt_setup.h"

#include <ArduinoMqttClient.h>
#include <ESP8266WiFi.h>

// Trace and OTA
#include <WiFiUdp.h>

#ifdef OTA_ALLOWED
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
char otaFlag = false;
#endif


const int  led = D7;
const bool led_on = false;    // Low
const bool led_off = true;    // High

const int  rxControl = D6;
const bool rxEnable =  false; // Low
const bool rxDisable = true;  // High

const int  rxLedControl = D5;
const bool rxLedEnable =  true;  // High
const bool rxLedDisable = false; // Low



///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate 
//    flashed in the WiFi module.

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

#ifdef TRACE_UDP
  WiFiUDP Udp;
  
  IPAddress     udp_ip(TRACE_UDP_IP);
  unsigned int  udp_port = TRACE_UDP_PORT;
  bool udpTraceFlag = false;
#endif

bool sys_data_send_ADC = false;
bool trace_begin = true;

unsigned long previousMillis = 0; // TEST
uint32_t      test_power = 0;     // TEST

unsigned long previousMillisDataP =       0;
unsigned long previousMillisDataEs =      0;
unsigned long previousMillisDataE =       0;
unsigned long previousMillisJson =        0;
unsigned long previousMillisSysDataRSSI = 0;
unsigned long previousMillisSysDataADC =  0;
unsigned long previousMillisSysData =     0;

unsigned long timer = 0;            // Sensor read time interval to calculate P
unsigned long timerData = 0;        // to Calculate P
unsigned long timerJson = 0;        // to Calculate P

uint64_t      energy = 0;           // Es in 0.1Wh
uint64_t      energyDailyOffset = 0;// to calculate E in 0.1Wh (offset value)
uint64_t      last_power_data = 0;  // last energy value used to Calculate P
uint64_t      last_power_json = 0;  // last energy value used to Calculate P JSON

#define traceStr(x) trace(String(x))
#define traceStrPSTR(x) trace(String(PSTR(x)))

void trace(String data="\n") {
#ifdef TRACE_UDP
  if(udpTraceFlag){
    if(WiFi.status() == WL_CONNECTED){
      if (trace_begin){
        Udp.beginPacket(udp_ip, udp_port);
        trace_begin = false;
      }
      Udp.print(data);
      if (data == "\n") {
        Udp.endPacket(); 
        trace_begin = true;
      }
    }
  }
#endif
#ifdef TRACE_UART
  Serial.print(data);
#endif
}


// ************* OTA *************************************************************
// Port defaults to 8266
// ArduinoOTA.setPort(8266);

// Hostname defaults to esp8266-[ChipID]
// ArduinoOTA.setHostname("myesp8266");

// No authentication by default
// ArduinoOTA.setPassword("admin");

// Password can be set with it's md5 value as well
// MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
// ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");



void ota_onStart(void) {
  String type;
  if (ArduinoOTA.getCommand() == U_FLASH) {
    type = "sketch";
  } else { // U_FS
    type = "filesystem";
  }

  // NOTE: if updating FS this would be the place to unmount FS using FS.end()
  traceStrPSTR("OTA: Start updating ");
  traceStr(type);
  trace();
}

void ota_onEnd(void) {
  traceStrPSTR("OTA: End");
  trace();
}

void ota_onProgress(unsigned int progress, unsigned int total) {
  static unsigned int progress_last = 99999;
  if(progress_last != progress){
    progress_last = progress;
    traceStrPSTR("OTA: ");
    traceStr(progress / (total / 100) );
    trace();
  }
}

void ota_onError(ota_error_t error) {
  traceStrPSTR("OTA: Error ");
  traceStr(error);
  traceStrPSTR(", ");
  if (error == OTA_AUTH_ERROR) {
    traceStrPSTR("Auth Failed");
  } else if (error == OTA_BEGIN_ERROR) {
    traceStrPSTR("Begin Failed");
  } else if (error == OTA_CONNECT_ERROR) {
    traceStrPSTR("Connect Failed");
  } else if (error == OTA_RECEIVE_ERROR) {
    traceStrPSTR("Receive Failed");
  } else if (error == OTA_END_ERROR) {
    traceStrPSTR("End Failed");
  }
  trace();
}


// ************* MQTT ****************************************************
void mqtt_onMessage(int messageSize) {
  // we received a message, print out the topic and contents
  uint8_t inBuffer[40];

  traceStrPSTR("Received a message with topic '");
  traceStr(mqttClient.messageTopic());
  traceStrPSTR("', length ");
  traceStr(messageSize);
  traceStrPSTR(" bytes: ");
  mqttClient.read(inBuffer, sizeof(inBuffer)-1);
  if (messageSize > sizeof(inBuffer)-1) {messageSize = sizeof(inBuffer)-1;}
  inBuffer[messageSize] = 0;
  String inStr = (char*)inBuffer;
  traceStr(inStr);
  trace();

  if (inStr == "CLR"){
    energyDailyOffset = energy;
    traceStrPSTR("Clear Energy counter");
    trace();
  }

#ifdef OTA_ALLOWED          
  if (inStr == "OTA"){
    otaFlag = true;
    traceStrPSTR("Enable OTA");
    ArduinoOTA.onStart(ota_onStart);
    ArduinoOTA.onEnd(ota_onEnd);
    ArduinoOTA.onProgress(ota_onProgress);
    ArduinoOTA.onError(ota_onError);
    ArduinoOTA.begin();
    trace();
  }
#endif          

#ifdef TRACE_UDP
  if (inStr == "TRACE_UDP"){
    if(udpTraceFlag){
      traceStrPSTR("Disable UDP trace sending");
      trace();
      udpTraceFlag = false;
    }
    else {
      traceStrPSTR("Enable UDP trace sending");
      trace();
      udpTraceFlag = true;
    }
  }
#endif

  if (inStr == "RESET"){
    traceStrPSTR("Reset by Watchdog!");
    trace();
    delay(1000);
    while(true){;}
  }

  if (inStr == "ADC"){
    if(sys_data_send_ADC){
      traceStrPSTR("Disable ADC sending");
      trace();
      sys_data_send_ADC = false;
    }
    else {
      traceStrPSTR("Enable ADC sending");
      trace();
      sys_data_send_ADC = true;
    }
  }

  if (inStr == "SCAN"){
    String ssid;
    int32_t rssi;
    uint8_t encryptionType;
    uint8_t* bssid;
    int32_t channel;
    bool hidden;
    int scanResult;
    char outStr[100];
  
    traceStrPSTR("Starting WiFi scan...");
    trace();
  
    scanResult = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
  
    if (scanResult == 0) {
      traceStrPSTR("No networks found");
      trace();

      mqttClient.beginMessage(topic_scan);
      mqttClient.print(PSTR("{\"networks\":0}"));
      mqttClient.endMessage();

    } else if (scanResult > 0) {
      traceStr(scanResult);
      traceStrPSTR(" networks found:");
      trace();
  
      // Print unsorted scan results
      String mqttOut;

      for (int8_t i = 0; i < scanResult; i++) {
        WiFi.getNetworkInfo(i, ssid, encryptionType, rssi, bssid, channel, hidden);
        // trace
        sprintf(outStr, PSTR("  %02d: [CH %02d] [%02X:%02X:%02X:%02X:%02X:%02X] %ddBm %c %c %s"),
                      i,
                      channel,
                      bssid[0], bssid[1], bssid[2],
                      bssid[3], bssid[4], bssid[5],
                      rssi,
                      (encryptionType == ENC_TYPE_NONE) ? ' ' : '*',
                      hidden ? 'H' : 'V',
                      ssid.c_str());
        traceStr(outStr);
        trace();
        // mqtt
        sprintf(outStr, PSTR("  {\"channel\":%02d, \"MAC\":\"%02X:%02X:%02X:%02X:%02X:%02X\", \"RSSI\":%d \"enc\":%s, \"hidden\":%s, \"SSID\":\"%s\"}%s\n"),
                      channel,
                      bssid[0], bssid[1], bssid[2],
                      bssid[3], bssid[4], bssid[5],
                      rssi,
                      (encryptionType == ENC_TYPE_NONE) ? "false" : "true",
                      hidden ? "true" : "false",
                      ssid.c_str(),
                      ((i+1)==scanResult) ? "" : ","
                );
        mqttOut += String(outStr);

        delay(0);
      }
      
      sprintf(outStr, PSTR(" \"networks\":%d,\n"), scanResult);
      mqttOut = "{\n" + String(outStr) + " \"network\":[\n" + mqttOut + " ]\n}";
      mqttClient.beginMessage(topic_scan, (unsigned long)mqttOut.length());
      mqttClient.print(mqttOut);
      mqttClient.endMessage();
    } else {
      traceStrPSTR("WiFi scan error ");
      traceStr(scanResult);
      trace();

      mqttClient.beginMessage(topic_scan);
      mqttClient.print(PSTR("{\"networks\":-1}"));
      mqttClient.endMessage();
    }
  }

}





// ************* Read Power-Meter UART **************************************
// return E in 0.1Wh
// return 0 if no new Value received
//

/*
 IR-Data 9600 Baud
/LOG5LK13BE803039<\r><\n>
<\r><\n>
1-0:96.1.0*255(001LOG0065193416)<\r><\n>
1-0:1.8.0*255(018342.3311*kWh)<\r><\n>
1-0:2.8.0*255(000000.0000*kWh)<\r><\n>
1-0:0.2.0*255(ver.03,432F,20170504)<\r><\n>
1-0:96.90.2*255(0F66)<\r><\n>
1-0:97.97.0*255(00000000)<\r><\n>
!<\r><\n>
*/

#define UART_BUFFER_LEN 1024
#define ENERGY_VALID_TIME   5 // sec
#define ENERGY_VALID_VALUE 60 // arround 22kWh => ((32A * 230V * 3) / 3600s) * 10 (value in 0.1Wh)
uint64_t uartHandler(void) {
  static int            uartRxCnt = 0;
  static uint64_t       energyLastValid = 0;      // to check if value changed
  static unsigned long  energyLastValidTime = 0;  // to get last read value

  char uartRxBuffer[UART_BUFFER_LEN];
  uint64_t energyRead = 0;
    
  while (Serial.available()) {
    int serInput = Serial.read();
    if (serInput == '\n') {
      uartRxBuffer[uartRxCnt] = 0x00;
      uartRxCnt = 0;
      String readstring = String(uartRxBuffer);

      // readstring = "1-0:1.8.0*255(018342.3311*kWh)\r\n" // TEST-INPUT

      if ((readstring.substring(0,14) == "1-0:1.8.0*255(") and (readstring.substring(25,30) == "*kWh)")) {
        unsigned long readTime = millis(); // in ms
        unsigned long readTimeDelta = (readTime - energyLastValidTime +500) / 1000;   // in sec
        if (readTimeDelta == 0) {
          readTimeDelta = 1;
        }
        
        energyRead = (uint64_t)String(readstring.substring(14,20)).toInt() * (uint64_t)10000;
        energyRead += (uint64_t)String(readstring.substring(21,25)).toInt();
        
        traceStrPSTR("UART RX:           <");
        traceStr(readstring);
        traceStrPSTR("> (dt=");
        traceStr(readTime - energyLastValidTime);
        traceStrPSTR(" dE=");
        traceStr(energyRead - energyLastValid);
        traceStrPSTR(" limit=");
        traceStr(ENERGY_VALID_VALUE * readTimeDelta);
        traceStrPSTR(")");
        trace();

        if(energyLastValid == 0){
          // only happens on startup
          energyLastValid = energyRead;
        }
        else{
          if(readTimeDelta > ENERGY_VALID_TIME) {
            // ignore first value after longer gap
            traceStrPSTR("UART RX ERROR 1");
            trace();
            energyLastValidTime = readTime;
            energyRead = 0;
          }
          else if(energyRead < energyLastValid) {
            // ignore values less then last valid value
            traceStrPSTR("UART RX ERROR 2");
            trace();
            energyRead = 0;
          }
          else if((readTimeDelta <= ENERGY_VALID_TIME) and (energyRead > (energyLastValid + (uint64_t)(ENERGY_VALID_VALUE * readTimeDelta)))) {
            // ignore values greater then max possible power
            traceStrPSTR("UART RX ERROR 3");
            trace();
            energyRead = 0;
          }
          else { 
            // Value seems to be OK
            energyLastValid = energyRead;
            energyLastValidTime = readTime;
          }
        }
      }
      else {
        traceStrPSTR("UART RX (unknown): <");
        traceStr(readstring);
        traceStrPSTR(">");
        trace();
      }
      //energy;
      
    }
    else {
      if (uartRxCnt < UART_BUFFER_LEN-1){
        if ((serInput >= 0x20) and (serInput <= 0x7F)) {
          uartRxBuffer[uartRxCnt++] = (char)serInput;
        }
      }
    }
    
  }        
  return(energyRead);
}



void setup() {
  //Initialize Serial and wait for port to open:
  pinMode(rxControl, OUTPUT);
  digitalWrite(rxControl, rxEnable);

  pinMode(rxLedControl, OUTPUT);
  digitalWrite(rxLedControl, rxLedEnable);

  pinMode(led, OUTPUT);

  Serial.begin(IR_UART_BAUD, IR_UART_MODE); // Baud rate for Power-Meter

  // ----- 3 x slow red blinking on startup -----
  digitalWrite(led, led_on);
  delay(500);
  digitalWrite(led, led_off);
  delay(500);
  digitalWrite(led, led_on);
  delay(500);
  digitalWrite(led, led_off);
  delay(500);
  digitalWrite(led, led_on);
  delay(500);
  digitalWrite(led, led_off);
  
  trace();
  traceStrPSTR("**********************************");
  traceStrPSTR("Attempting to connect to WPA SSID: ");
  traceStr(ssid);
  trace();

  WiFi.persistent(false);   // daten nicht in Flash speichern
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  //while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
  while (WiFi.status() != WL_CONNECTED) {
    // failed, retry
    traceStrPSTR(".");
    // ----- fast red blinking during WIFI connect -----
    digitalWrite(led, led_on);
    delay(150);
    digitalWrite(led, led_off);
    delay(150);
  }
  trace();

  traceStrPSTR("You're connected to the network");
  trace();
  trace();
}



#define BLINK_CNT_100ms 10
int blink_led_cnt = 0;

void loop() {
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  // to avoid having delays in loop, we'll use the strategy from BlinkWithoutDelay
  // see: File -> Examples -> 02.Digital -> BlinkWithoutDelay for more info
  unsigned long currentMillis = millis();


  /*
  // Get new Power Meter Data 
  if (currentMillis - previousMillis >= 1545) {
    // save the last time a message was sent
    previousMillis = currentMillis;

    uint64_t energyRead; //0.1Wh
    timer = millis();
    
    // new sensor data
    // TEST --------------------
    energyRead = energy + (uint64_t)test_power;
    //test_power += 2;
    test_power = 1;
    if (test_power > 14) {test_power = 1;}
    // TEST --------------------

    energy = energyRead;
    if (energyDailyOffset == 0) {
      // set Daily energy to 0 on startup
      energyDailyOffset = energy;

      // set initial Power values JSON
      last_power_json = energy;
      timerJson = timer;

      // set initial Power values
      last_power_data = energy;
      timerData = timer;
    }

    uint64_t energy_dayly_int = (energy - energyDailyOffset) / 10;
    uint64_t energy_dayly_dec = (energy - energyDailyOffset) % 10;
    uint64_t energy_int = energy / 10;
    uint64_t energy_dec = energy % 10;

    traceStr("Get Power Meter data: E=");
    traceStr(energy_dayly_int);
    traceStr(".");
    traceStr(energy_dayly_dec);
    traceStr("Wh, Es=");
    traceStr(energy_int);
    traceStr(".");
    traceStr(energy_dec);
    traceStr("Wh");
    trace();
  }
  */
  
  uint64_t energy_tmp; //0.1Wh
  energy_tmp = uartHandler();
  if (energy_tmp) {
    energy = energy_tmp;
    timer = millis();

    if (energyDailyOffset == 0) {
      // set Daily energy to 0 on startup
      energyDailyOffset = energy;

      // set initial Power values JSON
      last_power_json = energy;
      timerJson = timer;

      // set initial Power values
      last_power_data = energy;
      timerData = timer;
    }

    uint64_t energy_dayly_int = (energy - energyDailyOffset) / 10;
    uint64_t energy_dayly_dec = (energy - energyDailyOffset) % 10;
    uint64_t energy_int = energy / 10;
    uint64_t energy_dec = energy % 10;

    traceStrPSTR("Get Power Meter data: E=");
    traceStr(energy_dayly_int);
    traceStrPSTR(".");
    traceStr(energy_dayly_dec);
    traceStrPSTR("Wh, Es=");
    traceStr(energy_int);
    traceStrPSTR(".");
    traceStr(energy_dec);
    traceStrPSTR("Wh, JSON: t= ");
    traceStr(timerJson);
    traceStrPSTR(" E= ");
    traceStr(last_power_json);
    traceStrPSTR(" Data: t= ");
    traceStr(timerData);
    traceStrPSTR(" E= ");
    traceStr(last_power_data);
    
    trace();
  }


  if(!mqttClient.connected()) {
    traceStrPSTR("Attempting to connect to the MQTT broker: ");
    traceStr(broker);
    trace();
  
    if (!mqttClient.connect(broker, port)) {
      traceStrPSTR("MQTT connection failed! Error code = ");
      traceStr(mqttClient.connectError());
    }
    else {    
      traceStrPSTR("You're connected to the MQTT broker!");
    }
    // set the message receive callback
    mqttClient.onMessage(mqtt_onMessage);
    // subscribe to a topic
    mqttClient.subscribe(topic_CMD);
    // topics can be unsubscribed using:
    // mqttClient.unsubscribe(topic);

    trace();
  }
  else{
    // --------------------- MQTT send topic_P --------------------------------------
    if (data_interval_P and (currentMillis - previousMillisDataP >= data_interval_P)) {
      // save the last time a message was sent
      previousMillisDataP = currentMillis;

      // Calculate and Send Power
      uint32_t akt_power = 0;
      unsigned long timePeriod = timer - timerData;
      unsigned long timePeriod_s = (timePeriod + 500) / 1000;

      if ((timePeriod_s > 0) and (energy > last_power_data)) {
        if (last_power_data != 0) {
          akt_power = (energy - last_power_data) * 3600ull / timePeriod_s;
        }
        uint32_t akt_power_int = akt_power / 10;
        uint32_t akt_power_dec = akt_power % 10;
            
        traceStrPSTR("Send MQTT P=");
        traceStr(akt_power_int);
        traceStrPSTR(".");
        traceStr(akt_power_dec);
        traceStrPSTR("W (dt=");
        traceStr(timePeriod);
        traceStrPSTR("ms/");
        traceStr(timePeriod_s);
        traceStrPSTR("s, dE=");
        traceStr(energy - last_power_data);
        traceStrPSTR("dWh)");
        trace();
    
        // send message, the Print interface can be used to set the message contents
        mqttClient.beginMessage(topic_P);
        mqttClient.print(akt_power_int);
        mqttClient.print(".");
        mqttClient.print(akt_power_dec);
        mqttClient.endMessage();

        last_power_data = energy;
        timerData = timer;
        blink_led_cnt = BLINK_CNT_100ms;
      }
      else {
        traceStrPSTR("NOT Sending MQTT P: no new Energy value");
        trace();
      }
    }

    // --------------------- MQTT send topic_Es -------------------------------------
    if (data_interval_Es and (currentMillis - previousMillisDataE >= data_interval_Es)) {
      // save the last time a message was sent
      previousMillisDataEs = currentMillis;

      // Calculate and Send Energy
      uint64_t energy_int = energy / 10;
      uint64_t energy_dec = energy % 10;
            
      traceStrPSTR("Send MQTT Es=");
      traceStr(energy/10);
      traceStrPSTR(".");
      traceStr(energy%10);
      traceStrPSTR("Wh");
      trace();
  
      // send message, the Print interface can be used to set the message contents
      mqttClient.beginMessage(topic_Es);
      mqttClient.print(energy_int);
      mqttClient.print(".");
      mqttClient.print(energy_dec);
      mqttClient.endMessage();

      blink_led_cnt = BLINK_CNT_100ms;
    }

    // --------------------- MQTT send topic_E --------------------------------------
    if (data_interval_E and (currentMillis - previousMillisDataE >= data_interval_E)) {
      // save the last time a message was sent
      previousMillisDataE = currentMillis;

      if (energy > 0) {
        // Calculate and Send Energy
        uint64_t energy_dayly_int = (energy - energyDailyOffset) / 10;
        uint64_t energy_dayly_dec = (energy - energyDailyOffset) % 10;
              
        traceStrPSTR("Send MQTT E=");
        traceStr(energy_dayly_int);
        traceStrPSTR(".");
        traceStr(energy_dayly_dec);
        traceStrPSTR("Wh");
        trace();
    
        // send message, the Print interface can be used to set the message contents
        mqttClient.beginMessage(topic_E);
        mqttClient.print(energy_dayly_int);
        mqttClient.print(".");
        mqttClient.print(energy_dayly_dec);
        mqttClient.endMessage();
  
        blink_led_cnt = BLINK_CNT_100ms;
      }
      else {
        traceStrPSTR("NOT Sending MQTT E: no Energy value");
        trace();
      }
    }

    // --------------------- MQTT send topic_RSSI ------------------------------------
    if (sys_data_interval_RSSI and (currentMillis - previousMillisSysDataRSSI >= sys_data_interval_RSSI)) {
      // save the last time a message was sent
      previousMillisSysDataRSSI = currentMillis;

      long wifi_rssi = WiFi.RSSI();
      traceStrPSTR("Send MQTT RSSI=");
      traceStr(wifi_rssi);
      traceStrPSTR("dB");
      trace();

      mqttClient.beginMessage(topic_RSSI);
      mqttClient.print(wifi_rssi);
      mqttClient.endMessage();

      blink_led_cnt = BLINK_CNT_100ms;
    }

    // --------------------- MQTT send topic_ADC ------------------------------------
    if (sys_data_interval_ADC and (currentMillis - previousMillisSysDataADC >= sys_data_interval_ADC)) {
      // save the last time a message was sent
      previousMillisSysDataADC = currentMillis;
      
      if(sys_data_send_ADC){
        float adcValue = (float)(analogRead(A0)*3) / 1000.0;
        traceStrPSTR("Send MQTT ADC=");
        traceStr(adcValue);
        traceStrPSTR("V");
        trace();
  
        mqttClient.beginMessage(topic_ADC);
        mqttClient.print(adcValue);
        mqttClient.endMessage();
  
        blink_led_cnt = BLINK_CNT_100ms;
      }
    }

    // --------------------- MQTT send topic_MAC and topic_IP -----------------------
    if (sys_data_interval and (currentMillis - previousMillisSysData >= sys_data_interval)) {
      // save the last time a message was sent
      previousMillisSysData = currentMillis;

      String wifiMac = WiFi.macAddress();
      String wifiIP = WiFi.localIP().toString();
      traceStrPSTR("Send MQTT IP=");
      traceStr(wifiIP);
      traceStrPSTR(", MAC=");
      traceStr(wifiMac);
      trace();

      mqttClient.beginMessage(topic_MAC);
      mqttClient.print(wifiMac);
      mqttClient.endMessage();

      mqttClient.beginMessage(topic_IP);
      mqttClient.print(wifiIP);
      mqttClient.endMessage();
      
      blink_led_cnt = BLINK_CNT_100ms;
    }

    // --------------------- MQTT send topic_json -----------------------------------
    if (json_interval and (currentMillis - previousMillisJson >= json_interval)) {
      // save the last time a message was sent
      previousMillisJson = currentMillis;

      // Calculate Power
      uint32_t akt_power = 0;
      bool akt_power_valid = false;
      unsigned long timePeriod = timer - timerJson;
      unsigned long timePeriod_s = (timePeriod + 500) / 1000;

      if ((timePeriod_s > 0) and (energy > last_power_json)) {
        if (last_power_json != 0) {
          akt_power = (energy - last_power_json) * 3600ull / timePeriod_s;
          akt_power_valid = true;
        }
      }
      else {
        traceStrPSTR("NOT Sending Power in JSON message: no new Energy value");
        trace();
      }

      traceStrPSTR("Send JSON MQTT: ");
      mqttClient.beginMessage(topic_json);
      mqttClient.print("{");

      bool not_first_value = false;

      // ---------------- JSON P ---------------------
      if (akt_power_valid and json_P) {
        uint32_t akt_power_int = akt_power / 10;
        uint32_t akt_power_dec = akt_power % 10;

        traceStrPSTR("P=");
        traceStr(akt_power_int);
        traceStrPSTR(".");
        traceStr(akt_power_dec);
        traceStrPSTR("W (dt=");
        traceStr(timePeriod);
        traceStrPSTR("ms/");
        traceStr(timePeriod_s);
        traceStrPSTR("s, dE=");
        traceStr(energy - last_power_json);
        traceStrPSTR("dWh)");

        mqttClient.print("\"P\":");
        mqttClient.print(akt_power_int); 
        mqttClient.print("."); 
        mqttClient.print(akt_power_dec);
        not_first_value = true;
      }

      // ---------------- JSON E ---------------------
      if (json_E) {
        uint64_t energy_dayly_int = (energy - energyDailyOffset) / 10;
        uint64_t energy_dayly_dec = (energy - energyDailyOffset) % 10;

        if (not_first_value) {
          traceStrPSTR(", ");
          mqttClient.print(", ");
        }
        traceStrPSTR("E=");
        traceStr(energy_dayly_int);
        traceStrPSTR(".");
        traceStr(energy_dayly_dec);
        traceStrPSTR("Wh");

        mqttClient.print("\"E\":");
        mqttClient.print(energy_dayly_int); 
        mqttClient.print("."); 
        mqttClient.print(energy_dayly_dec);
        not_first_value = true;
      }

      // ---------------- JSON Es ---------------------
      if ((energy > 0) and json_Es) {
        uint64_t energy_int = energy / 10;
        uint64_t energy_dec = energy % 10;

        if (not_first_value) {
          traceStr(", ");
          mqttClient.print(", ");
        }
        traceStrPSTR("Es=");
        traceStr(energy_int);
        traceStrPSTR(".");
        traceStr(energy_dec);
        traceStrPSTR("Wh");

        mqttClient.print("\"Es\":");
        mqttClient.print(energy_int); 
        mqttClient.print("."); 
        mqttClient.print(energy_dec);
        not_first_value = true;
      }

      // ---------------- JSON RSSI ---------------------
      if (json_RSSI) {
        long wifi_rssi = WiFi.RSSI();

        if (not_first_value) {
          traceStr(", ");
          mqttClient.print(", ");
        }
        traceStrPSTR("RSSI=");
        traceStr(wifi_rssi);
        traceStrPSTR("dB");

        mqttClient.print("\"RSSI\":");
        mqttClient.print(wifi_rssi);
        not_first_value = true;
      }

      // ---------------- JSON MAC ---------------------
      if (json_MAC) {
        String wifiMac = WiFi.macAddress();

        if (not_first_value) {
          traceStr(", ");
          mqttClient.print(", ");
        }
        traceStrPSTR("MAC=");
        traceStr(wifiMac);

        mqttClient.print("\"MAC\":\"");
        mqttClient.print(wifiMac);
        mqttClient.print("\"");
        not_first_value = true;
      }

      // ---------------- JSON IP ---------------------
      if (json_IP) {
        String wifiIP = WiFi.localIP().toString();

        if (not_first_value) {
          traceStrPSTR(", ");
          mqttClient.print(", ");
        }
        traceStrPSTR("IP=");
        traceStr(wifiIP);

        mqttClient.print("\"IP\":\"");
        mqttClient.print(wifiIP);
        mqttClient.print("\"");
        not_first_value = true;
      }

      // ---------------- JSON ADC ---------------------
      if (json_ADC) {
        float adcValue = (float)(analogRead(A0)*3) / 1000.0;

        if (not_first_value) {
          traceStrPSTR(", ");
          mqttClient.print(", ");
        }
        traceStrPSTR("ADC=");
        traceStr(adcValue);
        traceStrPSTR("V");

        mqttClient.print("\"ADC\":");
        mqttClient.print(adcValue);
        not_first_value = true;
      }

      // ---------------- JSON end of message ---------------------
      trace();
      
      mqttClient.print("}");
      mqttClient.endMessage();

      last_power_json = energy;
      timerJson = timer;
      blink_led_cnt = BLINK_CNT_100ms;
    }
  }

#ifdef OTA_ALLOWED          
  if (otaFlag){
    //Serial1.println("OTA");
    ArduinoOTA.handle();  
  }
#endif

  if (blink_led_cnt) {
     digitalWrite(led, led_on);
     blink_led_cnt--;
  }
  else {
     digitalWrite(led, led_off);
  }
  delay(10);  

}
