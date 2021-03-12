#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <DS18B20.h>

#define FIRMWARE "v1.02fix"

#define WATER_TEMP 225

#define UPPER_LIMIT_SW 12
#define LOWER_LIMIT_SW 11
#define WATER_LIMIT_SW 10
#define VALVE_LIMIT_SW 9
#define POWER_SW  1
#define COFFEE_SW 0
#define WATER_POT A1
#define GRINDER_POT A0
#define TEMPERATURE_SENSOR A7

#define FLOW_METER 8 

#define ELEVATOR_UP 2
#define ELEVATOR_DOWN 3
#define PUMP 4
#define GRINDER 5
#define HEATER 6
#define ELEVATOR_POWER 13

#define DELAY_POWER 250

#define SEQ_INIT 0
#define SEQ_REM 1
#define SEQ_WAIT 2
#define SEQ_SET_GRIND 3
#define SEQ_GRINDING 4
#define SEQ_UP 5
#define SEQ_BREW 6
#define SEQ_THREW_UP 7

#define ERROR_SWITCH_LIMIT -2
#define ERROR_NO_WATER_TANK -3

#define FLOW_MAX 550
#define FLOW_PREBREW 50

LiquidCrystal_I2C LCD(0x27,16,2);
OneWire oneWire(7);
DS18B20 sensor(&oneWire);

int seq = SEQ_INIT;
bool brewing = false;
bool brewing_pause = false;
bool power = false;
unsigned long brewwing_time = 0;
unsigned long brewing_start_time = 0;

int water = 0;
int grind = 0;

unsigned long flow_counter= 0;

bool prev_flow = true;
bool actual_flow = true;

struct switches
{
  bool upper_limit;
  bool lower_limit;
  bool water_limit;
  bool valve_limit;
};

switches sw1;

int get_switches()
{
  sw1.upper_limit = digitalRead(UPPER_LIMIT_SW);
  sw1.lower_limit = digitalRead(LOWER_LIMIT_SW);
  sw1.water_limit = digitalRead(WATER_LIMIT_SW);
  sw1.valve_limit = digitalRead(VALVE_LIMIT_SW);

  if(sw1.water_limit)
    return ERROR_NO_WATER_TANK;
  return 1;
}

void setup_outputs()
{
  pinMode(UPPER_LIMIT_SW, INPUT_PULLUP);
  pinMode(LOWER_LIMIT_SW, INPUT_PULLUP);
  pinMode(WATER_LIMIT_SW, INPUT_PULLUP);
  pinMode(VALVE_LIMIT_SW, INPUT_PULLUP);
  pinMode(FLOW_METER, INPUT_PULLUP);
  
  pinMode(POWER_SW, INPUT_PULLUP);
  pinMode(COFFEE_SW, INPUT_PULLUP);

  pinMode(WATER_POT, INPUT);
  pinMode(GRINDER_POT, INPUT);
  pinMode(TEMPERATURE_SENSOR, INPUT);

  pinMode(ELEVATOR_UP, OUTPUT);
  pinMode(ELEVATOR_DOWN, OUTPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(GRINDER, OUTPUT);
  pinMode(HEATER, OUTPUT);
  pinMode(ELEVATOR_POWER, OUTPUT);

  digitalWrite(ELEVATOR_POWER, HIGH);
  for(byte i = ELEVATOR_UP; i <= HEATER; i++)
  {
    digitalWrite(i, HIGH);
  }
}

bool debug = true;

int sequence(int seq)
{
  int err = get_switches();

  if(err != 1)
    seq = -1;

  switch (seq)
  {
  case SEQ_INIT:
    LCD.setCursor(0,0);
    LCD.backlight();
    LCD.print("  KAWOMAT 9000  ");
      LCD.setCursor(0,1);
      LCD.print("Inicjalizacja   ");

      if(sw1.lower_limit)
      {
        digitalWrite(ELEVATOR_POWER, LOW);
        delay(DELAY_POWER);
        digitalWrite(ELEVATOR_UP, HIGH);
        digitalWrite(ELEVATOR_DOWN, LOW);
      }
      else
      {
        digitalWrite(ELEVATOR_POWER, HIGH);
        delay(DELAY_POWER);
        digitalWrite(ELEVATOR_UP, HIGH);
        digitalWrite(ELEVATOR_DOWN, HIGH);
        seq++;
        delay(1000);
      }
    break;
  case SEQ_REM:
    digitalWrite(ELEVATOR_POWER, LOW);
    delay(DELAY_POWER);
    digitalWrite(ELEVATOR_UP, LOW);
    digitalWrite(ELEVATOR_DOWN, HIGH);
    delay(750);
    digitalWrite(ELEVATOR_POWER, HIGH);
    delay(DELAY_POWER);
    digitalWrite(ELEVATOR_UP, HIGH);

    power = false;
    LCD.noBacklight();    
    seq++; 
    break;

  case SEQ_WAIT:
    LCD.setCursor(0,1);
    LCD.print("POWER ");
    
    
    if(!digitalRead(POWER_SW))
    {
      power = true;
      LCD.backlight();
    }

    if(power)
      LCD.print("ON        ");
    else
    {
      LCD.print("OFF       ");
    }  
        
    if(power)
      seq = SEQ_SET_GRIND;
    break;

  case SEQ_SET_GRIND:
    digitalWrite(ELEVATOR_POWER, LOW);
    delay(DELAY_POWER);
    digitalWrite(ELEVATOR_UP, LOW);
    digitalWrite(ELEVATOR_DOWN, HIGH);
    delay(2250);
    digitalWrite(ELEVATOR_POWER, HIGH);
    delay(DELAY_POWER);
    digitalWrite(ELEVATOR_UP, HIGH);
    seq = SEQ_GRINDING;
    break;
  
  case SEQ_GRINDING:

    flow_counter = 0; 
    brewing = false;
    brewing_pause = false;
    water = 1024 - analogRead(WATER_POT);
    grind = 1024 - analogRead(GRINDER_POT);

    grind = map(grind, 0, 1023, 0, 600);

    water = water / 2;
    water += 30;

    LCD.setCursor(0,0);
    LCD.print("Woda: ");
    LCD.print(water);
    LCD.print(" ml   ");
    LCD.setCursor(0,1);
    LCD.print("Mlynek: ");
    if(grind < 100)
      LCD.print("wyl        ");
    else
    {
      LCD.print((float)grind/100);
      LCD.print("s  ");
    }
    if(!digitalRead(POWER_SW))
    {
      power = false;
      seq = SEQ_INIT;
    }

    if(!digitalRead(COFFEE_SW))
    {

      if(grind >= 100)
        {
          digitalWrite(GRINDER,LOW);
          delay(grind * 10);
          digitalWrite(GRINDER,HIGH);
          delay(2000);
        }      
      seq = SEQ_UP;
    }
    break;

  case SEQ_UP:
    if(!sw1.upper_limit)
    {
      digitalWrite(ELEVATOR_POWER, LOW);
      delay(DELAY_POWER);
      digitalWrite(ELEVATOR_DOWN, HIGH);
      digitalWrite(ELEVATOR_UP, LOW);
    }
    else
    {
      digitalWrite(ELEVATOR_POWER, HIGH);
      delay(DELAY_POWER);
      digitalWrite(ELEVATOR_UP, HIGH);
      digitalWrite(ELEVATOR_DOWN, HIGH);
      seq = SEQ_BREW;
      delay(1000);
    }  
    break;

    case SEQ_BREW:
      
      if(!brewing)
      {
        
        brewing = true;
        digitalWrite(PUMP, LOW);
        delay(3000);
        digitalWrite(PUMP, HIGH);
      }
      else
      {        
        LCD.setCursor(0,0);
        LCD.print(analogRead(TEMPERATURE_SENSOR));
        LCD.print("     ");
        if(analogRead(TEMPERATURE_SENSOR) < WATER_TEMP)
          digitalWrite(PUMP,LOW);
        digitalWrite(HEATER, LOW);

        actual_flow = digitalRead(FLOW_METER);

        if(prev_flow && !actual_flow)
        {
          flow_counter++;
        }
        prev_flow = actual_flow;


        if(flow_counter == FLOW_PREBREW)
        {
          brewing_pause = true;

          digitalWrite(PUMP, HIGH);
          digitalWrite(HEATER, HIGH);

          delay(5000);

          digitalWrite(HEATER, LOW);
          digitalWrite(PUMP, LOW); 
          flow_counter++;
        }
        if(flow_counter >= (unsigned long)water * 2) 
        {
          digitalWrite(PUMP, HIGH);
          digitalWrite(HEATER, HIGH);
          seq = SEQ_THREW_UP;
          delay(5000);
        }

      } 
      break;

    case SEQ_THREW_UP:

      if(sw1.lower_limit)
      {
        digitalWrite(ELEVATOR_POWER, LOW);
        delay(DELAY_POWER);
        digitalWrite(ELEVATOR_UP, HIGH);
        digitalWrite(ELEVATOR_DOWN, LOW);
      }
      else
      {
        digitalWrite(ELEVATOR_POWER, HIGH);
        delay(DELAY_POWER);
        digitalWrite(ELEVATOR_UP, HIGH);
        digitalWrite(ELEVATOR_DOWN, HIGH);
        seq = 8;
        delay(1000);
      }
    break;

  case 8:
    digitalWrite(ELEVATOR_POWER, LOW);
    delay(DELAY_POWER);
    digitalWrite(ELEVATOR_UP, LOW);
    digitalWrite(ELEVATOR_DOWN, HIGH);
    delay(750);
    digitalWrite(ELEVATOR_POWER, HIGH);
    delay(DELAY_POWER);
    digitalWrite(ELEVATOR_UP, HIGH);
    seq = SEQ_INIT;
    break;

  default: //turn all off, print error, return -1
    LCD.setCursor(0,1);    
    for(byte i = PUMP; i <= HEATER; i++)
    {
      digitalWrite(i, HIGH);
    }
    digitalWrite(ELEVATOR_POWER, HIGH);
    delay(DELAY_POWER);
    digitalWrite(ELEVATOR_UP, HIGH);
    digitalWrite(ELEVATOR_DOWN, HIGH);
    power = false;

    switch (err)
    {
    case ERROR_NO_WATER_TANK:
      LCD.print(" Brak wody    ");
      seq = -1;
      break;
    
    default:
      seq = SEQ_INIT;
      break;
    }
    
  }

  return seq;
}

void setup() {
  setup_outputs();
  power = true;
  LCD.init();
  LCD.setCursor(2,0);
  LCD.backlight();
  LCD.print("KAWOMAT 9000");

  if(debug)
  {
    LCD.setCursor(0,1);
    LCD.print("Soft: ");
    LCD.print(FIRMWARE);
  }

  delay(1500);
}

void loop() {
  seq = sequence(seq);
}