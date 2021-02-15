#include <Herkulex.h>

void setup()  
{
  delay(2000);  //a delay to have time for serial monitor opening
  
  Serial.begin(115200);    // Open serial communications
  Serial.println("Begin");
  Herkulex.begin(115200,9,8); //open serial with rx=10 and tx=11
  delay(1000);
  
  Herkulex.reboot(0xfd); //reboot first motor
  delay(500); 
  Herkulex.initialize(); //initialize motors
  
  Serial.println("Move Speed");
  
  delay(10); 
  Herkulex.moveSpeedOne(0xfd, 300, 672, 1); //move motor with 300 speed  
  delay(2000);
  
  Serial.println("Move Position1: 200");
  
  delay(1);
  Herkulex.moveOne(0xfd, 200, 1500,2); //move to position 200 in 1500 milliseconds
  delay(1600);
  
  Serial.println("");
  Serial.print("Position servo 1:"); 
  Serial.println(Herkulex.getPosition(0xfd)); //get position
  
  Herkulex.setLed(2,LED_PINK); //set the led 
  Herkulex.setLed(0xfd,LED_GREEN2); //set the led
  
  Herkulex.end();
}

void loop(){

}
