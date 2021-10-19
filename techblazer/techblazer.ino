#include <ESP8266WiFi.h>
const char* ssid     = "surfACe";
const char* password = "12345678";
WiFiServer server(80);
String header;

#define PWMA 5
#define DIRA 0
#define PWMB 4
#define DIRB 2
#define UV D7
#define FR_IR D8
#define FL_IR D5
#define BACK_IR D6
#define MOVE_SPEED 50   
#define BRAKE_SPEED 15

#define DEBUG true

struct Motor {
  const int PIN;
  const int DIR_PIN;
  const bool reversed;
};

void setupMotor(Motor motor) {
  pinMode(motor.PIN, OUTPUT);
  pinMode(motor.DIR_PIN, OUTPUT);
}

void moveMotor(Motor motor, int speed) {
  if (motor.reversed) speed = -speed;
  digitalWrite(motor.DIR_PIN, speed >= 1);
  analogWrite(motor.PIN, abs(speed));
}

void setUV(bool state) {
  digitalWrite(UV, state);
}

bool isOnFloor(uint8_t pin) {
  return digitalRead(pin) == 0;
}

void debug() {
  Serial.print(isOnFloor(FL_IR));
  Serial.print(isOnFloor(FR_IR));
  Serial.print(isOnFloor(BACK_IR));
  Serial.print("\n");
}

enum State {
  ON_TABLE,
  OFF_TABLE,
  REVERSE,
  TERMINATION_LAP,
  TERMINATE
};

bool lifted() {
  return (digitalRead(FR_IR) && digitalRead(FL_IR) && digitalRead(BACK_IR));
}


void setup() {
  Serial.begin(115200);
  pinMode(FL_IR, INPUT);
  pinMode(FR_IR, INPUT);
  pinMode(BACK_IR, INPUT);
  pinMode(UV, OUTPUT);
  //  setUV(true);
  //  delay(10000);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();
}

void loop() {
  Motor lMotor = {PWMB, DIRB, true};
  Motor rMotor = {PWMA, DIRA, false};
  setupMotor(lMotor);
  setupMotor(rMotor);

  int turnDir = -1;
  boolean tracking = false;
  enum State state = ON_TABLE;
  setUV(false);

  WiFiClient client = server.available();
  if (client) {
    moveMotor(lMotor, 0);
    moveMotor(rMotor, 0);
    setUV(false);
    
    String currentLine = "";
    while(client.connected()) {
      Serial.println("Connected");
      if (client.available()) {
        char c = client.read();
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();


            if (header.indexOf("GET /stop") >= 0) {
              Serial.println("Stop");
              moveMotor(lMotor, 0);
              moveMotor(rMotor, 0);
              setUV(false);
              tracking = false;
              //              movedebug();
            } else if (header.indexOf("GET /start") >= 0) {
              Serial.println("Start");
              client.println();
              tracking = true;
              while(tracking) {
                Serial.println(state);
                if(header.indexOf("GET /stop") >= 0) {
                  state = TERMINATE;
                  break;
                }
                if (lifted()) {
                  state = TERMINATE;
                } else {
                  setUV(true);
                }
                switch (state) {
                  case ON_TABLE:
                    {
                      if (isOnFloor(FL_IR)) {
                        moveMotor(lMotor, MOVE_SPEED);
                      } else {
                        moveMotor(lMotor, 0);
                      }
                      if (isOnFloor(FR_IR)) {
                        moveMotor(rMotor, MOVE_SPEED);
                      } else {
                        moveMotor(rMotor, 0);
                      }
                      delay(5);
                      if (!isOnFloor(FL_IR) && !isOnFloor(FR_IR)) state = OFF_TABLE;
                      break;
                    }
                  case OFF_TABLE:
                    {
                      turnDir = -turnDir;
                      // Reverse
                      moveMotor(lMotor, -50);
                      moveMotor(rMotor, -50);
                      delay(500);
                      moveMotor(lMotor, 15);
                      moveMotor(rMotor, 15);
          
                      // turn 90
                      moveMotor(lMotor, MOVE_SPEED * turnDir);
                      moveMotor(rMotor, -MOVE_SPEED * turnDir);
                      delay(910);
                      moveMotor(lMotor, -15 * turnDir);
                      moveMotor(rMotor, 15 * turnDir);
          
                      // move fwd
                      moveMotor(lMotor, 50);
                      moveMotor(rMotor, 50);
                      //        delay(1000);
                      int i = 0;
                      bool offTable = false;
                      while (i < 1000) {
                        if (!isOnFloor(FL_IR) && !isOnFloor(FR_IR)) {
                          offTable = true;
                          break;
                        }
                        i += 5;
                        delay(5);
                      }
                      moveMotor(lMotor, -15);
                      moveMotor(rMotor, -15);
          
                      if (offTable) {
                        state = TERMINATION_LAP;
                        break;
                      } else {
                        // turn 90
                        moveMotor(lMotor, MOVE_SPEED * turnDir);
                        moveMotor(rMotor, -MOVE_SPEED * turnDir);
                        delay(870);
                        moveMotor(lMotor, -15 * turnDir);
                        moveMotor(rMotor, 15 * turnDir);
                        state = ON_TABLE;
                        break;
                      }
                    }
                  case TERMINATION_LAP:
                    {
                      // Reverse
                      moveMotor(lMotor, -50);
                      moveMotor(rMotor, -50);
                      delay(500);
                      moveMotor(lMotor, 15);
                      moveMotor(rMotor, 15);
          
                      // turn 90
                      moveMotor(lMotor, MOVE_SPEED * turnDir);
                      moveMotor(rMotor, -MOVE_SPEED * turnDir);
                      delay(910);
                      moveMotor(lMotor, -15 * turnDir);
                      moveMotor(rMotor, 15 * turnDir);
          
                      while (isOnFloor(FL_IR) || isOnFloor(FR_IR)) {
                        if (isOnFloor(FL_IR)) {
                          moveMotor(lMotor, MOVE_SPEED);
                        } else {
                          moveMotor(lMotor, 0);
                        }
                        if (isOnFloor(FR_IR)) {
                          moveMotor(rMotor, MOVE_SPEED);
                        } else {
                          moveMotor(rMotor, 0);
                        }
                        delay(5);
                      }
                      tracking = false;
                      //                          setUV(false);
                      state = TERMINATE;
                      break;
                    }
                  case TERMINATE:
                    {
                      tracking = false;
                      setUV(false);
                      moveMotor(lMotor, 0);
                      moveMotor(rMotor, 0);
                      break;
                    }
                }
          
                delay(5);
              }
              

            }
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");

            client.println("<body><h1>surfACe</h1>");
            client.println("<p><a href=\"/start\"><button class=\"button\">START</button></a></p>");
            client.println("<p><a href=\"/stop\"><button class=\"button\">STOP</button></a></p>");
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
      
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
