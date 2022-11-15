// ###################################################################################################################################################
// #
// # Code for "Analog LED ring clock" project: https://www.printables.com/de/model/226641-analog-led-ring-clock
// #
// # Code by https://github.com/AWSW-de 
// #
// # Released under license: GNU General Public License v3.0: https://github.com/AWSW-de/Analog-LED-ring-clock/blob/main/LICENSE
// #
// ###################################################################################################################################################

// ###################################################################################################################################################
// #
// # Code version:
// #
// ###################################################################################################################################################
#define CODEVERSION "V1.0.2"

//####################################################################################################################################################

// Adjust program settings here:
// #############################
#define BRIGHTNESS      255 // Brightness between 0 - 255
#define HOURTICKS       15  // Show a white pixel for the hours every 15 Pixel at 3, 6, 9 and 12. Set to 5 for a marker at each hour
#define HOURTICKSACTIVE 0   // Show the hour ticks = 1 or do not show them = 0
#define RESETWIFI       0   // To DELETE the WiFi settings set to "1" and upload the sketch to the ESP. Set to "0" again and upload once more. 
                            // Then use the temporary Wifi "LED RING CLOCK" again with "192.168.4.1" to set the Wifi credentials new.
                            
//####################################################################################################################################################

// Adjust time settings here:
// ##########################

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

const int timeZone = +2;    // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

//####################################################################################################################################################

// LED ring settings:
// ###################

#define PIN           D2  // D1 mini LED output pin
#define NUMPIXELS     60  // Number of pixels on the ring

//####################################################################################################################################################

// Libraries:
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <TimeLib.h>                // https://playground.arduino.cc/Code/Time/
#include <WiFiManager.h>            // https://github.com/tzapu/WiFiManager
#include <Adafruit_NeoPixel.h>      // https://github.com/adafruit/Adafruit_NeoPixel

//####################################################################################################################################################

// Declarations:
byte hourval, minuteval, secondval;
byte pixelColorRed, pixelColorGreen, pixelColorBlue;
time_t getNtpTime();
time_t prevDisplay = 0; // when the digital clock was displayed
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

//####################################################################################################################################################

// Wifi Manager:
WiFiServer server(80);    // Set web server port number to 80
String header;            // Variable to store the HTTP request

//####################################################################################################################################################

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.show();
  pixels.setBrightness(BRIGHTNESS);
  delay(1000);
  Serial.println(" ");
  Serial.println("#############################");
  Serial.print("# AWSW LED ring clock ");
  Serial.print(CODEVERSION);
  Serial.println(" #");
  Serial.println("#############################");
  Serial.println(" ");

  // LED test:
  colorWipe(pixels.Color(255, 0, 0), 25); // Red
  colorWipe(pixels.Color(0, 255, 0), 25); // Green
  colorWipe(pixels.Color(0, 0, 255), 25); // Blue
  colorWipe(pixels.Color(0, 0, 0), 25);   // Off
  // Set hour LED ticks to white:
  if (HOURTICKSACTIVE == 1) {
    for (int i = 0; i < pixels.numPixels(); i = i + HOURTICKS) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
      pixels.show();
      delay(50);
    }
    delay(1000);
    colorWipe(pixels.Color(0, 0, 0), 25);   // Off
  }

  Serial.println("Starting Wifi Manager");

  // WiFi Manager:
  WiFiManager wifiManager;          // Local intialization. Once its business is done, there is no need to keep it around
  if (RESETWIFI == 1) {
    wifiManager.resetSettings();    // Deletes the stored WiFi settings! Set to 0 on top of the sketch after reset again!
  }
  wifiManager.autoConnect("LED RING CLOCK");

  Serial.println("Connected to WiFi");  // if you get here you have connected to the WiFi
  server.begin();
  delay(1500);

  Serial.print("IP address assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  delay(500);
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

//####################################################################################################################################################

void loop() {
  // WiFi Manager Start:
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP8266 Web Server</h1>");
            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
  // WiFi Manager End


  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
      secondval = second();
      minuteval = minute();
      hourval = hour();

      if (hourval > 12) hourval -= 12; // This clock is 12 hour, if 13-23, convert to 0-11

      hourval = (hourval * 5) - 1; // + minuteval) / 12;
      minuteval = minuteval - 1;   // 1;
      secondval = secondval - 1;   // 1;


      for (uint8_t i = 0; i < pixels.numPixels(); i++) {
        if (i <= minuteval) {
          pixelColorRed = 150;
        }
        else {
          pixelColorRed = 0;
        }

        if (i <= hourval) {
          pixelColorGreen = 150;
        }
        else {
          pixelColorGreen = 0;
        }

        pixels.setPixelColor(i, pixels.Color(pixelColorRed, pixelColorGreen, pixelColorBlue));
        // Set hour LED ticks to white:
        if (HOURTICKSACTIVE == 1) {
          for (int i = 0; i < pixels.numPixels(); i = i + HOURTICKS) {
            pixels.setPixelColor(i, pixels.Color(255, 255, 255));
          }
        }
      }
    }
  }
  pixels.show();
  delay(100);
}

//####################################################################################################################################################

void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, c);
    pixels.show();
    delay(wait);
  }
}

//####################################################################################################################################################

void digitalClockDisplay() {
  // Serial output of digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

//####################################################################################################################################################

void printDigits(int digits) {
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

//####################################################################################################################################################

// NTP code:
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

//####################################################################################################################################################

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

//####################################################################################################################################################

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

//####################################################################################################################################################
