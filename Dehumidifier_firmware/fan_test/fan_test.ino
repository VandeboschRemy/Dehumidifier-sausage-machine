// Declare variables 
unsigned long previousRPMMillis;
unsigned long previousMillis;
float RPM;
int state = 0;
unsigned long interval = 3000;
volatile unsigned long pulses=0;
unsigned long lastRPMmillis = 0;

// Specify the input and output pins
const int readPin = 2;  // Read the 
const int fanPin = 8;

void countPulse() {
  // just count each pulse we see
  // ISRs should be short, not like
  // these comments, which are long.
  pulses++;
}

unsigned long calculateRPM() {
  unsigned long RPM;
  noInterrupts();
  float elapsedMS = (millis() - lastRPMmillis)/1000.0;
  unsigned long revolutions = pulses/2;
  float revPerMS = revolutions / elapsedMS;
  RPM = revPerMS * 60.0;
  lastRPMmillis = millis();
  pulses=0;
  interrupts();

  return RPM;
}

// Set the duty-cycle
void analogWrite25k(int value)
{
    OCR4C = value;
}

// Set up the pins - first we want to set the duty-cycle to be at 25,000 Hz for PWM Control
// next we want to set up the RPM reading.
void setup()
{   
    TCCR4A = 0;
    TCCR4B = 0;
    TCNT4  = 0;

    // Mode 10: phase correct PWM with ICR4 as Top (= F_CPU/2/25000)
    // OC4C as Non-Inverted PWM output
    ICR4   = (F_CPU/25000)/2;
    OCR4C  = ICR4/2;                    // default: about 50:50
    TCCR4A = _BV(COM4C1) | _BV(WGM41);
    TCCR4B = _BV(WGM43) | _BV(CS40);

    Serial.begin(9600);
    pinMode(readPin,INPUT_PULLUP); // Set pin to read the Hall Effect Sensor
    attachInterrupt(digitalPinToInterrupt(readPin), countPulse, RISING); // Attach an interrupt to count

    // Set the PWM pin as output.
    pinMode( fanPin, OUTPUT);
}

void loop()
{
    int w = Serial.parseInt();
    if (w>0) {
        analogWrite25k(w);
        Serial.println(w);
        state = w;
    }

    if (millis() - previousMillis > interval) {
    Serial.print("RPM=");
    Serial.print(calculateRPM());
    Serial.print(F(" @ PWM="));
    Serial.println(state);
    previousMillis = millis();  
    }
  
}
