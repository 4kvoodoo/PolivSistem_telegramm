#include <ESP8266WiFi.h>
#include <SimplePortal.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include <FileData.h>
#include <LittleFS.h>
#include <FastBot.h>

#define NasosPin 5
#define MAX_ID 10
#define BOT_TOKEN ""

FastBot bot(BOT_TOKEN);

struct Data {
  char ssid[32];
  char password[32];
  int avto_poliv_sec;
  uint32_t poliv_time;
  unsigned long id[MAX_ID];
};
Data mydata;

FileData data(&LittleFS, "/data.dat", 'B', &mydata, sizeof(mydata));

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ru.pool.ntp.org", 3600, 60000); // Смещение +1 час (3600 сек), 1 минута обновления

void portal() {
  Serial.println("Portal Run");
  portalRun();  // запустить с таймаутом 60с

  switch (portalStatus()) {
    case SP_SUBMIT: Serial.println("Submit");
      WiFi.begin(portalCfg.SSID, portalCfg.pass);
      delay(6000);
      if (WiFi.status() == WL_CONNECTED) {
        strcpy(mydata.ssid, portalCfg.SSID);
        strcpy(mydata.password, portalCfg.pass);
        Serial.println("Connected");
        Serial.println(portalCfg.SSID);
        Serial.println(portalCfg.pass);
        data.update();
        break;
      }
    case SP_EXIT: Serial.println("SP exit");
      portal();
      break;
    case SP_ERROR: Serial.println("SP Error"); break;
    case SP_SWITCH_AP: Serial.println("SP switch ap"); break;
    case SP_SWITCH_LOCAL: Serial.println("SP switch local"); break;
    case SP_TIMEOUT: Serial.println("SP Timeout"); break;
  }
}

void connectToWiFi() {
  Serial.print("Подключаемся к WiFi: ");
  Serial.println(mydata.ssid);
  
  // Однократный вызов WiFi.begin()
  WiFi.begin(mydata.ssid, mydata.password);
  
  // Ожидаем подключения с таймаутом
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ Connected successfully!");
    Serial.print("📡 IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("❌ Connection failed. Status: ");
    Serial.println(WiFi.status());
    
    // Расшифровка статуса
    switch(WiFi.status()) {
      case WL_IDLE_STATUS: Serial.println("WiFi is in idle state"); break;
      case WL_NO_SSID_AVAIL: Serial.println("SSID not available"); break;
      case WL_SCAN_COMPLETED: Serial.println("Scan completed"); break;
      case WL_CONNECTED: Serial.println("Connected"); break;
      case WL_CONNECT_FAILED: Serial.println("Connection failed"); break;
      case WL_CONNECTION_LOST: Serial.println("Connection lost"); break;
      case WL_DISCONNECTED: Serial.println("Disconnected"); break;
      default: Serial.println("Unknown status"); break;
    }
    
    portal(); // Запускаем портал для конфигурации
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(NasosPin, OUTPUT);
  digitalWrite(NasosPin, LOW);

  LittleFS.begin();

  FDstat_t stat = data.read();

  switch (stat) {
    case FD_FS_ERR: Serial.println("FS Error");
      break;
    case FD_FILE_ERR: Serial.println("Error");
      break;
    case FD_WRITE: Serial.println("Data Write");
      break;
    case FD_ADD: Serial.println("Data Add");
      break;
    case FD_READ: Serial.println("Data Read");
      break;
    default:
      break;
  }

  delay(3000);
  
  Serial.println(mydata.ssid);
  Serial.println(mydata.password);

  connectToWiFi();

  bot.attach(newMsg);
  timeClient.begin();

}

void newMsg(FB_msg& msg) {
  bool id = false;
  // выводим ID чата, имя юзера и текст сообщения
  Serial.print(msg.chatID);     // ID чата 
  Serial.print(", ");
  Serial.print(msg.username);   // логин
  Serial.print(", ");
  Serial.println(msg.text);     // текст
  if (msg.text == "/my_id" || msg.text == "Мой id") {
    bot.replyMessage(String(msg.chatID), msg.messageID, msg.chatID);
  }
  for (int i = 0; i < MAX_ID; i++) {
    Serial.println(mydata.id[i]);
    if (mydata.id[i] == msg.chatID.toInt()) {
      id = true;
      break;
    }
    else if (mydata.id[i] == 0 && millis() <= 60000 && msg.text == "/start") {
      mydata.id[i] = msg.chatID.toInt();
      data.update();
      bot.replyMessage("Вы сохранились в системе", msg.messageID, msg.chatID);
      bot.showMenu(" Полив \n Помощь \n Мой id \n Выйти из меню ", msg.chatID);
      id = true;
      break;
    }
  }
  Serial.println("!");
  if (id) {
    if (msg.text.startsWith("/poliv ")) {
      int seconds;
      if (sscanf(msg.text.c_str(), "/poliv %d", &seconds) == 1) {
        if (seconds > 90) bot.replyMessage("Боюсь вы можете затопить свои растения столь долгим поливом", msg.messageID, msg.chatID);
        else if (seconds > 0) {
          bot.replyMessage("Последний полив был " + String((msg.unix - mydata.poliv_time)/86400)  + " дней и " + String(((msg.unix - mydata.poliv_time) / 3600) % 24) + " часов назад", msg.messageID, msg.chatID);
          digitalWrite(NasosPin, HIGH);
          bot.replyMessage("Полив начат", msg.messageID, msg.chatID);
          delay(seconds * 1000);
          digitalWrite(NasosPin, LOW);
          bot.replyMessage("Полив закончен", msg.messageID, msg.chatID);
          mydata.poliv_time= msg.unix;
          data.update();
        } else {
          bot.replyMessage("время полива не может быть отрицательным", msg.messageID, msg.chatID);
        }
      }
      else {
        Serial.println("Ошибка: не удалось извлечь число");
      }
    } else if (msg.text.startsWith("/new_user ")) {
      int new_user_id;
      if (sscanf(msg.text.c_str(), "/new_user %d", &new_user_id) == 1) {
        if (new_user_id > 100000001) {
          for (int i = 0; i < MAX_ID; i++) {
            Serial.println(mydata.id[i]);
            if (mydata.id[i] == new_user_id) break;
            else if (mydata.id[i] == 0) {
              mydata.id[i] = new_user_id;
              data.update();
              bot.replyMessage("Пользователь был сохранён в системе", msg.messageID, msg.chatID);
              break;
            }
          }
        } else bot.replyMessage("Пользователя с таким id не существует", msg.messageID, msg.chatID);
      }
    } else if (msg.text == "/poliv" || msg.text == "Полив") {
      bot.replyMessage("Последний полив был " + String((msg.unix - mydata.poliv_time)/86400)  + " дней и " + String(((msg.unix - mydata.poliv_time) / 3600) % 24) + " часов назад", msg.messageID, msg.chatID);
      digitalWrite(NasosPin, HIGH);
      bot.replyMessage("Полив начат", msg.messageID, msg.chatID);
      delay(8000);
      digitalWrite(NasosPin, LOW);
      bot.replyMessage("Полив закончен", msg.messageID, msg.chatID);
      mydata.poliv_time = msg.unix;
      data.update();
    } else if (msg.text.startsWith("/avto_poliv ")) {
      int seconds;
      if (sscanf(msg.text.c_str(), "/avto_poliv %d", &seconds) == 1) {
        if (seconds > 90) bot.replyMessage("Боюсь вы можете затопить свои растения столь долгим поливом", msg.messageID, msg.chatID);
        else if (seconds > 0) {
          bot.replyMessage("Последний полив был " + String((msg.unix - mydata.poliv_time)/86400)  + " дней и " + String(((msg.unix - mydata.poliv_time) / 3600) % 24) + " часов назад", msg.messageID, msg.chatID);
          digitalWrite(NasosPin, HIGH);
          bot.replyMessage("Полив начат", msg.messageID, msg.chatID);
          delay(seconds * 1000);
          digitalWrite(NasosPin, LOW);
          bot.replyMessage("Полив закончен", msg.messageID, msg.chatID);
          mydata.avto_poliv_sec = seconds;
          mydata.poliv_time= msg.unix;
          data.update();
        } else {
          bot.replyMessage("время полива не может быть отрицательным", msg.messageID, msg.chatID);
        }
      }
      else {
        Serial.println("Ошибка: не удалось извлечь число");
      }

    } else if (msg.text == "/time") {
      bot.sendMessage("Последний полив был " + String((msg.unix - mydata.poliv_time)/86400)  + " дней и " + String(((msg.unix - mydata.poliv_time) / 3600) % 24) + " часов назад", msg.chatID);
      bot.sendMessage("Нынешнее время: " + String(timeClient.getFormattedTime()), msg.chatID);
      bot.sendMessage("Нынешнее время: " + String(timeClient.getHours()), msg.chatID);
      bot.sendMessage("unix-время последнего сообщения " +String(msg.unix), msg.chatID);
    } else if (msg.text == "/help" || msg.text == "Помощь") {
      bot.replyMessage("Приветствуем вас в системе автополива 2.6 \n\n"
                       "Команды: \n"
                       "\"/help\" - помощь по коммандам \n"
                       "\"/start\" - начало работы, и первая регистрация в боте \n"
                       "\"/my_id\" - узнать свой id в telegram \n"
                       "\"/poliv n\" - зупускает полив растений на n секунд \n"
                       "\"/avto_poliv n\" - зупускает полив растений на n секунд, чтобы выключить автополив вместо n поставте 0 \n"
                       "\"/new_user n\" - добавление нового пользователя, вместо n подставте id юзера которого хотите добавить, его можно узнать по команде /my_id", msg.messageID, msg.chatID);

      
    } else if (msg.text == "Выйти из меню") {
      bot.closeMenu(msg.chatID);
    } else if (msg.text == "/menu") {
      bot.showMenu(" Полив \n Помощь \n Мой id \n Выйти из меню ", msg.chatID);
    }
  }
}

void loop() {
  if (data.tick() == FD_WRITE) Serial.println("Data updated!");

  if (timeClient.getHours() == 18 && mydata.avto_poliv_sec != 0) {
    digitalWrite(NasosPin, HIGH);
    delay(mydata.avto_poliv_sec * 1000);
    digitalWrite(NasosPin, LOW);
    data.update();
  }

  if (WiFi.status() != WL_CONNECTED) {
    portal();
  } else {
    bot.tick();
    timeClient.update(); // Обновление времени
  }
}
