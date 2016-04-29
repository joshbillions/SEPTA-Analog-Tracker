
void turnOffAllGauges(){
  digitalWrite(kGauge1, LOW);
  digitalWrite(kGauge2, LOW);
  digitalWrite(kGauge3, LOW);
  digitalWrite(kGauge4, LOW);
  digitalWrite(kGauge5, LOW);
  digitalWrite(kGauge6, LOW);
}

void turnOnAllGauges(){
  digitalWrite(kGauge1, HIGH);
  digitalWrite(kGauge2, HIGH);
  digitalWrite(kGauge3, HIGH);
  digitalWrite(kGauge4, HIGH);
  digitalWrite(kGauge5, HIGH);
  digitalWrite(kGauge6, HIGH);
}

void setAllGaugesPWM(int pwmValue){
  analogWrite(kGauge1, pwmValue);
  analogWrite(kGauge2, pwmValue);
  analogWrite(kGauge3, pwmValue);
  analogWrite(kGauge4, pwmValue);
  analogWrite(kGauge5, pwmValue);
  analogWrite(kGauge6, pwmValue);
}

void runGaugeTests(){
  int mode = int(random(0, 1));

  switch(mode){
    case 0:
        setAllGaugesPWM(int(random(0, 1023)));
        break;
    case 1:
        int randomPosition = int(random(0,5));
        setAllGaugesPWM(positionArray[randomPosition]);
        break;
  }

  delay(int(random(1000, 5000)));
}
