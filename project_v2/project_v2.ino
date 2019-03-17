#include <LiquidCrystal.h>

#define INJECTOR_PIN 13
#define IMPULSATOR_PIN 2
#define IMPULSATOR_INTERRUPT 1
#define MAX_FUEL_CONSUMPTION 36 //liter per hour

//lcd and timer objects declaration
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//fuel consumption global variables
volatile uint16_t startingEdge = 0; 
volatile uint16_t endingEdge = 0; 
volatile uint16_t currentOverflowCounter = 0;
volatile uint16_t startOverflowCounter = 0;
volatile uint32_t timer3Clocks = 0;
volatile uint64_t overallTimer3Clocks = 0;
uint16_t injectorWorkingTimePerSecond = 0;
double consumptionPerHour = 0, consumptionPer100KM = 0;

//velocity global variables
volatile uint16_t impulsesPerSecond = 0;
uint64_t overallImpulseCounter = 0;
const double impulsesPerMeter = 16.85;
uint8_t speedOfVehicle = 0;

//general global variables
volatile bool reload = false;
uint16_t seconds = 0;


void calculateSpeed();
void calculateConsumption();
void display();
void incrementImpulsesAmmount();


void setup() {

	//LCD init
  lcd.begin(16,2);

  noInterrupts();
	//PIN's configuration  
  pinMode(INJECTOR_PIN, INPUT);
  pinMode(IMPULSATOR_PIN, INPUT);

	//interrupts configuration
	attachInterrupt(IMPULSATOR_INTERRUPT, incrementImpulsesAmmount, LOW);

  //T1 registers configuration
  TCCR1A = 0;

  TCCR1B |= (1 << WGM12); //CTC mode
  TCCR1B &= ~(1 << WGM13);

  TCCR1B &= ~(1 << CS10); // prescaler = 256
  TCCR1B &= ~(1 << CS11);
  TCCR1B |= (1 << CS12);

  TCNT1 = 0; // initialize counter 

  TIMSK1 |= (1 << OCIE1A); //enable output capture interrupt

  OCR1A = 62499;


  //T3 registers configuration
  TCCR3A = 0;
  
  TCCR3B |= (1 << CS30); // prescaler = 0
  TCCR3B &= ~(1 << CS31);
  TCCR3B &= ~(1 << CS32);
  
  TCCR3B &= ~(1 << ICES3); //input capture on falling edge
  

  TCNT3 = 0; // initialize counter  

  TIMSK3 |= (1 << ICIE3)|(1 << TOIE3); //enable input capture interrupt, enable timer ovf interrupt
  interrupts();
}


//loop method
void loop() {
  
  if(reload) {
    calculateSpeed();
    calculateConsumption();
    display();
    
    seconds++;
    reload = false;
  }
  
}

void calculateSpeed() {
  noInterrupts();
  speedOfVehicle = (impulsesPerSecond * 36) / (impulsesPerMeter * 10); //km/h (3600/1000) -> (36/10)
  overallImpulseCounter += impulsesPerSecond;
  impulsesPerSecond = 0;
  interrupts();
}

void calculateConsumption() {
  noInterrupts();
  injectorWorkingTimePerSecond = overallTimer3Clocks / 16000; //result in milliseconds
  overallTimer3Clocks = 0;

  consumptionPerHour = MAX_FUEL_CONSUMPTION * injectorWorkingTimePerSecond / 1000;
  consumptionPer100KM = MAX_FUEL_CONSUMPTION * injectorWorkingTimePerSecond / (speedOfVehicle * 10);
  interrupts();
}

void display() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("SPEED:");
  lcd.setCursor(7,0);
  lcd.print(speedOfVehicle);
  lcd.setCursor(12,0);
  lcd.print("KM/H");

  lcd.setCursor(0,1);
  lcd.print("CONS:");
  lcd.setCursor(7,1);
  if(speedOfVehicle < 5) {
    lcd.print(consumptionPerHour);
    lcd.setCursor(13,1);
    lcd.print("L/H");
  } else {
    lcd.print(consumptionPer100KM);
    lcd.setCursor(11,1);
    lcd.print("L/100");
  }
}


//impulsator ISR
void incrementImpulsesAmmount() {
  impulsesPerSecond++;
}

//T1 1000ms interrupt
ISR (TIMER1_COMPA_vect) {
  reload = true;
}

ISR (TIMER3_OVF_vect) {
  //ovf per 65536 timer clocks (4.096 [ms])
  currentOverflowCounter++;
}

ISR (TIMER3_CAPT_vect) {
  noInterrupts();
  
  //checking state of ICES3bit (ICES3 == 1 -> input capture on rising edge, ICES3 == 0 -> input capture on falling edge)
  if(((TCCR3B & (1 << ICES3)) >> ICES3) == 0) {           //if falling edge occurs
    startingEdge = ICR3;                                  //saving state(value) of ICR3 register
    startOverflowCounter = currentOverflowCounter;        //saving ovf counter state
  } else {                                                //if rising enge occurs
    endingEdge = ICR3;                                    //saving state(value) of ICR3 register

    //calculating how many timer3 clocks had place during injector impulse    
    timer3Clocks = (uint16_t)endingEdge + ((uint16_t)currentOverflowCounter * 65536) - ((uint16_t)startingEdge + (startOverflowCounter * 65536));
    
    overallTimer3Clocks += timer3Clocks;                  //adding to overall result
    currentOverflowCounter = 0;                           //clearing variable for next impulse
  }
  TCCR3B ^= (1 << ICES3);                                 //toggle ICES3 bit, preparing for next PWM's edge
  
  interrupts();
}