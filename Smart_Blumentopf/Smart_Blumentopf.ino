#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

const char *ssid = "Hans";
const char *password = "HansPeter";

//Uhr Intitalisieren
const long utcOffsetInSeconds = 3600;
char daysOfTheWeek[7][12] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
// Definiere NTP Client um Zeit zu holen
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

short aktMinute;
short aktStunde;
short aktSekunde;
short aktTag;
short aktMonat;
String aktWochenTag;

//Ende Uhr Intitalisieren

//Webserver starten
ESP8266WebServer server(80);

float LDRWert = 0;

float temperatureC = 0;
short tempOffset =0;
short Wunschtemperatur=18;

float FSensorWert = 0;
short FSensorOffset =600;
int TempRead;
float voltage;

bool automatischesLicht = true;
bool TasterLichtAn=true;

String bewaesserungUhrzeit = "12";
String bewaesserungsdauer = "5";
int LetztePumpZeit;
int sensorInterval=0;


String bewaesserungstyp = "Zeitgesteuert"; // Neuer Wert: Bewaesserungstyp
String derwasserstand = "Genug";

//PinkeLEDS initalisieren
#define PINPinkLEDs D6
#define PINKLEDsMenge 10
Adafruit_NeoPixel PINKLEDs(PINKLEDsMenge, PINPinkLEDs, NEO_GRB + NEO_KHZ800);
bool PinkStart =true;
bool PinkLichtAn =true;
short WarnungLEDDelay = 250;
//ENDE PinkeLEDS initalisieren

//Taster initalisieren
#define Taster D8
//Ende Taster initalisieren

//Taster initalisieren
#define Wasserstand D0
//Ende Taster initalisieren

//PUMPE initalisieren
#define PUMPE D7
//ENDEPUMPE initalisieren

//AnalogSignale initalisieren
#define AnalogIN A0
#define AnalogSwitchLDR D1
#define AnalogSwitchFeuchtS D2
#define AnalogSwitchTemp D5
//Ende initalisieren

void setup() {
  Serial.begin(115200);

  // Verbindung zum WiFi herstellen
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Verbindung zum WiFi herstellen...");
  }
  Serial.println("Verbunden mit dem WiFi");

  // IP-Adresse auf der seriellen Konsole ausgeben
  Serial.println("IP-Adresse: " + WiFi.localIP().toString());

  // Routen für die Webseite einrichten
  server.on("/", HTTP_GET, handleRoot);
  server.on("/submit", HTTP_POST, handleFormSubmit);
  server.on("/lichtAktivierenDeaktivieren", HTTP_GET, handleLichtAktivierenDeaktivieren);
  server.on("/bewaesserungstyp", HTTP_GET, handleBewaesserungstyp);
  server.on("/pumpeTesten", HTTP_GET, handlePumpeTesten);
  server.on("/lichtTesten", HTTP_GET, handleLichtTesten);
  server.on("/sensors", HTTP_GET, handleSensors);

  // Starten des Servers
  server.begin();

  //Starte Zeit
  timeClient.begin();

  //PinkeLEDS starten
  PINKLEDs.begin();
  //ENDE PinkeLEDS starten

  //Taster stellen
  pinMode(Taster, INPUT);
  attachInterrupt(digitalPinToInterrupt(Taster), Tastergedrueckt, RISING);
  //ENDE Taster stellen

  //Wasserstand stellen
  pinMode(Wasserstand, INPUT);
  digitalWrite(Wasserstand, LOW);  
  //ENDE Wasserstand stellen

  //PUMPE stellen
  pinMode(PUMPE, OUTPUT);
  digitalWrite(PUMPE, LOW);
  //ENDE PUMPE stellen

  //Analog starten
  pinMode(AnalogIN, INPUT);
  pinMode(AnalogSwitchFeuchtS, OUTPUT);
  pinMode(AnalogSwitchLDR, OUTPUT);
  pinMode(AnalogSwitchTemp, OUTPUT);
  //ENDE Analog starten
}

void loop() {
  //Webpage aktualisieren
  server.handleClient();

  //Zeit aktualisieren
  timeClient.update();
  ZeitSammeln();

  PINKLEDsHandel();
  PumpeHandle();
  if(sensorInterval+4<aktMinute){//folgendes alle 5 Minuten
    sensorInterval=aktMinute;
    handleSensors2();
    HandleWarnlichter();
   }
}

//PageLayout
void handleRoot() {
  String html = "<html><head>";
  html += "<style>"
          "body { font-family: 'Arial', sans-serif; margin: 20px; background-color: #f4f4f4; }"
          "h1 { color: #333; }"
          "form { max-width: 400px; margin: 0 auto; background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }"
          "input { width: 100%; padding: 10px; margin-bottom: 10px; box-sizing: border-box; }"
          "input[type='submit'] { background-color: #4caf50; color: white; cursor: pointer; }"
          "button { background-color: #008CBA; color: white; padding: 10px; margin: 5px 0; border: none; border-radius: 4px; cursor: pointer; }"
          "#result { margin-top: 20px; }"
          "#sensorValues { margin-top: 20px; }"
          "button, #sensorValues button { width: 100%; }"
          "</style>";

  html += "<script>"
          "function submitForm() {"
          "  var xhr = new XMLHttpRequest();"
          "  var formData = new FormData(document.getElementById('myForm'));"
          "  xhr.open('POST', '/submit', true);"
          "  xhr.onload = function () {"
          "    if (xhr.status === 200) {"
          "      document.getElementById('result').innerHTML = xhr.responseText;"
          "      updateSensorValues();"
          "    }"
          "  };"
          "  xhr.send(formData);"
          "  return false;"
          "}"
          "function callFunction(functionNumber) {"
          "  var xhr = new XMLHttpRequest();"
          "  xhr.open('GET', '/' + functionNumber, true);"
          "  xhr.onload = function () {"
          "    if (xhr.status === 200) {"
          "      document.getElementById('result').innerHTML = xhr.responseText;"
          "      updateSensorValues();"
          "    }"
          "  };"
          "  xhr.send();"
          "  return false;"
          "}"
                  "function updateSensorValues() {"
          "  var xhr = new XMLHttpRequest();"
          "  xhr.open('GET', '/sensors', true);"
          "  xhr.onload = function () {"
          "    if (xhr.status === 200) {"
          "      var sensorValues = JSON.parse(xhr.responseText);"
          "      document.getElementById('lichtstaerke').innerHTML = 'Lichtstaerke (1-5): ' + sensorValues.lichtstaerke + ' von 5';"
          "      document.getElementById('temperatur').innerHTML = 'Temperatur: ' + sensorValues.temperatur + 'C';"
          "      document.getElementById('erdfeuchtigkeit').innerHTML = 'Erdfeuchtigkeit (1-5): ' + sensorValues.erdfeuchtigkeit + ' von 5';"
          "      document.getElementById('automatischesLicht').innerHTML = 'Automatisches Licht: ' + (sensorValues.automatischesLicht ? 'Aktiviert' : 'Deaktiviert');"
          "      document.getElementById('bewaesserungUhrzeit').innerHTML = 'Bewaesserungsuhrzeit: ' + sensorValues.bewaesserungUhrzeit;"
          "      document.getElementById('bewaesserungsdauer').innerHTML = 'Bewaesserungsdauer: ' + sensorValues.bewaesserungsdauer + ' Sekunden';"
          "      document.getElementById('bewaesserungstyp').innerHTML = 'Bewaesserungstyp: ' + sensorValues.bewaesserungstyp;" // Wasserstand hinzugefügt
          "      document.getElementById('wasserstand').innerHTML = 'Wasserstand: ' + sensorValues.wasserstand;" // Wasserstand hinzugefügt
          "    }"
          "  };"
          "  xhr.send();"
          "}"
          "</script></head><body>";
  html += "<h1>Automatische Pflanzenversorgung</h1>";
  html += "<form id='myForm' onsubmit='return submitForm();'>";
  html += "<form id='myForm' onsubmit='return submitForm();'>";
  html += "Giessuhrzeit in Stunden (bsp. 22Uhr = 22): <input type='text' name='giessuhrzeit'><br>";
  html += "Giessdauer in Sekunden (ca. 25ml Wasser pro Sekunde): <input type='text' name='giessdauer'><br>";
  html += "<input type='submit' value='Absenden'>";
  html += "</form>";
  html += "<div id='result'></div>";

  // Schaltknöpfe für verschiedene Funktionen
  html += "<button onclick='callFunction(\"lichtAktivierenDeaktivieren\"); return false;'>Licht aktivieren/deaktivieren</button>";
  html += "<button onclick='callFunction(\"bewaesserungstyp\"); return false;'>Bewaesserungstyp</button>";
  html += "<button onclick='callFunction(\"pumpeTesten\"); return false;'>Pumpe Testen</button>";
  html += "<button onclick='callFunction(\"lichtTesten\"); return false;'>Pinkes & Warnlicht Testen</button>";

  // Div für Sensordaten
  html += "<div id='sensorValues'>";
  html += "<h2>Sensordaten & Kofiguration</h2>";
  html += "<div id='lichtstaerke'></div>";
  html += "<div id='temperatur'></div>";
  html += "<div id='erdfeuchtigkeit'></div>";
  html += "<div id='automatischesLicht'></div>";
  html += "<div id='bewaesserungUhrzeit'></div>";
  html += "<div id='bewaesserungsdauer'></div>";
  html += "<div id='bewaesserungstyp'></div>";  // Neues HTML-Element für Bewaesserungstyp
  html += "<div id='wasserstand'></div>";       // Neues HTML-Element für Wasserstand
  html += "<button onclick='updateSensorValues(); return false;'>Aktualisiere Sensordaten & Zeige Warnungen</button>";
  html += "</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}
//Giesseingabe Handle
void handleFormSubmit() {
  // Verarbeitung der Formulardaten nach dem Absenden
  String giessuhrzeit = server.arg("giessuhrzeit");
  bewaesserungUhrzeit=giessuhrzeit;
  String giessdauer = server.arg("giessdauer");
  bewaesserungsdauer = giessdauer;

  Serial.println("Gießuhrzeit: " + giessuhrzeit);
  Serial.println("Gießdauer: " + giessdauer);

  // Hier können Sie weitere Aktionen mit den Formulardaten durchführen

  // Antwort an den Client senden
  server.send(200, "text/plain", "Daten erhalten!");
}
void handleLichtAktivierenDeaktivieren() {
  // Hier Code für Licht aktivieren/deaktivieren einfügen
  if(automatischesLicht == true){
    automatischesLicht = false;
  }
  else{
    automatischesLicht = true;
    }

  if(automatischesLicht == true){
    server.send(200, "text/plain", "Automatisches Licht aktiviert");
  }
  else{
    server.send(200, "text/plain", "Automatisches Licht deaktiviert");
    }
}
void handleBewaesserungstyp() {
  // Hier Code für Bewaesserungstyp einfügen
    if(bewaesserungstyp == "Feuchtigkeit"){
    bewaesserungstyp = "Zeitgesteuert";
  }
  else{
    bewaesserungstyp = "Feuchtigkeit";
    }

  if(bewaesserungstyp == "Feuchtigkeit"){
    server.send(200, "text/plain", "Pumpe nun über Feutchtigkeit gesteuert");
  }
  else{
    server.send(200, "text/plain", "Pumpe nun über Zeit gesteuert");
    }
}
void handlePumpeTesten() {
  // Hier Code für Pumpe Testen einfügen
  server.send(200, "text/plain", "Pumpe läuft 3 Sekunden");
  PUMPEANAUS();
}
void handleLichtTesten() {
  // Hier Code für Licht Testen einfügen
  server.send(200, "text/plain", "Pinkes Licht und Warnlichter Prüfung");
  LichterTest();
}
void handleSensors() {
  // Hier können Sie echte Sensorwerte abrufen und zurücksenden
  LDRLesen();
  FSensorLesen();
  TempLesen();
  WasserCheck();
  String sensorValues = "{";
  sensorValues += "\"lichtstaerke\":" + String(LDRWert) + ",";
  sensorValues += "\"temperatur\":" + String(temperatureC) + ",";
  sensorValues += "\"erdfeuchtigkeit\":" + String(FSensorWert) + ",";
  sensorValues += "\"automatischesLicht\":" + String(automatischesLicht ? "true" : "false") + ",";
  sensorValues += "\"bewaesserungUhrzeit\":\"" + String(bewaesserungUhrzeit) + "\",";
  sensorValues += "\"bewaesserungsdauer\":" + String(bewaesserungsdauer) + ",";
  sensorValues += "\"bewaesserungstyp\":\"" + bewaesserungstyp + "\","; // Neuer Wert: Bewaesserungstyp
  sensorValues += "\"wasserstand\":\"" + derwasserstand + "\""; // Wasserstand als String
  sensorValues += "}";
  
  server.send(200, "application/json", sensorValues);
  HandleWarnlichter();
}
//Interrupt Taster Funktion //Licht Aktivieren Deaktivieren
ICACHE_RAM_ATTR void Tastergedrueckt(){
  if(digitalRead(Taster)==1){
    Serial.println("INTERRUPT");
    Serial.println();
    if(TasterLichtAn==false){
      TasterLichtAn=true;
    }
    else{
      TasterLichtAn=false;
    }
  }
}
void PUMPEANAUS(){
  digitalWrite(PUMPE,HIGH);
  Serial.println("Pumpt");
  delay(3000);
  digitalWrite(PUMPE,LOW);
}
void PUMPEAUS(){
  digitalWrite(PUMPE,LOW);
  delay(1000);
}
void PUMPEAN(){
  digitalWrite(PUMPE,HIGH);
  delay(1000);
}
void LDRLesen(){
  digitalWrite(AnalogSwitchFeuchtS, LOW);
  delay(30);
  digitalWrite(AnalogSwitchLDR, HIGH);
  delay(200);
  LDRWert=analogRead(AnalogIN);
  Serial.print("Realer LDR-Wert für Offset: ");
  Serial.println(LDRWert);

  if(LDRWert<=205){
    LDRWert=1;
  }
    if(LDRWert>205 && LDRWert<=410){
    LDRWert=2;
  }
    if(LDRWert>410 && LDRWert<=615){
    LDRWert=3;
  }
    if(LDRWert>615 && LDRWert<=820){
    LDRWert=4;
  }
    if(LDRWert>820){
    LDRWert=5;
  }

  Serial.print("LDRWert: ");
  Serial.println(LDRWert);
  Serial.println();

  delay(200);
  digitalWrite(AnalogSwitchLDR, LOW);;
}
void FSensorLesen(){
  digitalWrite(AnalogSwitchLDR, LOW);
  delay(30);
  digitalWrite(AnalogSwitchFeuchtS, HIGH);
  delay(200);
  short Feuchtangepast;
  FSensorWert=analogRead(AnalogIN);
  Feuchtangepast=analogRead(AnalogIN);
  Serial.print("Realer Feuchtigkeitswert für Offset: ");
  Serial.println(FSensorWert);
  Feuchtangepast=Feuchtangepast-FSensorOffset;

  if(Feuchtangepast<0){
    Feuchtangepast=0;
  }
  if(Feuchtangepast<=85){
    Feuchtangepast=5;
  }
    if(Feuchtangepast>85 && Feuchtangepast<=170){
    Feuchtangepast=4;
  }
    if(Feuchtangepast>170 && Feuchtangepast<=255){
    Feuchtangepast=3;
  }
    if(Feuchtangepast>255 && Feuchtangepast<=350){
    Feuchtangepast=2;
  }
    if(Feuchtangepast>350){
    Feuchtangepast=1;
  }

  FSensorWert=Feuchtangepast;
  Serial.print("Feuchtigkeit: ");
  Serial.println(FSensorWert);
  Serial.println();
  delay(200);
  digitalWrite(AnalogSwitchFeuchtS, LOW);
}
void PINKLEDsAUS(){
  for(short y=0; y<PINKLEDsMenge; y++){
    PINKLEDs.setPixelColor(y,0,0,0);
  }
  PINKLEDs.show();
}
void PINKLEDsAN(){
  for(short x=0; x<PINKLEDsMenge; x++){
    PINKLEDs.setPixelColor(x,255,8,127);
  }
  PINKLEDs.show();
}
void PINKLEDSANAUS(){
  for(short x=0; x<PINKLEDsMenge; x++){
    PINKLEDs.setPixelColor(x,255,8,127);
  }
  PINKLEDs.show();
  delay(3000);
  for(short y=0; y<PINKLEDsMenge; y++){
    PINKLEDs.setPixelColor(y,0,0,0);
  }
  PINKLEDs.show();
}
void PINKLEDsHandel(){
  if(TasterLichtAn==true){
    if(automatischesLicht==true){
    digitalWrite(AnalogSwitchLDR, HIGH);
    delay(30);
    LDRWert=analogRead(AnalogIN);
    }
    if(LDRWert<615 && automatischesLicht==true){
      PINKLEDsAN();
    }
    else{
      PINKLEDsAUS();
    }
  }
  else{
    PINKLEDsAUS();}
}
void PumpeHandle(){
  if(derwasserstand == "Genug"){
    if(aktStunde==bewaesserungUhrzeit.toInt() && aktSekunde==1 && aktMinute == 0 && bewaesserungstyp=="Zeitgesteuert"){
      PINKLEDsAUS();//Schutz Überspannung
      int Wasserdauer = bewaesserungsdauer.toInt()*1000;
      digitalWrite(PUMPE,HIGH);
      delay(Wasserdauer);
      digitalWrite(PUMPE,LOW);
      delay(300);
      PINKLEDsHandel();
    }

    if(bewaesserungstyp=="Feuchtigkeit" && FSensorWert<3 && LetztePumpZeit!=aktStunde){ //1mal jede stunde check
      PINKLEDsAUS(); //Schutz Überspannung
      int Wasserdauer = bewaesserungsdauer.toInt()*1000;
      digitalWrite(PUMPE,HIGH);
      LetztePumpZeit=aktStunde;
      delay(Wasserdauer);
      digitalWrite(PUMPE,LOW);
      PINKLEDsHandel();
    }
  }
}
void TempLesen(){
  digitalWrite(AnalogSwitchFeuchtS, LOW);
  delay(30);
  digitalWrite(AnalogSwitchLDR, LOW);
  delay(30);
  digitalWrite(AnalogSwitchTemp, HIGH);
  delay(200);
  TempRead=analogRead(AnalogIN);
  voltage = TempRead * 3.3;
  voltage /= 1024.0;
  temperatureC = (voltage - 0.5) * 100 ;
  temperatureC = temperatureC-tempOffset;
  Serial.print("Temperatur: ");
  Serial.println(temperatureC);
  Serial.println();
  delay(200);
  digitalWrite(AnalogSwitchTemp, LOW);
}
void WasserCheck(){
  if(digitalRead(Wasserstand)==0){
    derwasserstand = "Warnung - Leer";
  }
  else{
    derwasserstand = "Genug";
  }
  Serial.println("Wasserstand: " + derwasserstand);
  Serial.println();
}
void ZeitSammeln(){
  aktMinute=timeClient.getMinutes();
  aktStunde=timeClient.getHours();
  aktSekunde=timeClient.getSeconds();
  aktTag=timeClient.getDay();
  aktWochenTag=daysOfTheWeek[timeClient.getDay()];
  /*
  Serial.print("Minute: ");
  Serial.println(aktMinute);
  Serial.print("Stunde: ");
  Serial.println(aktStunde);
  Serial.print("Sekunde: ");
  Serial.println(aktSekunde);
  Serial.print("TagNr: ");
  Serial.println(aktTag);
  Serial.print("Tag: ");
  Serial.println(aktWochenTag);
  Serial.println();
  */
}
void HandleWarnlichter(){
  if(derwasserstand != "Genug"||temperatureC<Wunschtemperatur||LDRWert<2||FSensorWert<2){
    if(derwasserstand != "Genug"){//Nicht genug Wasser
      for(short y=0;y<3;y++){
        for(short x=0; x<PINKLEDsMenge; x++){
        PINKLEDs.setPixelColor(x,0,0,100);
        }
        PINKLEDs.show();
        delay(300);
        PINKLEDsAUS();
        delay(300);
      }
   }
    if(temperatureC<Wunschtemperatur){
      for(short y=0;y<3;y++){
        for(short x=0; x<PINKLEDsMenge; x++){
        PINKLEDs.setPixelColor(x,100,0,0);
        }
        PINKLEDs.show();
        delay(300);
        PINKLEDsAUS();
        delay(300);
      }
   } 
    if(LDRWert<=3){
      for(short y=0;y<3;y++){
        for(short x=0; x<PINKLEDsMenge; x++){
        PINKLEDs.setPixelColor(x,100,100,100);
        }
        PINKLEDs.show();
        delay(300);
        PINKLEDsAUS();
        delay(300);
      }
   }
    if(FSensorWert<2){
      for(short y=0;y<3;y++){
        for(short x=0; x<PINKLEDsMenge; x++){
        PINKLEDs.setPixelColor(x,0,100,0);
        }
        PINKLEDs.show();
        delay(300);
        PINKLEDsAUS();
        delay(300);
      }
   }
  }
  PINKLEDsHandel();
}
void LichterTest(){
  PINKLEDsAUS();
  delay(500);
  for(short x=0; x<PINKLEDsMenge; x++){
    PINKLEDs.setPixelColor(x,255,8,127);
  }
  PINKLEDs.show();
  delay(500);
  for(short x=0; x<PINKLEDsMenge; x++){
    PINKLEDs.setPixelColor(x,100,0,0);
  }
  PINKLEDs.show();
  delay(500);
  for(short x=0; x<PINKLEDsMenge; x++){
    PINKLEDs.setPixelColor(x,100,100,100);
  }
  PINKLEDs.show();
  delay(500);
  for(short x=0; x<PINKLEDsMenge; x++){
    PINKLEDs.setPixelColor(x,0,0,100);
  }
  PINKLEDs.show();
  delay(500);
  for(short x=0; x<PINKLEDsMenge; x++){
    PINKLEDs.setPixelColor(x,0,100,0);
  }
  PINKLEDs.show();
  delay(500);
  PINKLEDsHandel();
}

void handleSensors2(){
  LDRLesen();
  FSensorLesen();
  TempLesen();
  WasserCheck();
}
