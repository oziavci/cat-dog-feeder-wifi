// Wifi and Blynk
#define BLYNK_TEMPLATE_ID           "Your template ID"
#define BLYNK_DEVICE_NAME           "Your device name"
#define BLYNK_AUTH_TOKEN            "Your auth token"
#define BLYNK_FIRMWARE_VERSION      "0.1.0"

#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Your Wifi Name";
char pass[] = "Your Wifi Pass";

WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
int checkPeriod = 1800000; // Check Blynk connection every 30 minutes (1800000 millisecond)
unsigned long currTime = 0;
bool BlynkStatus = false;
int blynktimevalue;

// Blynk connection check
void CheckBlynk(){
  if (WiFi.status() == WL_CONNECTED) { // if wifi connected
    BlynkStatus = Blynk.connected();
    if(!BlynkStatus == 1){ // if Blynk disconnected
      Blynk.config(auth, BLYNK_DEFAULT_DOMAIN, BLYNK_DEFAULT_PORT);
      Blynk.connect(5000); // connect Blynk
    }
  } else {
    Blynk.disconnect(); // if wifi is not connected Blynk disconnect
  }
}
// //Wifi and Blynk

#include <EEPROM.h>
#define EEPROM_SIZE 512

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo motor;


long defaultTime = 21600; // default feeding time // 6 hour (21600 second)
int multiplier = 3600;
bool status = 1; // status 1 = feeding countdown mode / status 0 = time setting mode // will work in feeding mode at first (ex: countdown will start after power failure)
bool settingsactive = 0;
long feedtime = defaultTime;
long selectedTime = 0;
long second, minute, hour, eeprom_second, eeprom_minute, eeprom_hour;
bool bylnkbtn = 0;
bool restartbtn = 0;

#define pot A0
#define reset D7

// EEPROM - a read-only memory whose contents can be erased and reprogrammed using a pulsed voltage //
long eeprom_val;
long EEPROMReadlong(long address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}
void EEPROMWritelong(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}
// write the last time of the countdown to eeprom memory
void eeprom(){
    EEPROMWritelong(0, feedtime);
    EEPROM.commit();
}
// /EEPROM //


void setup()
{
  Serial.begin(115200);

  // Wifi and Blynk
  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event){});
  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event){});
  WiFi.begin(ssid, pass);
  delay(5000);
  // //Wifi and Blynk

  EEPROM.begin(512);

  lcd.init();
  lcd.backlight();
  motor.write(0);
  lcd.clear();
  pinMode(reset, INPUT_PULLUP);
  lcd.begin(16, 2);
  motor.attach(D6, 500, 2400);
  lcd.setCursor(5, 0);
  lcd.print("PET");
  lcd.setCursor(4, 1);
  lcd.print("FEEDER");
  delay(2000);
  CheckBlynk();
}
void loop()
{
  // Blynk connection check
  if(millis() >= currTime + checkPeriod){
      currTime += checkPeriod;
      CheckBlynk();
  }
  // // Blynk
  
  // if the status is 1, that is, if the feeding is in countdown mode, we run the food function
  if (status == 1) {
    if(settingsactive == 0){
      eeprom_val = EEPROMReadlong(0);
      if(int(eeprom_val) == 0 || isnan(int(eeprom_val)) || int(eeprom_val) < 0 || int(eeprom_val) > 36000){ // if eeprom data is wrong
        feed();
      } else { // if it's not wrong, get bait time from eeprom
        feedtime = eeprom_val;
        feed();
      }
    }
  } else { // if the state is 0, in the time setting mode, we get the time from the user with the potentiometer.
    Blynk.run();
    settingsactive = 1;
    lcd.clear();
    selectedTime = analogRead(pot);
    selectedTime = map(selectedTime, 0, 1023, 10, 0);
    lcd.setCursor(0, 0);
    lcd.print("Food Time:");
    lcd.setCursor(0, 1);
    lcd.print(String(selectedTime) + " hour");
    delay(1000);
    if (digitalRead(reset) == 1) { // if the button is pressed, new feed time save and return to countdown mode
      feedtime = selectedTime * multiplier;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(String(selectedTime) + " hour");
      EEPROMWritelong(0, feedtime);
      EEPROM.commit();
      delay(1500);
      status = 1;
    }
  }
}

// Blynk
BLYNK_WRITE(V1) { // feed if button pressed from Blynk app
  bylnkbtn = param.asInt();
  if (bylnkbtn == 1) {
    servo();
  }
}
BLYNK_WRITE(V5) {
  blynktimevalue = param.asInt();
}
BLYNK_WRITE(V6) {
  bylnkbtn = param.asInt();
  if (bylnkbtn == 1) {
    selectedTime = blynktimevalue;
    if(selectedTime == 0) {
      feedtime = defaultTime;
    } else {
      feedtime = selectedTime * multiplier;
    }
    EEPROMWritelong(0, feedtime);
    EEPROM.commit();
    status = 1;
  }
}
BLYNK_WRITE(V7) {
  restartbtn = param.asInt();
  if (restartbtn == 1) {
    ESP.restart();
  }
}
// //Blynk

void feed()
{
  settingsactive = 0;

  // the countdown will decrease by 1 second until the time becomes 1 and the loop will continue.
  while (feedtime > 0)
  {
    // Blynk connection check
    if(millis() >= currTime + checkPeriod){
        currTime += checkPeriod;
        CheckBlynk();
        eeprom();
    }
    // // Blynk

    second = feedtime;
    minute = second / 60;
    second = second % 60;
    hour = minute / 60;
    minute = minute % 60;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Feed time left");
    lcd.setCursor(0, 1);
    lcd.print(String(hour) + ":" + String(minute) + ":" + String(second));
    Blynk.virtualWrite(V2, String(hour) + ":" + String(minute) + ":" + String(second));

    delay(1000);

    // every 1 second we decrease the time by 1 second
    // if the time is 1, it will decrease to 0 below and go to the next step
    feedtime--;

    // when the time is 0, the countdown will be over and the following code will work
    if (feedtime == 0)
    {
      servo();

      // we set the time to start the loop again
      if (selectedTime == 0) { // If the user has not specified a time, we get the default time
        feedtime = defaultTime;
      } else { // if the user has specified a time, we equate the time to the time selected by the user
        feedtime = selectedTime * multiplier;
      }
      EEPROMWritelong(0, feedtime);
      EEPROM.commit();
    }

    // if the button is pressed while the countdown is active, we bring the status to 0, that is, the time setting mode and remove it from the countdown.
    if (digitalRead(reset) == 1) {
      status = 0;
      break;
    }
  }
}
void servo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enjoy your meal");
  motor.write(180);
  delay(2000);
  motor.write(0);
  delay(2000);
  Blynk.logEvent("feed_was_given", "The cat is happy!"); // send notification to Blynk app
}