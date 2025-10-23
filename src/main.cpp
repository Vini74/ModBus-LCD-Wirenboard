#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ModbusRTUSlave.h>

// Пины для конфигурации через джамперы
#define ADDRESS_PIN_0 2
#define ADDRESS_PIN_1 3
#define ADDRESS_PIN_2 4
#define ADDRESS_PIN_3 5
#define SPEED_PIN_0 6
#define SPEED_PIN_1 7

// I2C адрес LCD (обычно 0x27 или 0x3F)
#define LCD_I2C_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4
#define LCD_TOTAL_CHARS (LCD_COLUMNS * LCD_ROWS)
#define LCD_BUFFER_SIZE (LCD_TOTAL_CHARS / 2) // 40 регистров для 80 символов

// Создание объектов
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);
ModbusRTUSlave modbus(Serial);

// Массивы регистров Modbus
uint16_t holdingRegisters[120]; // Регистры 0-119
uint16_t inputRegisters[10];    // Регистры 100-109 для системной информации

// Конфигурационные переменные
uint8_t modbusAddress = 16;
unsigned long modbusBaud = 9600;

// Переменная для хранения кода ошибки
uint16_t errorCode = 0; // 0 - нет ошибок

// Переменная для хранения состояния отображения
bool displayEnabled = true; // true - отображение включено, false - выключено

// Функция установки кода ошибки
void setErrorCode(uint16_t code)
{
  errorCode = code;
  inputRegisters[2] = errorCode; // Регистр 102 - ошибки устройства
}

// Функция сброса кода ошибки
void resetErrorCode()
{
  errorCode = 0;
  inputRegisters[2] = errorCode; // Регистр 102 - ошибки устройства
}

// Функция отображения ошибки на LCD
void displayError(const char *errorMessage)
{
}

// Функция отображения диагностической информации Modbus

// Массив для отслеживания изменений регистров
bool registerChanged[LCD_BUFFER_SIZE]; // 40 регистров для 80 символов

// Функция чтения конфигурации через джамперы
void readConfiguration()
{
  // Чтение адреса (биты 0-3)
  modbusAddress = 0;
  if (digitalRead(ADDRESS_PIN_0) == LOW) modbusAddress |= (1 << 0);
  if (digitalRead(ADDRESS_PIN_1) == LOW) modbusAddress |= (1 << 1);
  if (digitalRead(ADDRESS_PIN_2) == LOW) modbusAddress |= (1 << 2);
  if (digitalRead(ADDRESS_PIN_3) == LOW) modbusAddress |= (1 << 3);
  
  if (modbusAddress == 0) modbusAddress = 16; // Default без джамперов

  // Чтение скорости (как раньше, но уточним: LOW устанавливает бит в 1)
  uint8_t speedBits = 0;
  if (digitalRead(SPEED_PIN_0) == LOW) speedBits |= (1 << 0);
  if (digitalRead(SPEED_PIN_1) == LOW) speedBits |= (1 << 1);

  switch (speedBits) {
    case 0: modbusBaud = 9600; break;   // 00
    case 1: modbusBaud = 19200; break;  // 01
    case 2: modbusBaud = 57600; break;  // 10
    case 3: modbusBaud = 115200; break; // 11
  }

  // Сохранение в регистры
  inputRegisters[3] = modbusAddress;
  inputRegisters[4] = modbusBaud / 100; // В сотнях бод
}
// Функция обновления системной информации
void updateSystemInfo()
{
  inputRegisters[0] = 0x0100; // Версия прошивки 1.0
  inputRegisters[1] = 1;      // Статус устройства: готов
  // Регистр 102 (inputRegisters[2]) обновляется через функции setErrorCode/resetErrorCode
  // Регистры 103-104 обновляются в readConfiguration()
  // Регистры 105-109 зарезервированы
}

// Функция включения/выключения отображения на дисплее
void toggleDisplay(bool enable)
{
  displayEnabled = enable;
  if (!enable)
  {
    // Отключаем вывод данных на дисплей
    lcd.command(0x08);
  }
  else
  {
    // Включаем отображение информации без отображения курсора
    lcd.command(0x0C);
  }
}

// Функция преобразования символа в зависимости от кодовой страницы
uint8_t convertChar(uint8_t ch)
{
  // Пока просто возвращаем символ как есть
  // В будущем можно добавить преобразование для разных кодовых страниц
  return ch;
}

// Функция обновления экрана из буфера кадра
void updateScreenFromBuffer()
{
  bool screenChanged = false;

  // Проверка изменений в регистрах
  for (int i = 0; i < LCD_BUFFER_SIZE; i++)
  {
    if (registerChanged[i])
    {
      screenChanged = true;
      registerChanged[i] = false; // Сброс флага изменения
    }
  }

  // Если есть изменения, обновляем экран
  if (screenChanged)
  {
    for (int i = 0; i < LCD_BUFFER_SIZE; i++)
    {
      uint16_t regValue = holdingRegisters[i];

      // Получаем два символа из регистра
      uint8_t char1 = (regValue >> 8) & 0xFF;
      uint8_t char2 = regValue & 0xFF;

      // Вычисляем позиции символов на экране
      int pos1 = i * 2;
      int pos2 = i * 2 + 1;

      // Проверяем, что позиции находятся в пределах экрана
      if (pos1 < LCD_TOTAL_CHARS)
      {
        int row1 = pos1 / LCD_COLUMNS;
        int col1 = pos1 % LCD_COLUMNS;
        // Не выводим символ с кодом 0x00, так как его нет в знакогенераторе
        if (char1 != 0x00)
        {
          lcd.setCursor(col1, row1);
          lcd.write(char1);
        }
      }

      if (pos2 < LCD_TOTAL_CHARS)
      {
        int row2 = pos2 / LCD_COLUMNS;
        int col2 = pos2 % LCD_COLUMNS;
        // Не выводим символ с кодом 0x00, так как его нет в знакогенераторе
        if (char2 != 0x00)
        {
          lcd.setCursor(col2, row2);
          lcd.write(char2);
        }
      }
    }
  }
}

// Функция очистки экрана
void clearScreen()
{
  // Попытка очистки экрана
  lcd.clear();

  // Очистка буфера кадра
  for (int i = 0; i < LCD_BUFFER_SIZE; i++)
  {
    // Заполняем регистры пробелами
    holdingRegisters[i] = (32 << 8) | 32; // Два пробела
    registerChanged[i] = false;           // Сбрасываем флаги изменений
  }

  // Сброс кода ошибки
  resetErrorCode();
}

void setup()
{
  // Инициализация пинов конфигурации с подтяжкой к +5В
  pinMode(ADDRESS_PIN_0, INPUT_PULLUP);
  pinMode(ADDRESS_PIN_1, INPUT_PULLUP);
  pinMode(ADDRESS_PIN_2, INPUT_PULLUP);
  pinMode(ADDRESS_PIN_3, INPUT_PULLUP);
  pinMode(SPEED_PIN_0, INPUT_PULLUP);
  pinMode(SPEED_PIN_1, INPUT_PULLUP);

  // Небольшая задержка для стабилизации сигналов
  delay(100);

  // Чтение конфигурации
  readConfiguration();

  // Инициализация LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Modbus LCD Ready");
  lcd.setCursor(0, 1);
  lcd.print("Addr: ");
  lcd.print(modbusAddress);
  lcd.setCursor(0, 2);
  lcd.print("Baud: ");
  lcd.print(modbusBaud);

  // Конфигурация регистров Modbus
  modbus.configureHoldingRegisters(holdingRegisters, 120);
  modbus.configureInputRegisters(inputRegisters, 10);

  // Инициализация регистров
  // Инициализация регистров буфера пробелами (код 32)
  for (int i = 0; i < LCD_BUFFER_SIZE; i++)
  {
    // Каждый регистр содержит 2 символа
    holdingRegisters[i] = (32 << 8) | 32; // Два пробела
  }
  // Инициализация остальных регистров нулями
  for (int i = 100; i < 120; i++)
  {
    holdingRegisters[i] = 0;
  }
  for (int i = 0; i < 10; i++)
  {
    inputRegisters[i] = 0;
  }

  // Инициализация массива отслеживания изменений
  for (int i = 0; i < LCD_BUFFER_SIZE; i++)
  {
    registerChanged[i] = false;
  }

  // Установка системной информации
  updateSystemInfo();

  // Сброс кода ошибки
  resetErrorCode();

  // Инициализация Modbus
  Serial.begin(modbusBaud);
  modbus.begin(modbusAddress, modbusBaud);

  // Отображение статуса готовности
  lcd.setCursor(0, 3);
  lcd.print("Status: Ready");
}

void loop()
{
  // Сохраняем текущие значения регистров для отслеживания изменений
  static uint16_t lastHoldingRegisters[120];
  static bool firstRun = true;

  // При первом запуске копируем текущие значения регистров
  if (firstRun)
  {
    firstRun = false;
    // Инициализируем регистры modbus при первом запуске
    holdingRegisters[112] = 1; // Включаем отображение по умолчанию

    for (int i = 0; i < 120; i++)
    {
      lastHoldingRegisters[i] = holdingRegisters[i];
    }
  }

  // Обработка Modbus запросов
  modbus.poll();

  // Проверяем изменения в регистрах для отслеживания активности Modbus
  for (int i = 0; i < 120; i++)
  {
    if (lastHoldingRegisters[i] != holdingRegisters[i])
    {
      // Устанавливаем флаг изменения для регистров буфера кадра
      if (i < LCD_BUFFER_SIZE)
      {
        registerChanged[i] = true;
      }

      lastHoldingRegisters[i] = holdingRegisters[i];
    }
  }

  // Обработка команды очистки экрана (регистр 110)
  if (holdingRegisters[110] == 1)
  {
    clearScreen();
    holdingRegisters[110] = 0; // Сброс команды
    holdingRegisters[111] = 1; // Установка статуса выполнения (1 - выполнено)
  }

  // Обработка команды включения/выключения отображения (регистр 112)
  static bool lastDisplayEnabled = true;
  if (holdingRegisters[112] != lastDisplayEnabled)
  {
    toggleDisplay(holdingRegisters[112] != 0);
    lastDisplayEnabled = (holdingRegisters[112] != 0);
  }

  // Обновление экрана из буфера кадра
  updateScreenFromBuffer();

  // Небольшая задержка для предотвращения перегрузки процессора
  delay(10);
}