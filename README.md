# KasaSmartPlug
 Arduino Library for TP Link KASA Smart Plug for ESP32. 
 This library will allow ESP32 to scan the TP Link Kasa smart plug and Light Switch on the local network.
 You can control the TP Link Smart Plugs and Light Switches. Please make sure you ESP32 is on the same
 WIFI network as your TP Link Smart Plug devices.
 
 So far I tested the library with TP Link Smart Plug model HS103(US) and HS200(US).
 
 # Dependencie
 This library need ArduinoJson by Benoit Blanchon. 
 https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
 
 
 #Example
 Scan the TP Link Kasa Smart Plug devices and print out the device details.
 ~~~
     #include <WiFi.h>
     #include "KasaSmartPlug.h"
     
     const char *ssid = "your wifi ssid";
     const char *password = "your wifi password";
     
     KASAUtil kasaUtil;
     
     void setup()
     {
      int found;
      Serial.begin(115200);

      // connect to WiFi
      Serial.printf("Connecting to %s ", ssid);
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED)
      {
        delay(500);
        Serial.print(".");
      }
      Serial.println(" CONNECTED");

      found = kasaUtil.ScanDevices();
      Serial.printf("\r\n Found device = %d", found);

      // Print out devices name and ip address found..
      for (int i = 0; i < found; i++)
      {
        KASASmartPlug *p = kasaUtil.GetSmartPlugByIndex(i);
        if (p != NULL)
        {
          Serial.printf("\r\n %d. %s IP: %s Relay: %d", i, p->alias, p->ip_address, p->state);
        }
       } 
     }
     
 ~~~
