// Wifi ve Blynk
#define BLYNK_TEMPLATE_ID           "Template ID'nizi yazın"
#define BLYNK_DEVICE_NAME           "Device Name bilgisini yazın"
#define BLYNK_AUTH_TOKEN            "Auth Token bilgisi yazın"
#define BLYNK_FIRMWARE_VERSION      "0.1.0"

#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Bağlanılacak wifi adı";
char pass[] = "Bağlanılacak wifi şifresi";

WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
int checkPeriod = 1800000; // Blynk bağlantısının kontrol edileceği zaman aralığı // 30 dakika (1800000 milisaniye)
unsigned long currTime = 0;
bool BlynkStatus = false;
int blynktimevalue;

// Blynk bağlantı kontrolü
void CheckBlynk(){
  if (WiFi.status() == WL_CONNECTED) { // Wifi bağlı ise
    BlynkStatus = Blynk.connected();
    if(!BlynkStatus == 1){ // Blynk bağlı değil ise
      Blynk.config(auth, BLYNK_DEFAULT_DOMAIN, BLYNK_DEFAULT_PORT);
      Blynk.connect(5000); // Blynk'e bağlan
    }
  } else {
    Blynk.disconnect(); // Wifi bağlı değil ise Blynk bağlantısını da kes
  }
}
// //Wifi ve Blynk

#include <EEPROM.h>
#define EEPROM_SIZE 512

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo motor;


long defaultTime = 21600; // Varsayılan mama verme saati // 6 saat (21600 saniye)
int multiplier = 3600;
bool status = 1; // status 0 = Süre ayarlama modu // status 1 = Mama verme geri sayım modu // ilk başta mama verme modunda çalışacak (örn: elektrik gidip gelmesi durumunda geri sayımdan başlayacak)
bool settingsactive = 0;
long feedtime = defaultTime;
long selectedTime = 0;
long second, minute, hour, eeprom_second, eeprom_minute, eeprom_hour;
bool bylnkbtn = 0;
bool restartbtn = 0;

#define pot A0
#define reset D7

// EEPROM; elektrikle silinebilir programlanabilir salt okunur bellek.
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

// Geri sayımdaki son süreyi eeprom'a yazıyoruz
void eeprom(){
    EEPROMWritelong(0, feedtime);
    EEPROM.commit();
}
// /EEPROM //

void setup()
{
  Serial.begin(115200);

  // Wifi ve Blynk
  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event){});
  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event){});
  WiFi.begin(ssid, pass);
  delay(5000);
  // //Wifi ve Blynk

  EEPROM.begin(512);

  lcd.init();
  lcd.backlight();
  motor.write(0);
  lcd.clear();
  pinMode(reset, INPUT_PULLUP);
  lcd.begin(16, 2);
  motor.attach(D6, 500, 2400);
  lcd.setCursor(5, 0);
  lcd.print("ZEYTiN");
  lcd.setCursor(4, 1);
  lcd.print("DOYURUCU");
  delay(2000);
  CheckBlynk();
}
void loop()
{
  // Blynk
  if(millis() >= currTime + checkPeriod){
      currTime += checkPeriod;
      CheckBlynk();
  }
  // // Blynk
  

  // Eğer status 1 ise yani mama verme geri sayım modundaysa mama fonksiyonunu çalıştırıyoruz
  if (status == 1) {
    if(settingsactive == 0){
      eeprom_val = EEPROMReadlong(0);
      if(int(eeprom_val) == 0 || isnan(int(eeprom_val)) || int(eeprom_val) < 0 || int(eeprom_val) > 36000){ // eğer eeprom'daki veri bozuk ise varsayılan süre ile geri sayımı başlat
        feed();
      } else { // bozuk değil ise geçici bellekteki son süreden geri sayıma devam et
        feedtime = eeprom_val;
        feed();
      }
    }
  } else { // Eğer status 0 yani süre ayarlama modunda ise kullanıcıdan potansiyometre ile süreyi alıyoruz.
    Blynk.run();
    settingsactive = 1;
    lcd.clear();
    selectedTime = analogRead(pot);
    selectedTime = map(selectedTime, 0, 1023, 10, 0);
    lcd.setCursor(0, 0);
    lcd.print("Mama Araligi:");
    lcd.setCursor(0, 1);
    lcd.print(String(selectedTime) + " saat");
    delay(1000);
    if (digitalRead(reset) == 1) { // butona basıldığında ayarlanan saati kaydet ve geri sayım moduna dön
      feedtime = selectedTime * multiplier;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(String(selectedTime) + " Saat Ayarlandi");
      EEPROMWritelong(0, feedtime);
      EEPROM.commit();
      delay(1500);
      status = 1;
    }
  }
}

// Blynk
BLYNK_WRITE(V1) { // Blynk uygulamasından butona basıldığında motoru çalıştırarak mama ver.
  bylnkbtn = param.asInt();
  if (bylnkbtn == 1) {
    servo();
  }
}
BLYNK_WRITE(V5) {
  blynktimevalue = param.asInt(); // Blynk uygulamasından seçilen süreyi al
}
BLYNK_WRITE(V6) {
  bylnkbtn = param.asInt(); // Blynk uygulamasından seçilen süreyi kaydet ve geri sayım için ayarla
  if (bylnkbtn == 1) {
    selectedTime = blynktimevalue;
    if(selectedTime == 0) { // Eğer uygulamadan 0 gelirse varsayılan mama verme saatini ayarla
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

  // Geri sayım, süre 1 olana kadar 1'er saniye eksilip döngü devam edecek.
  while (feedtime > 0)
  {
    // Belirlediğimiz periyotta Blynk bağlantısını kontrol et ve eeprom'a süreyi yaz
    if(millis() >= currTime + checkPeriod){
        currTime += checkPeriod;
        CheckBlynk();
        eeprom();
    }
    // // Blynk

    // Ekrana yazdırmak için saniyeyi "saat:dakika:saniye" cinsine çeviriyoruz.
    second = feedtime;
    minute = second / 60;
    second = second % 60;
    hour = minute / 60;
    minute = minute % 60;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Mama vaktine");
    lcd.setCursor(0, 1);
    lcd.print(String(hour) + ":" + String(minute) + ":" + String(second) + " var");
    Blynk.virtualWrite(V2, String(hour) + ":" + String(minute) + ":" + String(second));

    delay(1000);
    // Her 1 saniyede süreyi 1 saniye eksiltiyoruz
    // Eğer süre 1 ise aşağıda eksilerek 0 olacak ve sonraki adıma geçecek
    feedtime--;
    // Süre 0 olduğunda geri sayım bitmiş olacak ve aşağıdaki kod çalışacak
    if (feedtime == 0)
    {
      // Motoru çalıştırarak yem verme işlemini yapıyoruz
      servo();

      /// Döngüyü tekrar başlatmak için süreyi ayarlıyoruz
      // değer en başta tanımladığımız gibi 0 ise yani kullanıcı potansiyometre ile yeni süre belirtmediyse süreyi varsayılan süreye eşitliyoruz
      if (selectedTime == 0) {
        feedtime = defaultTime;
      } else {
        // Kullanıcı süre belirtmişse süreyi kullanıcının seçtiği süreye eşitliyoruz
        feedtime = selectedTime * multiplier;
      }
      EEPROMWritelong(0, feedtime);
      EEPROM.commit();
    }

    // Geri sayım aktif iken butona basılırsa status 0 yani süre ayarlama moduna getiriyoruz ve geri sayımdan çıkarıyoruz
    if (digitalRead(reset) == 1) {
      status = 0;
      break;
    }
  }
}
void servo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Afiyet olsun");
  lcd.setCursor(0, 1);
  lcd.print("Zeytin haniiim");
  motor.write(180);
  delay(2000);
  motor.write(0);
  delay(2000);
  Blynk.logEvent("yem_verildi", "Şişko zeytin mutlu!");
}