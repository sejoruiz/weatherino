#include <LiquidCrystal.h>
#include <avr/pgmspace.h>
#include "HumidityLookup.h"


//Voltage and ADC
#define ADC_MAX_VAL 1023
#define ADC_NUM_VAL 1024

// Humidity constants
#define HUMIDITY_PIN A3
#define HUMIDITY_VCC 10
#define RESISTOR_LOAD 9940
#define RESISTOR_1K 1000

// Power LED 
#define LED_PIN 13 //Power on LED

// Temperature constants 
#define TEMP_PIN A1
#define VOLTAGE_SUM .378

// Light constants
#define LIGHT_PIN A2
#define LIGHT_MIN_R 1000
#define LIGHT_MAX_R 200000
#define F_OSC 16000000

//Light levels
#define LIGHT_LEVEL_CLOUDY_RELAY_OFF 270 //Must be under this level to turn OFF relay
#define LIGHT_LEVEL_CLOUDY_RELAY_ON 130 //Must be over this level to turn ON relay
#define LIGHT_LEVEL_NIGHT_RELAY_OFF 220 //Must be under this level to turn OFF relay
#define LIGHT_LEVEL_NIGHT_RELAY_ON 80 //Must be over this level to turn ON relay


//Serial
#define SERIAL_SPEED 115200
#define MAX_COMMAND_LENGTH 15

//Relay Plug
#define RELAY_PIN 9
#define RELAY_DELAY 500 //Time in ms to wait between relay switches

//LiquidCrystal pins
#define RS_PIN 12
#define E_PIN 11
#define D4_PIN 5
#define D5_PIN 4
#define D6_PIN 3
#define D7_PIN 2

/******************************************GLOBAL VARIABLE DEFINITION***************************************************/
int second, count;
boolean isDay, debug, manual;


//Init the LiquidCrystal
LiquidCrystal lcd(RS_PIN, E_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN); //Define LCD Pins


/*********************************FUNCTION DEFNIITIONS***********************************************/

/**
Function that transforms the measurements into ºC (estimated) Modify it to calibrate the measures.
**/
float getTemperature(int);

/**
 * Gets the humidity from the sensor, and converts it into relative humidity RH
 **/
float getHumidity()
{
  int hum_adc;
  float hum_res;
  digitalWrite(HUMIDITY_VCC, HIGH);
  while((hum_adc=analogRead(HUMIDITY_PIN))<=0);
  digitalWrite(HUMIDITY_VCC, LOW);
  
  //Let's get the humidity resistor value
  hum_res = (float) hum_adc/(ADC_MAX_VAL-hum_adc) * RESISTOR_LOAD / RESISTOR_1K;

  //Now we can look into the table to determine the humidity
  float temp = getTemperature(2);
  int tempCol = (int) temp/5 - 1; //We will look into this and next position
  float tempQuot = temp - 5*( (int) temp/5); //We will estimate the real Humidity using the quotient
  if(temp<0)
    {
      tempCol=0;
      tempQuot=0;
      temp=0;
    }
  else if(temp>60)
    {
      tempCol=11;
      tempQuot=0;
      temp=60;
    }
  Serial.println("DEBUG temp params:");
  Serial.print("Temp Col:");
  Serial.print(tempCol);
  Serial.print(" Temperature:");
  Serial.print(temp);
  Serial.print(" Quot:");
  Serial.println(tempQuot);
  Serial.print("humres: ");
  Serial.println(hum_res);
  //Now we need to determine which R is the closer in the table
  //We will guess the humidity for two closest temperatures
  float h1,h2, extracted;
  int row1=0, row2=0;
  extracted = pgm_read_float(&humidity_lookup[row1][tempCol]);
  //Humidity H1 Row. The humidity with temperature t1 shall be between row1 and row1-1
  while(row1<HUMIDITY_ROWS && extracted>hum_res)
    {
      row1++;
      extracted = pgm_read_float(&humidity_lookup[row1][tempCol]); //Update extracted value      
    }
  Serial.print("Closest R with h1: ");
  Serial.println(extracted);

  if(tempCol>=11)
    {
      row2=row1; //Security. The system will be accurate as long as Temp<=60
      extracted = pgm_read_float(&humidity_lookup[row2][tempCol]);
    }
  else
    {
      //Humidity H2. The humidity with temperature t2 shall be betweem row2 and row2-1
      extracted = pgm_read_float(&humidity_lookup[row2][tempCol+1]); //Update extracted value      
      while(row2<HUMIDITY_ROWS && humidity_lookup[row2][tempCol+1]>hum_res)
	{
	  row2++;
	  extracted = pgm_read_float(&humidity_lookup[row2][tempCol+1]); //Update extracted value      
	}
      Serial.print("Closest R with h2: ");
      Serial.println(extracted);
    }

  //Calculate H1
  if(row1<=0)
    {
      h1 = 20;
    }
  else
    {
      h1 = (hum_res - pgm_read_float(&humidity_lookup[row1][tempCol])) * (5*(row1-1)+20)/
	(pgm_read_float(&humidity_lookup[row1-1][tempCol]) - pgm_read_float(&humidity_lookup[row1][tempCol]));

      h1 += (pgm_read_float(&humidity_lookup[row1-1][tempCol]) - hum_res) * (5*(row1)+20)/
	(pgm_read_float(&humidity_lookup[row1-1][tempCol]) - pgm_read_float(&humidity_lookup[row1][tempCol]));
    }

  //Calculate H2
  if(row2<=0)
    { 
      h2 = 20;
    }
  else if(tempCol>=11)
    {
      h2=h1;
    }
  else
    {
      h2 = (hum_res - pgm_read_float(&humidity_lookup[row2][tempCol+1])) * (5*(row2-1)+20)/
	(pgm_read_float(&humidity_lookup[row2-1][tempCol+1]) - pgm_read_float(&humidity_lookup[row2][tempCol+1]));
  
      h2 += (pgm_read_float(&humidity_lookup[row2][tempCol+1]) - hum_res) * (5*(row2)+20)/
	(pgm_read_float(&humidity_lookup[row2-1][tempCol+1])-pgm_read_float(&humidity_lookup[row2][tempCol+1]));
    }

  //Finally, we can calculate the value of the humidity using both temperatures
  hum_res = ((h2*tempQuot)+(h1*(5-tempQuot)))/5;
  Serial.println("DEBUG humid params:");
  Serial.print("Row1: ");
  Serial.print(row1);
  Serial.print(" Row2: ");
  Serial.println(row2);
  Serial.print(" H1:");
  Serial.print(h1);
  Serial.print(" H2:");
  Serial.println(h2);

  return hum_res;
}

/**
 * Gets the light measurement. 
 * @return raw measurement from 0 to 1024. The higher the value, the more light is getting
 **/

float getLight(int numMeasurements)
{
  float light=0;
  for (int i = 0; i < (numMeasurements-1); i++)
    {
      light+=( float )analogRead(LIGHT_PIN)/numMeasurements;
      delay(5);
    }
  return light;
}

/**
 * Calculates the temperature in ºC
 **/
float getTemperature(int numMeasurements)
{
  float temp=0;
  for (int i=0; i < numMeasurements; i++)
    {
      temp+=( float )analogRead(TEMP_PIN)/numMeasurements;
      delay(15);
    }
  //Calculate the temperature
  temp=1.1*temp/1024; //Output voltage from the sensor
  temp=temp*100; // 10mV/ºC
  return temp;
}
		     
/*
 * Gets all the measurements into an array
 */		     
void getMeasures(float *measures)
{
  //We will take 4 measures in 1s and then we will average the result
  measures[0] = getTemperature(4);
  measures[1] = getLight(4);
  measures[2] = getHumidity();
}

/* Send measurements to UART 
 * float *measurement: [TEMP, LIGHT, HUMID]
 */
void measurementsToUART()
{
  float temp, light, humid;
  temp = getTemperature(1);
  light = getLight(4);
  humid = getHumidity();
  Serial.print("Temperature: ");
  Serial.println(temp);
  Serial.print("Light: ");
  Serial.println(light);
  Serial.print("Humidity: ");
  Serial.println(humid);
}

/* Send measurements to LCD
 * float *measurement: [TEMP, LIGHT, HUMID]
 */
void measurementsToLCD()
{
  float temp, light;
  temp = getTemperature(4);
  light = getLight(4);
  lcd.clear();
  lcd.print("Temp: ");
  lcd.print(temp);
  lcd.setCursor(11,0);
  lcd.print(" C");
  lcd.setCursor(0,1); //New Line
  lcd.print("Light: ");
  lcd.print(light);    
}

/**
 * Records a command through the UART. Debugging flag enabled means that it will print back the command through UART
 * @param Allocated array of MAX_COMMAND_LENGTH chars where the command will be stored
 **/
void recordCommand(char* command)
{
  int index=0;
  while(Serial.available() && index<MAX_COMMAND_LENGTH) //If we get any data through Serial
    {
      char c = Serial.read(); //consume the data
      command[index]=c;
      index++;
      delay(10); //We can wait for the next char to arrive. Shouldn't be necessary but just in case
    }

  // Insert the End of String char so the comparison is made correctly
  if(index==MAX_COMMAND_LENGTH)
    index--;
  
  command[index]='\0';
  if(debug)
  { 
    //DEBUG: Print command
    Serial.print("The received command is ");
    Serial.println(command);  
  }
}

/**
 * Routine during the day. Under LIGHT_LEVEL_CLOUDY light level, it is cloudy, and thus, we can turn on the relay
 **/
void relayDuringDay()
{
  if(digitalRead(RELAY_PIN)) //Lamp is ON, we need to check if we need to turn it OFF
    {
      int templight;//Relay is ON, Check if we are under the light OFF threshold
      if((templight=getLight(3))<LIGHT_LEVEL_CLOUDY_RELAY_OFF)
	{
	  if(debug)
	    {
	      Serial.print("Day mode, turning Relay OFF. Light level: ");
	      Serial.println(templight);
	    }
	  digitalWrite(RELAY_PIN, LOW);
	}
    }
  else //Lamp is OFF. Check if we need to turn it ON
    {
      int templight;
      //Relay is OFF. Check if we are under the light ON threshold
      if((templight=getLight(3))>LIGHT_LEVEL_CLOUDY_RELAY_ON)
	{
	  if(debug)
	    {
	      Serial.print("Day mode, turning Relay ON. Light level: ");
	      Serial.println(templight);
	    }
	  digitalWrite(RELAY_PIN, HIGH);
	}
    }
  delay(RELAY_DELAY); //Set a small delay 
}


/**
 * Routine during the night. Under LIGHT_LEVEL_LIGHTS_OFF light level, the lights are off, and thus, we can turn off the relay
 **/
void relayDuringNight()
{
  int templight; 
  if(digitalRead(RELAY_PIN)) //Lamp is ON, we need to check if we need to turn it OFF
    {
      //Relay is ON, Check if we are under the light OFF threshold
      if((templight=getLight(3))<LIGHT_LEVEL_NIGHT_RELAY_OFF)
	{
	  if(debug)
	    {
	      Serial.print("Night mode, turning Relay OFF. Light level: ");
	      Serial.println(templight);
	    }
	  digitalWrite(RELAY_PIN, LOW);
	}
    }
  else //Lamp is OFF. Check if we need to turn it ON
    {
      //Relay is OFF. Check if we are under the light ON threshold
      if((templight=getLight(3))>LIGHT_LEVEL_NIGHT_RELAY_ON)
	{
	  if(debug)
	    {
	      Serial.print("Night mode, turning Relay ON. Light level: ");
	      Serial.println(templight);
	    }
	  digitalWrite(RELAY_PIN, HIGH);
	}
    }
  delay(RELAY_DELAY); //Set a small delay 
}

/**
 * Routine for manual behavior. We basically turn on the relay all the time
 **/
void relayManual()
{
  digitalWrite(RELAY_PIN, HIGH);
}

/**
 * Execute the command stored in command
 * @param command: String ended in '\0' containing the command
 **/
void execCommand(char* comm)
{
  String command = String(comm); //We need to build a String object from the char*
  if(command.equals("day"))
    {
      if(debug)
	Serial.println("Good morning! Working on day light mode");
      isDay=true;
    }
  else if(command.equals("night"))
    {
      if(debug)
	Serial.println("Good night! Working on night mode");
      isDay=false;
    }
  else if(command.equals("debug ON"))
    {
      Serial.println("Turning debug ON");
	debug=true;
    }
  else if(command.equals("debug OFF"))
    {
      debug=false;
      Serial.println("Turning debug OFF");
    }
  else if(command.equals("manual ON"))
    {
      if(debug)
	Serial.println("Manual mode enabled");
      manual=true;
    }
  else if(command.equals("manual OFF"))
    {
      if(debug)
	Serial.println("Manual mode disabled");
      manual=false;
    }
  else
    {
      Serial.print("Command ");
      Serial.print(command);
      Serial.println(" not recognized. Printing measurements");
      measurementsToUART();
    }
}
/**
 * ISR interrupting. Every 15686 ints of a second
 *
 */
ISR(TIMER2_OVF_vect) //TIMER 2 Interrupt Service Routine
{
  second++; //Increment the LED counter
}

/*************************************SETUP AND LOOP******************************************************************************/
/**
 * Setup
 **/
void setup()
{ 
  //Init I/O pins
  pinMode(LED_PIN, OUTPUT); //Power On LED
  pinMode(HUMIDITY_VCC, OUTPUT); //VCC for the Humidity
  pinMode(RELAY_PIN, OUTPUT); //Relay
  pinMode(TEMP_PIN, INPUT);
  digitalWrite(HUMIDITY_VCC, LOW); //Just in case we put it to low

  //Setup analog reference
  analogReference(INTERNAL1V1); //Reference is 1.1V

  //LCD panel
  lcd.begin(16, 2); //Init LCD

  //Serial init
  Serial.begin(SERIAL_SPEED); //Testing serial
  
  /* Setup of interrupts */
  cli(); //Disable interrupts
  TCCR2A = 0; //Init the registers. Sometimes not needed, but we need to set an initial value
  TCCR2B = 0;
  TCNT2 = 0;
  TCCR2A |= (1 << COM2A1); //Non inverting mode
  TCCR2A |= (1 << WGM20); //Phase correct PWM mode
  TCCR2B |= (1 << CS20); //Clock/1
  TIMSK2 |= (1 << TOIE2); //We need an interrupt every time the TIMER 2 overflows so we can add the phase_add to phase_accu
  sei(); //Enable back the interrupts

  //Init global vars
  manual=false;
  debug=true;
  isDay=true;
}

/**
 * Execute forever
 **/
void loop()
{ //Interrupt driven program. We will just toggle a Power ON LED
  int recieved = 0; //If we get data through Serial, we will send a measure.
  
  // Blinking led routine. Indicator that the program is working
  if(second >= 15686)
  {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    second = 0;
    count++;
  }
  if(Serial.available()) {
    char command[MAX_COMMAND_LENGTH];
    
    //Record command
    recordCommand(command);

    //Execute the command
    execCommand(command);
  }

  // Update the LCD
  if(count>=10) //Update data every 5s
  {
    count=0;
    measurementsToLCD();
  }

  //Relay management
  if(manual)
    {
      relayManual();
    }
  else
    {
      if(isDay)
	relayDuringDay();
      else
	relayDuringNight();
    }
}

