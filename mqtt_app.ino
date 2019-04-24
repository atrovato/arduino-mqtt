#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

byte mac[] = {0xFA, 0x2B, 0x4E, 0x0E, 0x27, 0xED};
IPAddress ip(192, 168, 1, 64);
IPAddress server(192, 168, 1, 63);

EthernetClient ethClient;
PubSubClient client(ethClient);

typedef String (*GeneralFunction)(int);

struct Device
{
  String deviceName;
  unsigned int pinID;
  bool analog;
  GeneralFunction computer;
  unsigned int millisToWait;
  unsigned long lastTime;
  int lastValue;
  bool initialized;
};

String currentIP = "";

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("arduinoClient", "alex", "anarchy51"))
    {
      IPAddress address = Ethernet.localIP();
      currentIP = String(address[0]) + "." +
                  String(address[1]) + "." +
                  String(address[2]) + "." +
                  String(address[3]);
      Serial.println("connected");
      delay(1000);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

bool publishMessage(const char *topic, const String message)
{
  if (!client.connected())
  {
    reconnect();
  }

  client.loop();

  char msgChar[message.length() + 1];
  message.toCharArray(msgChar, message.length() + 1);
  bool result = client.publish(topic, msgChar);
  if (!result)
  {
    Serial.print("ERROR publishing :");
    Serial.println(msgChar);
  }

  delay(20);
  return result;
}

void readAndPublish(Device &device)
{
  unsigned long currentTime = millis();
  if (!device.initialized || device.lastTime + device.millisToWait < currentTime)
  {
    int rawVal;
    if (device.analog)
    {
      rawVal = analogRead(device.pinID);
    }
    else
    {
      rawVal = digitalRead(device.pinID);
    }

    if (!device.initialized || device.lastValue != rawVal)
    {
      device.lastValue = rawVal;
      device.lastTime = currentTime;
      device.initialized = true;

      String val;
      if (device.computer != NULL)
      {
        val = device.computer(rawVal);
      }
      else
      {
        val = String(rawVal);
      }

      String message = "{\"identifier\":\"";
      message += currentIP;
      message += "\",\"service\":\"mqtt\",\"type\":\"";
      message += device.deviceName;
      message += "\",\"state\":{\"value\":";
      message += val;
      message += "}}";

      Serial.println(device.deviceName + " sending value=" + val);
      publishMessage("gladys/master/devicestate/create", message);
    }
  }
}

String computeTemperature(int defaultValue)
{
  long Resistance = 1000.0 * ((1024.0 / defaultValue) - 1);
  double temp = log(Resistance);
  temp = 1 / (0.001129148 + (0.000234125 * temp) + (0.0000000876741 * temp * temp * temp));
  temp = temp - 273.15;
  return String(temp);
}

void setup()
{
  Serial.begin(57600);

  client.setServer(server, 1883);

  Ethernet.begin(mac);
  // Allow the hardware to sort itself out
  delay(1500);
}

int nbDevices = 3;
Device devices[] = {
    {"luminence", 0, true, NULL, 5000},
    {"presence", 7, false, NULL, 0},
    {"temp", 1, true, computeTemperature, 60000}};

void loop()
{
  for (int i = 0; i < nbDevices; i++)
  {
    readAndPublish(devices[i]);
    delay(50);
  }
}