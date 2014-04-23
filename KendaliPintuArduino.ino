#include <SPI.h>        
#include <Ethernet.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

const char server_addr[] = "192.168.2.4";
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x0E, 0xF5, 0xF8};
byte ip[] = {
  192,168,1,73};

EthernetServer server(80);
boolean device_status = true;
boolean isInputting = false;

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = 
{{'1','2','3'},
{'4','5','6'},
{'7','8','9'},
{'*','0','#'}};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS );
LiquidCrystal_I2C lcd(0x27,20,4);
Servo myservo;

char pass[10];
int attempt = 0;
int count = 0;
String disp = "";

const int sensor = A2;
const int push_btn = A3;
const int servo = 9;

long prevTime = 0;
long idleTime = 0;
long interval_reset = 180000;
long interval_idle = 20000;

boolean disp_init = false;
boolean disp_lock = false;
boolean disp_offline = false;
boolean disp_online = false;
boolean disp_inactive = false;

void setup(){
  Serial.begin(9600);
  myservo.attach(9);

  Ethernet.begin(mac, ip);
  server.begin();

  lcd.init();
  lcd.backlight();

  pinMode(sensor, INPUT);
  digitalWrite(sensor, HIGH);
  pinMode(push_btn, INPUT);
  digitalWrite(push_btn, HIGH);

  myservo.write(70);
  Serial.println("Ready");
}

void loop(){
  EthernetClient client = server.available();
  if(client){
    while(client.connected()){
      if(client.available()){
        if(client.find("GET /")){
          while(client.findUntil("command_", "\n\r")){
            if(!disp_online){
              disp_online = true;
              disp_offline = false;
              lcd_offline(false);
            }  
            char type = client.read();
            if(type == 'o') {
              Serial.println("Open Command");
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println();
              client.print("{\"response\":\"open\"}");
              open_door();
            }
            else if(type == 's'){
              int val = client.parseInt();
              Serial.println("Status Command");
              if(val == 2){
                device_status = true;
                lcd_init();
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: application/json");
                client.println();
                client.print("{\"response\":\"activate\"}");
              }
              else if(val == 0){
                device_status = false;
                Serial.println("Deactivate");
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: application/json");
                client.println();
                client.print("{\"response\":\"deactivate\"}");
              }
              else{
                Serial.println("Unexpected command");
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: application/json");
                client.println();
                client.print("{\"response\":\"invalid_status\"}");
              }
            }
            else if(type == 'a') {
              Serial.println("Pinging");
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println();
              client.print("{\"response\":\"online\"}");
            }
            else if(type == 'c') {
              Serial.println("Check Status");
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println();
              if(device_status){
                client.print("{\"response\":\"active\"}");
              }
              else{
                client.print("{\"response\":\"inactive\"}");
              }
            }
            else if(type == 'f') {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println();
              client.print("{\"response\":\"false\"}");
              wrong_password();
            }
            else {
              Serial.print("Unexpected type ");
              Serial.print(type);
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println();
              client.print("{\"response\":\"invalid\"}");
            }
          }
        }
        break;
      }
    }
    delay(10);
    client.stop();
  }

  if(digitalRead(push_btn) == LOW){
    auth_user("keluar");
    open_door();
  }

  if(device_status){
    if(!device_isLock()){
      char key = keypad.getKey();
      if (key != NO_KEY && key != '*' && key != '#'){
        idleTime = millis();
        isInputting = true;
        disp += "*";
        lcd.setCursor(0,2);
        lcd.print(disp);
        pass[count] = key;
        count++;
      }
      if(key == '*'){
        idleTime = millis();
        isInputting = false;
        attempt++;
        auth_user(pass);
        sys_init();
      }
      if(key == '#'){
        idleTime = millis();
        sys_init();
      }
      if(count > 9){
        auth_user(pass);
        sys_init();
      }
    }
  }
  else{
    if(!disp_inactive){
      disp_inactive = true;
      lcd_print("Perangkat Non-Aktif");
    }
  }

  unsigned long curTime = millis();
  if(curTime - idleTime > interval_idle){
    idleTime = curTime;
    isInputting = false;
    sys_init();
  }

  if(!isInputting){
    if(curTime - prevTime > interval_reset){
      prevTime = curTime;
      attempt = 0;
      lcd.setCursor(0,3);
      for(int i=0;i<20;i++){
        lcd.print(" ");
      }
    }
  }
}

void sys_init(){
  Serial.println("sys_init()");
  Serial.println(pass);
  memset(pass, 0, 10);
  count = 0;
  disp = "";
  lcd.setCursor(0,2);
  for(int i=0;i<20;i++){
    lcd.print(" ");
  }
  Serial.println(pass);
}

void auth_user(char user_input[]){
  idleTime = millis();
  EthernetClient client;
  Serial.println(user_input);
  if (client.connect(server_addr,80))
  {
    client.print("GET /api/arduino/auth_user/");
    client.print(user_input);
    client.print(" HTTP/1.1\n");
    client.print("Host: 192.168.2.4\n");
    client.print("Connection: close\n\n");
    Serial.println("Sending success");
    if(!disp_online){
      disp_online = true;
      disp_offline = false;
      lcd_offline(false);
    }
  }
  else{
    if(!disp_offline){
      disp_online = false;
      disp_offline = true;
      lcd_offline(true);
    }
    Serial.println("Sending failed");
    String pass(user_input);
    if(pass != "keluar"){
      if(pass == "123"){
        open_door();
      }
      else{
        wrong_password();
      }
    }
  }
}

void open_door(){
  lcd_print("Pintu Terbuka");
  attempt = 0;
  myservo.write(30);
  delay(3000);
  while(digitalRead(sensor) == HIGH);
  delay(1000);
  myservo.write(70);
  lcd_init();
  idleTime = millis();
}

boolean device_isLock(){
  if(attempt >= 3){
    if(!disp_lock){
      lcd_print("Perangkat Terkunci");
      lcd.setCursor(0,3);
      lcd.print("Tunggu 30 detik");
      disp_lock = true;
      disp_init = false;
      disp_inactive = false;
    }
    return true;
  }
  else{
    if(!disp_init){
      lcd_init();
      disp_init = true;
      disp_lock = false;
      disp_inactive = false;
    }
    return false;
  }
}

void wrong_password(){
  if(!disp_lock){
    lcd_print("Password Salah");
    lcd_attempts();
  }
  Serial.println("Wrong Password");
  delay(1000);
  idleTime = millis();
}

void lcd_init(){
  lcd.setCursor(0,1);
  for(int i=0;i<20;i++){
    lcd.print(" ");
  }
  lcd.setCursor(0,2);
  for(int i=0;i<20;i++){
    lcd.print(" ");
  }
  lcd.setCursor(3,0);
  lcd.print("Kendali Pintu");
  lcd.setCursor(0,1);
  lcd.print("Kata Kunci :");
  lcd.setCursor(0,2);
  for(int i=0;i<20;i++){
    lcd.print(" ");
  }
}

void lcd_print(String s){
  disp_init = false;
  disp_lock = false;
  lcd.setCursor(0,1);
  for(int i=0;i<20;i++){
    lcd.print(" ");
  }
  lcd.setCursor(0,2);
  for(int i=0;i<20;i++){
    lcd.print(" ");
  }
  lcd.setCursor(0,3);
  for(int i=0;i<20;i++){
    lcd.print(" ");
  }
  lcd.setCursor(0,1);
  lcd.print(s);
}

void lcd_attempts(){
  lcd.setCursor(0,3);
  lcd.print("Percobaan :");
  lcd.setCursor(13,3);
  lcd.print(attempt);
  lcd.setCursor(15,3);
  lcd.print("/3");
}

void lcd_offline(boolean b){
  if(b){
    lcd.setCursor(19,0);
    lcd.print("*");
  }
  else{
    lcd.setCursor(19,0);
    lcd.print(" ");
  }
}





