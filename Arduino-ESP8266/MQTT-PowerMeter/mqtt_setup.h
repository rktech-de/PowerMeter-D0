#define VERSION      "V3.0.3"
#define VERSIONDATE  "11.11.2022"

// ************** Baudrate of IR Interface ************************
#define IR_UART_BAUD    9600
#define IR_UART_MODE    SERIAL_7E1

// ************** OTA *********************************************
#define OTA_ALLOWED

// ************** TRACE *******************************************
// For receiving UDP Traces
// use "nc -ul 4242" to read traces

//#define TRACE_UART
#define TRACE_UDP
#define TRACE_UDP_IP    192,168,1,2
#define TRACE_UDP_PORT  4242

// ************** WIFI ********************************************
#define SECRET_SSID "mySSID"
#define SECRET_PASS "myPasswd"

// ************** MQTT ********************************************
const char broker[] = "my.mqtt.broker";
int        port     = 1883;

// location / type / device / Value
#define MQTT_BASE_TOPPIC  "home/energy/EZ2/"
const char topic_P[] =    MQTT_BASE_TOPPIC "data/P";    // Leistung über 10s
const char topic_Es[] =   MQTT_BASE_TOPPIC "data/Es";   // Energie Absolut
const char topic_E[] =    MQTT_BASE_TOPPIC "data/E";    // Energie Täglich (reset cmd "CLR")
const char topic_RSSI[] = MQTT_BASE_TOPPIC "sys/RSSI";  // WIFI RSSI
const char topic_MAC[] =  MQTT_BASE_TOPPIC "sys/MAC";   // WIFI MAC
const char topic_IP[] =   MQTT_BASE_TOPPIC "sys/IP";    // WIFI IP
const char topic_ADC[] =  MQTT_BASE_TOPPIC "sys/ADC";   // Input-Comperator ADC Value
const char topic_scan[] = MQTT_BASE_TOPPIC "sys/scan";  // All visible WIFI APs, only on cmd request
const char topic_json[] = MQTT_BASE_TOPPIC "json";      // all data as JSON
const char topic_CMD[] =  MQTT_BASE_TOPPIC "cmd";       // Configuration

const long data_interval_P =         5000;              // for topic_P 
const long data_interval_Es =        5000;              // for topic_Es
const long data_interval_E =         5000;              // for topic_E
const long sys_data_interval_RSSI =  5000;              // for topic_RSSI
const long sys_data_interval_ADC  =  1000;              // for topic_ADC, need cmd => ADC to activate
const long sys_data_interval =      60000;              // for topic_IP and topic_MAC
const long json_interval =          30000;              // for topic_json
const bool json_P =     true;
const bool json_Es =    true;
const bool json_E =     true;
const bool json_RSSI =  true;
const bool json_MAC =   false;
const bool json_IP =    false;
const bool json_ADC =   false;
