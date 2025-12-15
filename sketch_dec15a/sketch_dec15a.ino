#include <Wire.h> // библиотека для I2C-коммуникации
#include <WiFi.h> // библиотека для работы с Wi-Fi
#include <Adafruit_GFX.h> // библиотека для работы с графикой на дисплее
#include <Adafruit_SSD1306.h> // библиотека для работы с драйвером дисплея (ssd1315)

// настройки дисплея
const int SCREEN_WIDTH = 128; // настройка высоты экрана
const int SCREEN_HEIGHT = 64; // настройка ширины экрана
const int OLED_ADDRESS = 0x3C; // I2C адрес дисплея в шестнадцатиричном формате

// настройка кнопок на дисплее (установка кнопкам gpio)
const int BUTTON_UP = 15; // кнопка вверх
const int BUTTON_DOWN = 16; // кнопка вниз
const int BUTTON_SELECT = 17; // кнопка выбора
const int BUTTON_SCAN = 18; // кнопка сканирования

// создание экземпляра класса для работы с дисплеем
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// настройка хранения информации о сетях
const int MAX_NETWORKS = 30; // максимальное значение списка сетей (можно менять)
String networks[MAX_NETWORKS]; // определение массива для хранения SSID имён сетей
int rssiValues[MAX_NETWORKS]; // массив для хранения уровня сигнала каждой сети
bool encrypted[MAX_NETWORKS]; // массив для хранения методов шифрования сетей
String bssidList[MAX_NETWORKS]; // массив для хранения MAC-адресов сетей
int network_count = 0; // инициализация переменной для хранения количества сетей

// переменные интерфейса
int scroll_offset = 0; // переменная, определяющая номер сети с которой начинается отображение
int selected_index = 0; // переменная, определяющая номер выбранной сети на которую выполняется подсветка
int networks_per_page = 7; // количество сетей на странице
bool in_detail_view = false; // флаг выбора режима просмотра


// функция инициализации устройства
void setup() {
  // скорость для передачи данных по серийному порту (нужно для отладки)
  Serial.begin(115200);
  
  // установка кнопок
  pinMode(BUTTON_UP, INPUT_PULLUP); // настройка кнопки вверх
  pinMode(BUTTON_DOWN, INPUT_PULLUP); // настройка кнопки вниз
  pinMode(BUTTON_SELECT, INPUT_PULLUP); // настройка кнопки выбора
  pinMode(BUTTON_SCAN, INPUT_PULLUP); // настройка кнопки сканирования
  
  // инициализация дисплея
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("Display error!");
    while (1);
  }
  
  display.clearDisplay(); // очистка информации с дисплея
  display.setTextSize(1); // установка размера текста
  display.setTextColor(WHITE); // установка цвета текста
  display.cp437(true); // включение расширенного набора символов
  
  // инициализация Wi-Fi
  WiFi.mode(WIFI_STA); // включение режима станции (клиента)
  WiFi.disconnect(); // отключение от сетей
  delay(100); // пауза
  
  // отрисовка стартового экрана
  display.setCursor(0, 0); // установка курсора на нулевой символ нулевой строки
  display.println(F("WiFi Scanner")); // Вывод строк
  display.println(F("Press SCAN (K4)")); // Вывод строк
  display.println(F("to find networks")); // Вывод строк
  display.display(); // Вывод информации на дисплей
}


// функция цикла событий для обработки нажатия на кнопки 
void loop() {
  // обработка нажатия на кнопку сканирования сетей
  if (digitalRead(BUTTON_SCAN) == LOW) {
    delay(250);
    scanNetworks();
  }
  
  // работа в режиме списка сетей
  if (!in_detail_view && network_count > 0) {
    // обработка нажатия на кнопку вверх
    if (digitalRead(BUTTON_UP) == LOW) {
      delay(200);
      // обработка при ситуации когда нажатие произошло на краю списка
      if (selected_index > 0) {
        selected_index--;
        if (selected_index < scroll_offset) {
          scroll_offset = selected_index;
        }
      }
      displayNetworks();
    }
    
    // обработка нажатия на кнопку вниз
    if (digitalRead(BUTTON_DOWN) == LOW) {
      delay(200);
      // обработка при ситуации когда нажатие произошло на краю списка
      if (selected_index < network_count - 1) {
        selected_index++;
        if (selected_index >= scroll_offset + networks_per_page) {
          scroll_offset = selected_index - networks_per_page + 1;
        }
      }
      displayNetworks();
    }
    
    // обработка нажатия на кнопку выбора
    if (digitalRead(BUTTON_SELECT) == LOW) {
      delay(200);
      showDetails();
    }
  } else if (in_detail_view) {
    // обработка нажатия на кнопку режима детального просмотра
    if (digitalRead(BUTTON_UP) == LOW || digitalRead(BUTTON_DOWN) == LOW || 
       digitalRead(BUTTON_SELECT) == LOW || digitalRead(BUTTON_SCAN) == LOW) {
      delay(250);
      in_detail_view = false;
      displayNetworks();
    }
  }
  
  delay(50);
}


// функция сканирования сетей Wi-Fi
void scanNetworks() {
  // отображение статуса сканирования
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Scanning..."));
  display.display();
  
  // переменная для хранения количества найденных сетей
  int found_count = WiFi.scanNetworks();
  
  // обработка события, при котором найденных сетей нет
  if (found_count <= 0) {
    network_count = 0;
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("No networks found"));
    display.display();
    delay(1000);
    displayNetworks();
    return;
  }
  
  // переменная, хранящая количество сетей
  network_count = min(found_count, MAX_NETWORKS);
  
  // сохранение информации о найденных сетях
  for (int i = 0; i < network_count; i++) {
    networks[i] = cleanSSID(WiFi.SSID(i));
    rssiValues[i] = WiFi.RSSI(i);
    encrypted[i] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    bssidList[i] = WiFi.BSSIDstr(i);
  }
  
  // сортировка по убыванию RSSI
  for (int i = 0; i < network_count - 1; i++) {
    for (int j = i + 1; j < network_count; j++) {
      if (rssiValues[j] > rssiValues[i]) {

        String temp_name = networks[i];
        networks[i] = networks[j];
        networks[j] = temp_name;
        
        int temp_rssi = rssiValues[i];
        rssiValues[i] = rssiValues[j];
        rssiValues[j] = temp_rssi;
        
        bool temp_enc = encrypted[i];
        encrypted[i] = encrypted[j];
        encrypted[j] = temp_enc;
        
        String temp_bssid = bssidList[i];
        bssidList[i] = bssidList[j];
        bssidList[j] = temp_bssid;
      }
    }
  }
  
  // сброс состояния интерфейса
  selected_index = 0;
  scroll_offset = 0;
  in_detail_view = false;
  displayNetworks();
}


// Функция очистки данных
String cleanSSID(String ssid) {
  String clean = "";
  
  // Формирование строки имени из прочитанных символов
  for (int i = 0; i < ssid.length(); i++) {
    char c = ssid[i];
    if (c >= 32 && c <= 126) {
      // если символ в нужном диапозоне, добавляем его в строку
      clean += c;
    }
  }
  // если длина строки равна нулю, заменяем её на Hidden
  if (clean.length() == 0) {
    clean = F("Hidden");
  }
  // если длина больше 22 символов, обрезаем её, чтобы вывести на дисплей
  if (clean.length() > 22) {
    clean = clean.substring(0, 22);
  }
  
  // возвращаем строку
  return clean;
}

// функция для отображения найденных сетей
void displayNetworks() {
  // очистка дисплея
  display.clearDisplay();
  
  // вывод количества сетей и позицию
  display.setCursor(0, 0);
  display.print(F("Net:"));
  display.print(network_count);
  display.print(F(" Set:"));
  display.print(selected_index + 1);
  display.fillRect(124, 0, 4, 8, BLACK);
  display.fillRect(124, 56, 4, 8, BLACK);
  
  // отрисовка случая отсутствия найденных сетей
  if (network_count == 0) {
    display.setCursor(0, 20);
    display.println(F("Press SCAN"));
    display.println(F("(K4 button)"));
    display.display();
    return;
  }
  
  // вычисление диапазона отображения
  int start_idx = scroll_offset;
  int end_idx = min(scroll_offset + networks_per_page, network_count);
  
  // отображение сетей
  for (int i = start_idx; i < end_idx; i++) {
    int line = i - scroll_offset;
    int y = 9 + line * 8;
    
    // отображение стрелки выбора
    display.setCursor(0, y);
    if (i == selected_index) {
      display.print(F(">"));
    } else {
      display.print(F(" "));
    }
    
    // вывод номера  сети
    display.print(i + 1);
    display.print(F("."));
    
    // вывод имени сети
    String name = networks[i];
    if (name.length() > 23) {
      name = name.substring(0, 23);
    }
    display.print(name);
    
    // вывод информации о защите сети
    if (encrypted[i]) {
      display.setCursor(120, y);
      display.print(F("*"));
    }
    
    // отображение уровня сигнала
    drawSignalDots(105, y, rssiValues[i]);
  }
  
  // отображение индиатора прокрутки
  if (network_count > networks_per_page) {
    if (scroll_offset > 0) {
      display.setCursor(124, 0);
      display.print(F("^"));
    }
    
    if (scroll_offset + networks_per_page < network_count) {
      display.setCursor(124, 56);
      display.print(F("v"));
    }
  }
  
  // вывод готовой информации
  display.display();
}


// функция отрисовки точек визуализации уровня сигнала RSSI
void drawSignalDots(int x, int y, int rssi) {
  int dots = 0;
  
  if (rssi > -50) {
    dots = 4;
  } else if (rssi > -60) { 
    dots = 3;
  } else if (rssi > -70) { 
    dots = 2;
  } else if (rssi > -80) { 
    dots = 1;
  }

  for (int i = 0; i < dots; i++) {
    display.setCursor(x + i * 3, y);
    display.print(F("."));
  }
}

// функция отрисовки детальной информации о сети
void showDetails() {
  in_detail_view = true;
  
  display.clearDisplay();
  display.setTextSize(1);
  
  // отрисовка имени сети
  display.setCursor(0, 0);
  display.print(F("Name: "));
  display.println(networks[selected_index]);
  
  // отрисовка мощности сигнала
  display.setCursor(0, 10);
  display.print(F("Signal: "));
  display.print(rssiValues[selected_index]);
  display.println(F(" dBm"));
  
  // отрисовка качества сигнала в процентах
  display.setCursor(0, 20);
  display.print(F("Quality: "));
  int quality = map(constrain(rssiValues[selected_index], -100, -30), -100, -30, 0, 100);
  display.print(quality);
  display.println(F("%"));
  
  // отрисовка полоски качетсва
  int bar_width = map(quality, 0, 100, 0, 50);
  display.fillRect(70, 20, bar_width, 6, WHITE);
  display.drawRect(70, 20, 50, 6, WHITE);
  
  // отображение шифрования
  display.setCursor(0, 30);
  display.print(F("Encryption: "));
  display.println(encrypted[selected_index] ? F("Yes") : F("No"));
  
  // отображение канала
  display.setCursor(0, 40);
  display.print(F("Channel: "));
  display.println(WiFi.channel());
  
  // отображение MAC адреса
  display.setCursor(0, 50);
  display.print(F("MAC: "));
  String mac = bssidList[selected_index];
  if (mac.length() > 0) {
    // отображение только первых 11 символов, в противном случае будет произведено обрезание
    if (mac.length() >= 11) {
      display.println(mac.substring(0, 11));
    } else {
      display.println(mac);
    }
  }
  
  // отрисовка готовой информации
  display.display();
}