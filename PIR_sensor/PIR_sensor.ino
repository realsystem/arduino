
int SENSOR = 2;
int val = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSOR, INPUT);
}

void loop() {
  val = digitalRead(SENSOR);
  if (val == HIGH) {
    digitalWrite(LED_BUILTIN, HIGH);
    //delay(100);
  }
  else {
    digitalWrite(LED_BUILTIN, LOW);
    //delay(200);
  }
}
