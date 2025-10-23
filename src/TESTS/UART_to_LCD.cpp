#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Константы
const int LCD_ADDRESS = 0x27; // Адрес I2C дисплея (проверьте с помощью сканера)
const int LCD_COLUMNS = 20;   // Количество столбцов дисплея
const int LCD_ROWS = 4;       // Количество строк дисплея
const int BAUD_RATE = 9600;   // Скорость UART

// Инициализация объекта дисплея
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

void setup() {
  // Инициализация UART
  Serial.begin(BAUD_RATE);
  Serial.println("Starting UART...");

  // Инициализация дисплея
  Wire.begin();
  lcd.begin(LCD_COLUMNS, LCD_ROWS); // Указываем размеры дисплея
  lcd.backlight();
  Serial.println("LCD initialized");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Waiting for data");
  Serial.println("Displayed: Waiting for data");
}

void loop() {
  // Проверка наличия данных в UART
  if (Serial.available() > 0) {
    // Очистка дисплея
    lcd.clear();
    
    // Чтение данных с UART
    String input = Serial.readStringUntil('\n');
    Serial.print("Received: ");
    Serial.println(input);
    
    // Возврат полученных данных обратно через UART
    Serial.print("Echo: ");
    Serial.println(input);
    
    // Вывод на дисплей по строкам
    for (int row = 0; row < LCD_ROWS; row++) {
      int startIndex = row * LCD_COLUMNS;
      if (startIndex < static_cast<int>(input.length())) {
        lcd.setCursor(0, row);
        int endIndex = min(startIndex + LCD_COLUMNS, static_cast<int>(input.length()));
        lcd.print(input.substring(startIndex, endIndex));
        Serial.print("Displayed on row ");
        Serial.print(row);
        Serial.print(": ");
        Serial.println(input.substring(startIndex, endIndex));
      }
    }
  }
}