/*
 *  ESP8266_PIR 
 *  Motion Sensor Alarm using Adafruit HUZZAH ESP8266 and PIR Sensor
 *  Sends events to IFTTT for SMS script
 *  
 *  NOTE: Alarm delays for 60sec (default) to activate/deactivate
 *  
 *  Requires HTTPS SSL.  To get SHA1 Fingerprint:
 *  $ openssl s_client -servername maker.ifttt.com -connect maker.ifttt.com:443 | openssl x509 -fingerprint -noout
 *  (Replace colons with spaces in result)
 *  
 *  Create a IFTTT Recipe using the Maker Channel to trigger a Send SMS Action.
 *  Update the api_key from your IFTTT Maker URL
 *  
 *  HTTPSRedirect is available from GitHub at:
 *  https://github.com/electronicsguy/ESP8266/tree/master/HTTPSRedirect
 *  
 *  The ESP8266WiFi library should include WiFiClientSecure already in it
 *
 *  version 1.1 2017-02-20 R.Grokett - Added alarm activate sound control & PIR pullup
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "HTTPSRedirect.h"

extern "C" {
  #include "gpio.h"
}

extern "C" {
#include "user_interface.h" 
}

const char* ssid     = "{YOUR_WIFI_SSID}";  // Your WiFi SSID
const char* password = "{YOUR_WIFI_PWD}";   // Your WiFi Password
const char* api_key  = "{YOUR_IFTTT_API_KEY}"; // Your API KEY from https://ifttt.com/maker
 
const char* host     = "maker.ifttt.com";
const int port       = 443;
const char* event    = "piralarm";             // Your IFTTT Event Name

const char* SHA1Fingerprint="A9 81 E1 35 B3 7F 81 B9 87 9D 11 DD 48 55 43 2C 8F C3 EC 87";  // See SHA1 comment above 
bool verifyCert = false; // Select true if you want SSL certificate validation

int ALARM = 4;         // GPIO 4 (Alarm Buzzer)
int LED = 13;          // GPIO 13 (GREEN Activity LED)
int PIRpin = 14;       // GPIO 14 (PIR Sensor)
int ALARM_DELAY = 60;  // Delay in seconds when activating alarm/deactivating alarm
int MOTION_DELAY = 10 ;// Delay in seconds between events to keep from flooding IFTTT SMS
int ALARM_BEEP = 3;    // Adjusts activate/deactivate sound alert intensity (0-10)


int onboardLED = 0;    // Internal ESP red LED
int motionState = 0;   // cache for current motion state value


//--------------------------
// INITIALIZE and Connect to WiFi
void setup() {
  Serial.begin(115200);
  delay(10);

  // prepare PIR input pin
  pinMode(PIRpin, INPUT);
  digitalWrite(PIRpin, HIGH);  // turn on pullup

  // prepare activity LED
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);  

  // prepare internal LED
  pinMode(onboardLED, OUTPUT);
  digitalWrite(onboardLED, HIGH);  

  // prepare Alarm buzzer
  pinMode(ALARM, OUTPUT);
  digitalWrite(ALARM, LOW);  
  
  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int rc = connectWifi();
  if (rc == 0) {
    // Blink onboard LED to signify its connected
    blink();
    blink();
    blink();
    blink(); 
  }
  else {
    Serial.println("NO WIFI CONNECTION!");
  }

  // Turn off Wifi to save power
  powerSave();  

  // Beep the Alarm buzzer to signify its activating
  Serial.println("Activating Alarm. Leave room now!");
  activate();

  // Read PIR motion sensor initially.
  int dummy = digitalRead(PIRpin);
  
  Serial.println("Alarm now Active");
  digitalWrite(LED, HIGH);   // Alarm Active 

  // Goes to Main loop()
}


// SEND HTTPS GET TO IFTTT
void sendEvent()
{
    Serial.println("sendEvent() Triggered!");

    // Beep the Alarm buzzer to signify its activating
    activate();

    // TURN ON ALARM BUZZER!
    digitalWrite(ALARM, HIGH);

    int rc = powerUp();  // Turn on Wifi
    
    if (rc == 0) {  // SEND SMS ALARM MSG
        Serial.print("connecting to ");
        Serial.println(host);
  
        // Use WiFiClient class to create TCP connections
        // IFTTT does a 302 Redirect, so we need HTTPSRedirect 
        HTTPSRedirect client(port);
        if (!client.connect(host, port)) {
          Serial.println("connection failed");
          return;
        }

	      if (verifyCert) {
	        if (!client.verify(SHA1Fingerprint, host)) {
              Serial.println("certificate doesn't match. will not send message.");
              return;
	        } 
	      }
  
        // We now create a URI for the request
        String url = "/trigger/"+String(event)+"/with/key/"+String(api_key);
        Serial.print("Requesting URL: https://");
        Serial.println(host+url);
  
        // This will send the request to the server
        client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
        delay(10);
  
        // Read all the lines of the reply from server and print them to Serial
        while(client.available()){
          String line = client.readStringUntil('\r');
          Serial.print(line);
        }
  
        Serial.println();
        Serial.println("closing connection");

		// Turn off Wifi to save power
  		powerSave();  

    }
    // Leave Alarm buzzer on for a while
    delay(ALARM_DELAY * 1000);

    // TURN OFF ALARM BUZZER
    digitalWrite(ALARM, LOW);
    Serial.println("Alarm resetting");

    // Keep from flooding IFTTT and SMS from quick triggers
    delay(MOTION_DELAY * 1000);

    digitalWrite(LED, HIGH);   // Alarm reset back Active 
    Serial.println("Alarm is Active");

}

// Blink the LED 
void blink() {
  digitalWrite(LED, HIGH);
  digitalWrite(onboardLED, LOW);
  delay(100); 
  digitalWrite(LED, LOW);
  digitalWrite(onboardLED, HIGH);
  delay(100);
}

// Beep the Alarm Buzzer
void beep() {
  Serial.println("beep()");
  blink();
  digitalWrite(ALARM, HIGH);
  delay(ALARM_BEEP * 100);
  digitalWrite(ALARM, LOW);
  delay(ALARM_BEEP * 100);
}

// Beep the Alarm buzzer to signify its activated
void activate() {
    Serial.println("activate()");
    beep();
    // Provide time for you to deactivate before alarm!
    delay(ALARM_DELAY * 330);
    beep();
    delay(ALARM_DELAY * 330);
    beep();
    delay(ALARM_DELAY * 330);
}

// POWER DOWN WIFI TO SAVE BATTERY
void powerSave()
{
  Serial.println("going to modem sleep");
  delay(100);
  WiFi.forceSleepBegin(0);
  delay(200);
} 
// POWER UP WIFI TO TRANSMIT
int powerUp()
{
  Serial.println("wake up modem");
  WiFi.forceSleepWake();
  int rc = connectWifi();
  return(rc);
} 

// Connect to Wifi
int connectWifi() {
  Serial.println("connectWifi()");
  int rc = 0;
  int cnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (cnt > 30) {
      Serial.println("Wifi connect failed!");
      rc = 1;
      break;
    }
    Serial.print(".");
    cnt = cnt + 1;
  }
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  return(rc);
}


//--------------------------
// MAIN LOOP
void loop() {
    delay(500);

    // Read PIR motion sensor
    int newState = digitalRead(PIRpin);

    // Trigger only if state has changed from last detection
    if (newState != motionState)
    {
      motionState = newState;
      Serial.print("State changed: ");
      Serial.println(motionState);
      if (motionState == 0)   // PIR sensor is normally High (1) so zero (0) means motion detected
      {
        sendEvent();
      }
    }

}

