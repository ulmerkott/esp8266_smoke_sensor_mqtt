# esp8266_smoke_sensor
Software for the ESP2866 that sends MQ135 smoke sensor data to an mqtt broker (with OTA firmware updates)

# Getting started
This setup requires:
- An esp8266 (d1 mini have been tested)
- A MQ135 smoke detector sensor

Compiling up using Arduino IDE:
- Ensure you have selected the right board
- Using the Tools->Manage Libraries... install `PubSubClient` and `WifiManager`
- In the file `settings.h` change `OTA_PASSWORD` to a safe secret value
- Flash the software

Finishing off:
- You should now see a new wifi network `ESP******` connect to this wifi network, a popup should appear, else manually navigate to `192.168.4.1`
- Configure your wifi and Mqtt settings
- To check if everything is up and running you can listen to the MQTT topic `hass/status`, on startup a single message is sent.

## Data Sent
The software sends out to the following MQTT topics:

```
sensors/smoke/current_value 
```

## Thanks to
Code in this sketch is greatly inspired by https://github.com/daniel-jong/esp8266_p1meter