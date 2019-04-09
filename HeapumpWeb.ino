/*YO5PTD Samsung/immergas heat pump monitor through web app with wemos d1 mini and max485*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>

#define MAX485_RE_NEG   4 //D2 RS485 has a enable/disable pin to transmit or receive data. Arduino Digital Pin 2 = Rx/Tx 'Enable'; High to Transmit, Low to Receive
#define Slave_ID        1
#define RX_PIN          5 //D1 
#define TX_PIN          2  //D4 


#ifndef STASSID
#define STASSID "yourWifiRouter"
#define STAPSK  "wifiPasswd"
#endif

byte message[56];         // a String to hold incoming data
bool buffComplete = false;  // whether the string is complete
int buffpoint=0;

  int outTemp=0;
  int dischargeTemp=0;
  int fanSpeed=0;
  int condOutTemp=0;
  int current=0;
  int inSetTemp=0;
  int roomTemp=0;

  SoftwareSerial swSer(RX_PIN, TX_PIN, false, 128);
  
const char *ssid = STASSID;
const char *password = STAPSK;

byte temps[300];
const byte offset=55;
byte tempPointer=0;
const unsigned long fiveMinutes = 5 * 60 * 1000UL;
static unsigned long lastSampleTime = 0 - fiveMinutes;  // initialize such that a reading is due the first time through loop()


ESP8266WebServer server(80);

const int led = 13;

void handleRoot() {
  digitalWrite(led, 1);
  char temp[400];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 400,
           "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Samsung heat pump web interface</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>outTemp:%02doC discTemp:%02doC fanSpeed:%02dRPM inSetTemp:%02doC roomTemp:%02doC current:%02d00mA</p>\
    <img src=\"/test.svg\" />\
  </body>\
</html>",
           hr, min % 60, sec % 60, outTemp, dischargeTemp, fanSpeed, inSetTemp, roomTemp, current
          );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/test.svg", drawGraph);
  server.on("/inline", []() {
    server.send(200, "text/plain", "YO5PTD Samsung/immergas heat pump monitor through web app with wemos d1 mini and max485.");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
  
  // initialize serial:
  pinMode(MAX485_RE_NEG, OUTPUT);
  // Init in receive mode
  digitalWrite(MAX485_RE_NEG, LOW);
  //we use digital pins for a software serial port - supposed to work well up unti 115200 bps
  swSer.begin(9600);
}

void loop(void) {
  serialEvent();
  if (millis() - lastSampleTime >= fiveMinutes)
  {
    lastSampleTime += fiveMinutes;
    temps[++tempPointer]=roomTemp+offset;
    if (tempPointer>fiveMinutes*60*24) tempPointer=0;
  }
  server.handleClient();
  MDNS.update();

}

void drawGraph() {

  String out = "";
  char temp[100];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"1450\" height=\"150\">\n";
  out += "<rect width=\"1500\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  out += "<line x1=\"0\" y1=\"80\" x2=\"1455\" y2=\"80\" style=\"stroke:rgb(255,0,0);stroke-width:1\" />";
  int y = temps[0]-offset;
  for (int x = 5; x < 1445; x += 5) {
    int y2 = temps[x/5-1]-offset;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 80 - y*2, x + 5, 80 - y2*2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send(200, "image/svg+xml", out);
}

void debugmess()
{
  for(byte i=0;i<14;i++)
      {
        if (message[i]<16) Serial.print(0);
        Serial.print(message[i],HEX);
        Serial.print(" ");
      }
  Serial.println();
}

void process()
{
  if ((message[0]==0x32) && (message[13]==0x34))
  {
    byte checksum=0;
    for(int i=1;i<12;i++)
      checksum^=message[i];
    if (checksum==message[12])
    {
      switch (message[3]){
      case 0xC0:
        debugmess();
        outTemp=(int)message[8]-offset;
        dischargeTemp=(int)message[10]-offset;//bit 48..55
        break;
      case 0xC3:
        debugmess();
        fanSpeed=(int)message[7]*10;//bit 24..31
        break;
      case 0x20:
        debugmess();
        inSetTemp=(int)(message[4])-offset;//bit 1..7 (bit0 isFahrenHeit)
        roomTemp=(int)message[5]-offset; //bit 8..15
        break;
      case 0xF3:
        debugmess();
        current=(int)message[8];//bit 32..39
        break;
      }
    }      else Serial.println("checksum fail");
  }
}

void debugTemps()
{
    if (buffpoint==12) {
    Serial.print("outTemp:");
    Serial.print(outTemp);
    Serial.print(" discTemp");
    Serial.print(dischargeTemp);
    Serial.print(" fanSpeed:");
    Serial.print(fanSpeed);
    Serial.print(" inSetTemp:");
    Serial.print(inSetTemp);
    Serial.print(" roomTemp:");
    Serial.print(roomTemp);
    Serial.print(" current:");
    Serial.println(current);
  }
}

/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/
void serialEvent() {
  while (swSer.available()) {
    // get the new byte:
    char inChar = (char)swSer.read();
    // add it to the inputString:
    message[buffpoint]=inChar;
    buffpoint++;

    if((buffpoint>14)||(inChar==0x34))
    {
      buffpoint=0;
      process();
    }
  }
}
