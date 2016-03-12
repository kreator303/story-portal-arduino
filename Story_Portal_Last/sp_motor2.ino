#define MOTOR_1_DIR_PIN 3
#define MOTOR_1_PUL_PIN 3


void setup() {                
  pinMode(3, OUTPUT);     
  pinMode(4, OUTPUT);
  digitalWrite(3, LOW);
  digitalWrite(4, LOW);
}

void loop() {
  for (int i = 3000; i > 500; i--) {
    digitalWrite(4, HIGH);
    delayMicroseconds(i);          
    digitalWrite(4, LOW); 
    delayMicroseconds(i);     
  }
  
  for(int i = 0; i < 10000; i++) {
    digitalWrite(4, HIGH);
    delayMicroseconds(500);
    digitalWrite(4, LOW);
    delayMicroseconds(500);
  }
  
  for (int i = 500; i <= 3000; i++) {
    digitalWrite(4, HIGH);
    delayMicroseconds(i);          
    digitalWrite(4, LOW); 
    delayMicroseconds(i);     
  }
  
  delay(10000);
  
}
