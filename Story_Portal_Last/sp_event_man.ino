#define WHEEL_STATE_PIN 2
#define WHEEL_TICK_PIN 3

#define TICK_DEBOUNCE_MS 150

volatile int wheel_state = LOW;
volatile long last_tick;
volatile long current_tick; 

void setup() {
  
  Serial.begin(115200);
  
  pinMode(WHEEL_STATE_PIN, INPUT);
  pinMode(WHEEL_TICK_PIN, INPUT);
  
  pinMode(13, OUTPUT);
  
  attachInterrupt(0, toggle_wheel_state, CHANGE);
  attachInterrupt(1, toggle_tick_state, CHANGE);

  Serial.println("READY");

}



void loop() {

 current_tick = millis();
  
 if (wheel_state == HIGH) {
   
    // do stuff when the wheel is moving
    Serial.println("MOVING");
    delay(200);
    
  } else {
   
    // do stuff when the wheel isn't moving
//    Serial.println("STOPPED");
    
  }
  
}

void toggle_tick_state() {
  if (current_tick - last_tick > TICK_DEBOUNCE_MS) {
    
    // do stuff when the tick pin is on
    digitalWrite(13, HIGH);
    delay(100);
    digitalWrite(13, LOW);
    Serial.println("TICK");
    last_tick = current_tick;
  }
}

void toggle_wheel_state() {
  wheel_state = !wheel_state; 
}
