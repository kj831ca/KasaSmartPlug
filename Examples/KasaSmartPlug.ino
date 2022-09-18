#include <WiFi.h>
#include "KasaSmartPlug.h"

const char *ssid = "WIFI SSID";
const char *password = "WIFI PASSWORD";

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
  KASASmartPlug *testPlug = kasaUtil.GetSmartPlug("Test1");
  // KASAPlug *testPlug = new KASAPlug("My PLug","192.168.0.212");
  if (testPlug != NULL)
  {
    Serial.printf("\r\nTurn On the plug");
    testPlug->SetRelayState(1);
    testPlug->UpdateInfo();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    Serial.printf("\r\nTurn Off the plug");
    testPlug->SetRelayState(0);
    testPlug->UpdateInfo();
  }
}

void loop()
{
  // put your main code here, to run repeatedly:
}
