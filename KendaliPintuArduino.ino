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
char keys[ROWS][COLS] = {
  {
    '1','2','3'    }
  ,
  {
    '4','5','6'    }
  ,
  {
    '7','8','9'    }
  ,
  {
    '*','0','#'    }
};

byte rowPins[ROWS] = {
  2, 3, 4, 5};
byte colPins[COLS] = {
  6, 7, 8};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS );
LiquidCrystal_I2C lcd(0x27,20,4);
Servo myservo;

char pass[10];
int attempt = 0;
int count = 0;

const int sensor = A2;
const int push_btn = A3;
const int servo = 9;

long prevTime = 0;
long idleTime = 0;
long interval_reset = 180000;
long interval_idle = 20000;

void setup(){
  Serial.begin(9600);
  myservo.attach(9);
  Ethernet.begin(mac, ip);
  server.begin();
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
                Serial.println("Activate");
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
  
  if(device_status && !device_isLock()){
    char key = keypad.getKey();
    if (key != NO_KEY && key != '*' && key != '#'){
      isInputting = true;
      pass[count] = key;
      count++;
    }
    if(key == '*'){
      isInputting = false;
      attempt++;
      auth_user(pass);
      sys_init();
    }
    if(key == '#'){
      sys_init();
    }
    if(count > 9){
      auth_user(pass);
      sys_init();
    }
  }
  else{
    //Device Non-Active
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
    }
  }
}

void sys_init(){
  Serial.println("sys_init()");
  Serial.println(pass);
  memset(pass, 0, 10);
  count = 0;
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
  }
  else{
    Serial.println("Sending failed");
    String pass(user_input);
    if(pass == "123"){
      if(pass != "keluar"){
        open_door();
      }
    }
    else{
      wrong_password();
    }
  }
}

void open_door(){
  Serial.println("Door open");
  attempt = 0;
  myservo.write(30);
  delay(3000);
  while(digitalRead(sensor) == HIGH);
  delay(1000);
  myservo.write(70);
  Serial.println("Door closed");
  idleTime = millis();
}

boolean device_isLock(){
  if(attempt >= 3){
    //Serial.println("Device lock");
    return true;
  }
  else{
    //Serial.println("Device unlock");
    return false;
  }
}

void wrong_password(){
  Serial.println("Wrong Password");
  idleTime = millis();
}
