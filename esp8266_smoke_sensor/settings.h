// Update treshold in milliseconds, messages will only be sent on this interval
#define UPDATE_INTERVAL 2000

// Baud rate for both hardware and software 
#define BAUD_RATE 115200

// The hostname of our little creature
#define HOSTNAME "smokesensor"

// The password used for OTA
#define OTA_PASSWORD "admin"

// Wifi timeout in milliseconds
#define WIFI_TIMEOUT 30000

// MQTT network settings
#define MQTT_MAX_RECONNECT_TRIES 10

// MQTT root topic
#define MQTT_ROOT_TOPIC "sensors/smoke"

// MQTT Last reconnection counter
long LAST_RECONNECT_ATTEMPT = 0;
long LAST_UPDATE_SENT = 0;

// To be filled with EEPROM data
char MQTT_HOST[64] = "";
char MQTT_PORT[6]  = "";
char MQTT_USER[32] = "";
char MQTT_PASS[32] = "";

// Smoke sensor
const int SMOKE_SENSOR_PIN = A0;

// Avoid initial spike in sensor data
const int SMOKE_SENSOR_STARTUP_TIME_MS = 10000;

// Only update if new sensor data differs to the previous value more than this threshold
const int SMOKE_SENSOR_DELTA_THESHOLD = 5;

int CURRENT_SMOKE_SENSOR_DATA = 0;
