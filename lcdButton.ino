//Test Script

#include <SPI.h>
#include "src/NKKSmartDisplayLCD/NKKSmartDisplayLCD.h"
#include "src/gfx/test.h"

//Backlight pins
#define RED_PIN   4
#define GREEN_PIN 5

//Panel: 36x24, LP=22, FLM=23
NKKSmartDisplayLCD lcd(36, 24, 22, 23);

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(GREEN_PIN, HIGH);

  //Power the Mega / VDD first
  //Then apply VLC
  //Datasheet warning: do NOT apply VLC before VDD.
  delay(500);

  lcd.begin(1000000, SPI_MODE2);   // 1 MHz until stable
  lcd.startRefresh(277);
}

void loop()
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Vergil");

  delay(1500);

  lcd.setCursor(0,1);
  lcd.print("Müller");

  delay(1500);

  lcd.setCursor(0,2);
  lcd.print("ÄÖÜ äöü ß");

  delay(1500);

  lcd.drawBarsTest();

  delay(1500);

  lcd.clear();
  test::draw(lcd, 0, 0);

  delay(1500);
}
