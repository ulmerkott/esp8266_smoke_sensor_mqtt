#include <FS.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

#include "settings.h"

// Initiate WIFI client
WiFiClient espClient;

// * Initiate MQTT client
PubSubClient mqtt_client(espClient);

// **********************************
// * WIFI                           *
// **********************************

// Gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println(F("Entered config mode"));
    Serial.println(WiFi.softAPIP());

    // If you used auto generated SSID, print it
    Serial.println(myWiFiManager->getConfigPortalSSID());
}

// **********************************
// * MQTT                           *
// **********************************

// Send a message to a broker topic
void send_mqtt_message(const char *topic, char *payload) {
    Serial.printf("MQTT Outgoing on %s: ", topic);
    Serial.println(payload);

    bool result = mqtt_client.publish(topic, payload, false);

    if (!result) {
        Serial.printf("MQTT publish to topic %s failed\n", topic);
    }
}

// Reconnect to MQTT server and subscribe to in and out topics
bool mqtt_reconnect() {
    // Loop until we're reconnected
    int MQTT_RECONNECT_RETRIES = 0;

    while (!mqtt_client.connected() && MQTT_RECONNECT_RETRIES < MQTT_MAX_RECONNECT_TRIES) {
        MQTT_RECONNECT_RETRIES++;
        Serial.printf("MQTT connection attempt %d / %d ...\n", MQTT_RECONNECT_RETRIES, MQTT_MAX_RECONNECT_TRIES);

        // Attempt to connect
        if (mqtt_client.connect(HOSTNAME, MQTT_USER, MQTT_PASS)) {
            Serial.println(F("MQTT connected!"));

            // Once connected, publish an announcement...
            char *message = new char[16 + strlen(HOSTNAME) + 1];
            strcpy(message, "smoke sensor alive: ");
            strcat(message, HOSTNAME);
            mqtt_client.publish("hass/status", message);

            Serial.printf("MQTT root topic: %s\n", MQTT_ROOT_TOPIC);
        }
        else {
            Serial.print(F("MQTT Connection failed: rc="));
            Serial.println(mqtt_client.state());
            Serial.println(F(" Retrying in 5 seconds"));
            Serial.println("");

            // Wait 5 seconds before retrying
            delay(5000);
        }
    }

    if (MQTT_RECONNECT_RETRIES >= MQTT_MAX_RECONNECT_TRIES) {
        Serial.printf("*** MQTT connection failed, giving up after %d tries ...\n", MQTT_RECONNECT_RETRIES);
        return false;
    }

    return true;
}

void send_metric(String name, long metric)
{
    Serial.print(F("Sending metric to broker: "));
    Serial.print(name);
    Serial.print(F("="));
    Serial.println(metric);

    char output[10];
    ltoa(metric, output, sizeof(output));

    String topic = String(MQTT_ROOT_TOPIC) + "/" + name;
    send_mqtt_message(topic.c_str(), output);
}

void send_data_to_broker() {
    LAST_UPDATE_SENT = millis();
    send_metric("current_value", CURRENT_SMOKE_SENSOR_DATA);
}

// **********************************
// * EEPROM helpers                 *
// **********************************

String read_eeprom(int offset, int len) {
    Serial.print(F("read_eeprom()"));

    String res = "";
    for (int i = 0; i < len; ++i) {
        res += char(EEPROM.read(i + offset));
    }
    return res;
}

void write_eeprom(int offset, int len, String value) {
    Serial.println(F("write_eeprom()"));
    for (int i = 0; i < len; ++i) {
        if ((unsigned)i < value.length()) {
            EEPROM.write(i + offset, value[i]);
        }
        else {
            EEPROM.write(i + offset, 0);
        }
    }
}

// ******************************************
// * Callback for saving WIFI config        *
// ******************************************

bool shouldSaveConfig = false;

// * Callback notifying us of the need to save config
void save_wifi_config_callback() {
    Serial.println(F("Should save config"));
    shouldSaveConfig = true;
}

// **********************************
// * Setup OTA                      *
// **********************************

void setup_ota() {
    Serial.println(F("Arduino OTA activated."));

    // Port defaults to 8266
    ArduinoOTA.setPort(8266);

    // Set hostname for OTA
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        Serial.println(F("Arduino OTA: Start"));
    });

    ArduinoOTA.onEnd([]() {
        Serial.println(F("Arduino OTA: End (Running reboot)"));
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Arduino OTA Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Arduino OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println(F("Arduino OTA: Auth Failed"));
        else if (error == OTA_BEGIN_ERROR)
            Serial.println(F("Arduino OTA: Begin Failed"));
        else if (error == OTA_CONNECT_ERROR)
            Serial.println(F("Arduino OTA: Connect Failed"));
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println(F("Arduino OTA: Receive Failed"));
        else if (error == OTA_END_ERROR)
            Serial.println(F("Arduino OTA: End Failed"));
    });

    ArduinoOTA.begin();
    Serial.println(F("Arduino OTA finished"));
}

// **********************************
// * Setup MDNS discovery service   *
// **********************************

void setup_mdns() {
    Serial.println(F("Starting MDNS responder service"));

    bool mdns_result = MDNS.begin(HOSTNAME);
    if (mdns_result) {
        MDNS.addService("http", "tcp", 80);
    }
}

// **********************************
// * Smoke sensor                   *
// **********************************

void read_smoke_sensor_data() {
    int new_data = analogRead(SMOKE_SENSOR_PIN);
    int diff = new_data - CURRENT_SMOKE_SENSOR_DATA;
    if (abs(diff) < SMOKE_SENSOR_DELTA_THESHOLD) {
      return;
    }
    CURRENT_SMOKE_SENSOR_DATA = new_data;
    Serial.printf("Got new smoke data: %d\n", CURRENT_SMOKE_SENSOR_DATA);
    send_data_to_broker();
}

// **********************************
// * Setup main                     *
// **********************************

void setup() {
    // Configure EEPROM
    EEPROM.begin(512);

    // Setup a hw serial connection
    Serial.begin(BAUD_RATE, SERIAL_8N1, SERIAL_FULL);
    Serial.println("");
    Serial.println("Swapping UART0 RX to inverted");
    Serial.flush();

    Serial.println("Serial port is ready to recieve.");

    // Disable blue LED by setting GPIO2 HIGH.
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Get MQTT Server settings
    String settings_available = read_eeprom(134, 1);

    if (settings_available == "1") {
        read_eeprom(0, 64).toCharArray(MQTT_HOST, 64);   // * 0-63
        read_eeprom(64, 6).toCharArray(MQTT_PORT, 6);    // * 64-69
        read_eeprom(70, 32).toCharArray(MQTT_USER, 32);  // * 70-101
        read_eeprom(102, 32).toCharArray(MQTT_PASS, 32); // * 102-133
    }

    WiFiManagerParameter CUSTOM_MQTT_HOST("host", "MQTT hostname", MQTT_HOST, 64);
    WiFiManagerParameter CUSTOM_MQTT_PORT("port", "MQTT port",     MQTT_PORT, 6);
    WiFiManagerParameter CUSTOM_MQTT_USER("user", "MQTT user",     MQTT_USER, 32);
    WiFiManagerParameter CUSTOM_MQTT_PASS("pass", "MQTT pass",     MQTT_PASS, 32);

    // WiFiManager local initialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    // Reset settings - uncomment for testing
    // wifiManager.resetSettings();

    // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);

    // Set timeout
    wifiManager.setConfigPortalTimeout(WIFI_TIMEOUT);

    // Set save config callback
    wifiManager.setSaveConfigCallback(save_wifi_config_callback);

    // Add all your parameters here
    wifiManager.addParameter(&CUSTOM_MQTT_HOST);
    wifiManager.addParameter(&CUSTOM_MQTT_PORT);
    wifiManager.addParameter(&CUSTOM_MQTT_USER);
    wifiManager.addParameter(&CUSTOM_MQTT_PASS);

    // Fetches SSID and pass and tries to connect
    // Reset when no connection after 10 seconds
    if (!wifiManager.autoConnect()) {
        Serial.println(F("Failed to connect to WIFI and hit timeout"));

        // Reset and try again, or maybe put it to deep sleep
        ESP.reset();
        delay(WIFI_TIMEOUT);
    }

    // Read updated parameters
    strcpy(MQTT_HOST, CUSTOM_MQTT_HOST.getValue());
    strcpy(MQTT_PORT, CUSTOM_MQTT_PORT.getValue());
    strcpy(MQTT_USER, CUSTOM_MQTT_USER.getValue());
    strcpy(MQTT_PASS, CUSTOM_MQTT_PASS.getValue());

    // Save the custom parameters to FS
    if (shouldSaveConfig) {
        Serial.println(F("Saving WiFiManager config"));

        write_eeprom(0, 64, MQTT_HOST);   // * 0-63
        write_eeprom(64, 6, MQTT_PORT);   // * 64-69
        write_eeprom(70, 32, MQTT_USER);  // * 70-101
        write_eeprom(102, 32, MQTT_PASS); // * 102-133
        write_eeprom(134, 1, "1");        // * 134 --> always "1"
        EEPROM.commit();
    }

    // If you get here you have connected to the WiFi
    Serial.println(F("Connected to WIFI..."));

    // Configure OTA
    setup_ota();

    // Startup MDNS Service
    setup_mdns();

    // Setup MQTT
    Serial.printf("MQTT connecting to: %s:%s\n", MQTT_HOST, MQTT_PORT);

    mqtt_client.setServer(MQTT_HOST, atoi(MQTT_PORT));
}

// **********************************
// * Main loop                           *
// **********************************
void loop() {
    ArduinoOTA.handle();
    long now = millis();

    if (!mqtt_client.connected()) {
        if (now - LAST_RECONNECT_ATTEMPT > 5000) {
            LAST_RECONNECT_ATTEMPT = now;

            if (mqtt_reconnect()) {
                LAST_RECONNECT_ATTEMPT = 0;
            }
        }
    }
    else {
        mqtt_client.loop();
    }
    
    if (now > SMOKE_SENSOR_STARTUP_TIME_MS
        && (now - LAST_UPDATE_SENT > UPDATE_INTERVAL)) {
        read_smoke_sensor_data();
    }
}
