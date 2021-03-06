#include <SoftwareSerial.h>
#include "stk500.h"
#include <Arduino.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



const char* ssid     = "redmi_4xxx";
const char* password = "87654321";

// http://cc33953.tmweb.ru/other/program.hex

const String fileURL = "http://cc33953.tmweb.ru/other/program.hex";
const String fileName = "program.hex";

HTTPClient Http;
File uploadFile;


#define BOOT_BAUD 115200
#define DEBUG_BAUD 19200

#define txPin 10   // S3 GPIO10 10
#define rxPin 9    // S2 GPIO9 9

#define RESET_PIN 2   // D4 GPIO2 2

SoftwareSerial sSerial;

File myFile;

const int chipSelect = 0; // D3 GPIO0 0

avrmem mybuf;
unsigned char mempage[128];

#ifdef STANDALONE_DEBUG
#define DEBUGPLN Serial.println
#define DEBUGP Serial.print
#else
#define DEBUGPLN sSerial.println
#define DEBUGP sSerial.print
#endif

// Line Buffer is set up in global SRAM
#define LINELENGTH 50
unsigned char linebuffer[LINELENGTH];
unsigned char linemembuffer[16];


// Декларация для дисплея SSD1306, подключенного к I2C (контакты SDA, SCL)
Adafruit_SSD1306 display(128, 64, &Wire, LED_BUILTIN);

#define dlOLED 1000 // Время задержки OLED сообщений

static const unsigned char PROGMEM logo_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };





void setup() {
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Работа с 128x64 OLED дисплеем по адресу 0x3C
  display.display(); // Устанавливаем изображение
  delay(2000); // Задержка

  displayOLED ("Старт", 0, 0, 1000, 1);
  

  sSerial.begin(DEBUG_BAUD, SWSERIAL_8N1, rxPin, txPin, false, 95, 11);
  Serial.begin(BOOT_BAUD);

  mybuf.buf = &mempage[0];

  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  pinMode(RESET_PIN,OUTPUT);
  pinMode(chipSelect,OUTPUT);


  // Соединение с сетью
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(1000);
    DEBUGPLN("Connecting..");
    displayOLED("", 0, 0, 100, 1);
    displayOLED("Подключение к сети..", 0, 0, 100, 1);
  }
  DEBUGPLN();
  displayOLED("Подключение к сети\nуспешно", 0, 0, dlOLED, 1);


  // Соединение с SD картой
  if (!SD.begin(chipSelect)) {
    DEBUGPLN("Card failed, or not present");
    displayOLED("Ошибка подключения\nSD карты", 0, 0, 2000, 1);
    return;
  }
  DEBUGPLN("Card initialized.");
  displayOLED("SD карта\nинициализирована", 0, 0, dlOLED, 1);


  // Подключение к серверу и загрузка файла прошивки на SD-карту
  displayOLED("Функция загрузки\nпрошивки", 0, 0, dlOLED, 1);
  if(!downloadFile()){
    return;
  }
  displayOLED("Функция загрузки\nпрошивки завершена", 0, 0, 1000, 1);


  // Функция установки прошивки
  displayOLED("Функция установки\nпрошивки", 0, 0, dlOLED, 1);
  programArduino("program.hex");
  displayOLED("Функция установки\nпрошивки завершена", 0, 0, dlOLED, 1);
}





void loop() {

}





int downloadFile() {
  
  int err = 0;
  
  // Открываем соединение
  Http.begin(fileURL);
  displayOLED("Открываем соединение", 0, 0, dlOLED, 1);
  
  // Код ответа
  int httpCode = Http.GET();
  displayOLED("Код ответа: " + String(httpCode), 0, 0, dlOLED, 1);
  
  if (httpCode > 0) {
  
    // Файл найден на сервере
    if (httpCode == HTTP_CODE_OK) {
      
      displayOLED("Файл найден на\nсервере", 0, 0, dlOLED, 1);

      // Удалить файл:
      SD.remove(fileName);
      displayOLED("Удаляем program.hex \nс SD карты", 0, 0, dlOLED, 1);

       // Открываем в режиме записи
      uploadFile = SD.open(fileName, FILE_WRITE);
      displayOLED("Подготовка к загрузке\nновой прошивки", 0, 0, dlOLED, 1);

       // Получить длину документа
      int len = Http.getSize();
      displayOLED("Размер документа:\n" + String(len) + " byte", 0, 0, dlOLED, 1);

       // Создать буфер для чтения
      uint8_t buff[2048] = {0};
      
      // Получить TCP-поток
      WiFiClient *stream = Http.getStreamPtr();
    
      // Считывание всех данных с сервера
      while (Http.connected() && (len > 0 || len == -1)) {

        // Получить доступный размер данных
        size_t size = stream -> available();
        
        // Чтение до 128 байт
        if (size) { 
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

           // Запишем в файл
          uploadFile.write(buff, c);
          
          if (len > 0) len -= c;
        }
        delayMicroseconds(1);
      }
      
      displayOLED("Загрузка завершена", 0, 0, dlOLED, 1);  
      err = 1;
    }
    
    else {
      displayOLED("Ошибка соединения\nКод: " + String(httpCode), 0, 0, dlOLED, 1);
      err = 0;
    }
  }
  
  Http.end();
  displayOLED("Закрываем соединение: ", 0, 0, dlOLED, 1);
  
  uploadFile.close();
  displayOLED("Закрываем Файл: ", 0, 0, dlOLED, 1);

  if(err == 0){
    displayOLED("Ошибка соединения\nКод: " + String(httpCode), 0, 0, dlOLED, 1);
  }
  
  return err;
}





int readPage(File input, avrmem *buf) {
  
  int len;
  int address;
  int total_len = 0;
  
  // grab 128 bytes or less (one page)
  for (int i = 0 ; i < 8; i++) {
    
    len = readIntelHexLine(input, &address, &linemembuffer[0]);
    
    if (len < 0) {
      break;
    }
    else {
      total_len = total_len + len;
    }

    // first record determines the page address
    if (i == 0) {
      buf->pageaddress = address;
    }
    
    memcpy((buf -> buf) + (i * 16), linemembuffer, len);
  }
  
  buf -> size = total_len;
  
  return total_len;
}

// read one line of intel hex from file. Return the number of databytes
// Since the arduino code is always sequential, ignore the address for now.
// If you want to burn bootloaders, etc. we'll have to modify to 
// return the address.



// INTEL HEX FORMAT:
// :<8-bit record size><16bit address><8bit record type><data...><8bit checksum>
int readIntelHexLine(File input, int *address, unsigned char *buf){
  
  unsigned char c;
  int i = 0;
  
  while (true) {
    if (input.available()) {
      c = input.read();
      
      // this should handle unix or ms-dos line endings.
      // break out when you reach either, then check
      // for lf in stream to discard
      
      if ((c == 0x0d) || (c == 0x0a)) {
        break;
      }
      else {
        linebuffer[i++] = c;
      }
    }
    
    else {
      return -1; //end of file
    }
  }
  
  linebuffer[i] = 0; // terminate the string
  
  //peek at the next byte and discard if line ending char.
  if (input.peek() == 0xa){
    input.read();
  }
  
  int len = hex2byte(&linebuffer[1]);
  
  *address = (hex2byte(&linebuffer[3]) <<8) | (hex2byte(&linebuffer[5]));
  
  int j = 0;
  for (int i = 9; i < ((len * 2) + 9); i += 2){
    buf[j] = hex2byte(&linebuffer[i]);
    j++;
  }
  
  return len;
}



unsigned char hex2byte(unsigned char *code) {
  unsigned char result = 0;
  if      ((code[0] >= '0') && (code[0] <= '9')) result = ((int)code[0] - '0') << 4;
  else if ((code[0] >= 'A') && (code[0] <= 'F')) result = ((int)code[0] - 'A' + 10) << 4;
  if      ((code[1] >= '0') && (code[1] <= '9')) result |= ((int)code[1] - '0');
  else if ((code[1] >= 'A') && (code[1] <= 'F')) result |= ((int)code[1] -'A' + 10);  
  return result;
}



// Right now there is only one file.
void programArduino(char * filename) {
  digitalWrite(RESET_PIN, HIGH);
  
  unsigned int major = 0;
  unsigned int minor = 0;
  delay(100);
  
  toggle_Reset();
  delay(10);
  
  stk500_getsync();
  
  stk500_getparm(Parm_STK_SW_MAJOR, &major);
  
  DEBUGP("software major: ");
  DEBUGPLN(major);
  
  stk500_getparm(Parm_STK_SW_MINOR, &minor);
  
  DEBUGP("software Minor: ");
  DEBUGPLN(minor);
  
  if (SD.exists(filename)){
    myFile = SD.open(filename, FILE_READ);
  }
  
  else{
    DEBUGP(filename);
    DEBUGPLN(" doesn't exist");
    return;
  }
  
  // enter program mode
  stk500_program_enable();
  
  while (readPage(myFile, &mybuf) > 0){
    stk500_loadaddr(mybuf.pageaddress>>1);
    stk500_paged_write(&mybuf, mybuf.size, mybuf.size);
  }
  
  // could verify programming by reading back pages and comparing but for now, close out
  stk500_disable();
  delay(10);
  
  toggle_Reset();
  
  myFile.close();
  
  blinky(4,500); 
}



void blinky(int times, long delaytime){
  for (int i = 0 ; i < times; i++){
//    digitalWrite(LED1,HIGH);
    delay(delaytime);
//    digitalWrite(LED1, LOW);
    delay (delaytime);
  } 
}



void toggle_Reset() {
  digitalWrite(RESET_PIN, LOW);
  delayMicroseconds(1000);
  digitalWrite(RESET_PIN,HIGH);
}



static int stk500_send(byte *buf, unsigned int len) {
  Serial.write(buf, len);
}



static int stk500_recv(byte * buf, unsigned int len) {
  int rv;
  rv = Serial.readBytes((char *)buf, len);
  if (rv < 0) {
    error(ERRORNOPGMR);
    return -1;
  }
  return 0;
}



int stk500_drain() {
  while (Serial.available()> 0) {  
    DEBUGP("draining: ");
    DEBUGPLN(Serial.read(),HEX);
  }
  return 1;
}



int stk500_getsync() {
  byte buf[32], resp[32];
  
  /* get in sync */
  buf[0] = Cmnd_STK_GET_SYNC;
  buf[1] = Sync_CRC_EOP;
  
  /* First send and drain a few times to get rid of line noise */
  
  stk500_send(buf, 2);
  stk500_drain();
  stk500_send(buf, 2);
  stk500_drain();
  stk500_send(buf, 2);
  
  if (stk500_recv(resp, 1) < 0) {
    return -1;
  }
  
  if (resp[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC,resp[0]);
    stk500_drain();
    return -1;
    
    if (stk500_recv(resp, 1) < 0) {
      return -1;
    }
    
    if (resp[0] != Resp_STK_OK) {
      error1(ERRORNOTOK,resp[0]);
      return -1;
    }
  }
  return 0;
}



static int stk500_getparm(unsigned parm, unsigned * value) {
  byte buf[16];
  unsigned v;
  int tries = 0;

retry:

  tries++;
  buf[0] = Cmnd_STK_GET_PARAMETER;
  buf[1] = parm;
  buf[2] = Sync_CRC_EOP;

  stk500_send(buf, 3);

  if (stk500_recv(buf, 1) < 0) {
    exit(1);
  }
  
  if (buf[0] == Resp_STK_NOSYNC) {
    if (tries > 33) {
      error(ERRORNOSYNC);
      return -1;
    }
    if (stk500_getsync() < 0) {
      return -1;
    }
    goto retry;
  }
  
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC,buf[0]);
    return -2;
  }

  if (stk500_recv(buf, 1) < 0) {
    exit(1);
  }
  
  v = buf[0];

  if (stk500_recv(buf, 1) < 0) {
    exit(1);
  }
  
  if (buf[0] == Resp_STK_FAILED) {
    error1(ERRORPARMFAILED,v);
    return -3;
  }
  
  else if (buf[0] != Resp_STK_OK) {
    error1(ERRORNOTOK,buf[0]);
    return -3;
  }

  *value = v;

  return 0;
}



/* read signature bytes - arduino version */
static int arduino_read_sig_bytes(AVRMEM * m) {
  unsigned char buf[32];

  /* Signature byte reads are always 3 bytes. */

  if (m -> size < 3) {
    DEBUGPLN("memsize too small for sig byte read");
    return -1;
  }

  buf[0] = Cmnd_STK_READ_SIGN;
  buf[1] = Sync_CRC_EOP;

  stk500_send(buf, 2);

  if (stk500_recv(buf, 5) < 0) {
    return -1;
  }
  
  if (buf[0] == Resp_STK_NOSYNC) {
    error(ERRORNOSYNC);
    return -1;
  } 
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC,buf[0]);
    return -2;
  }
  
  if (buf[4] != Resp_STK_OK) {
    error1(ERRORNOTOK,buf[4]);
    return -3;
  }

  m->buf[0] = buf[1];
  m->buf[1] = buf[2];
  m->buf[2] = buf[3];

  return 3;
}



static int stk500_loadaddr(unsigned int addr) {
  
  unsigned char buf[16];
  int tries;

  tries = 0;
  
retry:

  tries++;
  
  buf[0] = Cmnd_STK_LOAD_ADDRESS;
  buf[1] = addr & 0xff;
  buf[2] = (addr >> 8) & 0xff;
  buf[3] = Sync_CRC_EOP;

  stk500_send(buf, 4);

  if (stk500_recv(buf, 1) < 0) {
    exit(1);
  }
  
  if (buf[0] == Resp_STK_NOSYNC) {
    if (tries > 33) {
      error(ERRORNOSYNC);
      return -1;
    }
    if (stk500_getsync() < 0) {
      return -1;
    }
    goto retry;
  }
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC, buf[0]);
    return -1;
  }

  if (stk500_recv(buf, 1) < 0) {
    exit(1);
  }
  if (buf[0] == Resp_STK_OK) {
    return 0;
  }

  error1(ERRORPROTOSYNC, buf[0]);
  return -1;
}



static int stk500_paged_write(AVRMEM * m, int page_size, int n_bytes) {
  
  // This code from avrdude has the luxury of living on a PC and copying buffers around.
  // not for us...
  
  // unsigned char buf[page_size + 16];
  unsigned char cmd_buf[16]; //just the header
  int memtype;
  // unsigned int addr;
  int block_size;
  int tries;
  unsigned int n;
  unsigned int i;
  int flash;

  // Fix page size to 128 because that's what arduino expects
  page_size = 128;
  
  // EEPROM isn't supported
  memtype = 'F';
  flash = 1;

    /* build command block and send data separeately on arduino*/
    
    i = 0;
    cmd_buf[i++] = Cmnd_STK_PROG_PAGE;
    cmd_buf[i++] = (page_size >> 8) & 0xff;
    cmd_buf[i++] = page_size & 0xff;
    cmd_buf[i++] = memtype;
    stk500_send(cmd_buf,4);
    stk500_send(&m->buf[0], page_size);
    cmd_buf[0] = Sync_CRC_EOP;
    stk500_send( cmd_buf, 1);

    if (stk500_recv(cmd_buf, 1) < 0) {
      exit(1); // errr need to fix this... 
    }
    
    if (cmd_buf[0] == Resp_STK_NOSYNC) {
        error(ERRORNOSYNC);
        return -3;
     }
    else if (cmd_buf[0] != Resp_STK_INSYNC) {
      error1(ERRORPROTOSYNC, cmd_buf[0]);
      return -4;
    }
    
    if (stk500_recv(cmd_buf, 1) < 0) {
      exit(1);
    }
    
    if (cmd_buf[0] != Resp_STK_OK) {
      error1(ERRORNOTOK,cmd_buf[0]);
      return -5;
    }

  return n_bytes;
}



#ifdef LOADVERIFY //maybe sometime? note code needs to be re-written won't work as is



static int stk500_paged_load(AVRMEM * m, int page_size, int n_bytes) {
  
  unsigned char buf[16];
  int memtype;
  unsigned int addr;
  int a_div;
  int tries;
  unsigned int n;
  int block_size;

  memtype = 'F';

  a_div = 1;

  if (n_bytes > m -> size) {
    n_bytes = m -> size;
    n = m -> size;
  }
  
  else {
    if ((n_bytes % page_size) != 0) {
      n = n_bytes + page_size - (n_bytes % page_size);
    }
    else {
      n = n_bytes;
    }
  }

  for (addr = 0; addr < n; addr += page_size) {
    // report_progress (addr, n_bytes, NULL);
    if ((addr + page_size > n_bytes)) {
      block_size = n_bytes % page_size;
  }
  else {
     block_size = page_size;
  }
  
    tries = 0;
    
  retry:
  
    tries++;
    
    stk500_loadaddr(addr / a_div);
    
    buf[0] = Cmnd_STK_READ_PAGE;
    buf[1] = (block_size >> 8) & 0xff;
    buf[2] = block_size & 0xff;
    buf[3] = memtype;
    buf[4] = Sync_CRC_EOP;
    
    stk500_send(buf, 5);

    if (stk500_recv(buf, 1) < 0) {
      exit(1);
    }
    
    if (buf[0] == Resp_STK_NOSYNC) {
      if (tries > 33) {
        error(ERRORNOSYNC);
        return -3;
      }
      if (stk500_getsync() < 0) {
        return -1;
      }
      goto retry;
    }
    else if (buf[0] != Resp_STK_INSYNC) {
      error1(ERRORPROTOSYNC, buf[0]);
      return -4;
    }

    if (stk500_recv(&m -> buf[addr], block_size) < 0)
      exit(1);

    if (stk500_recv(buf, 1) < 0)
      exit(1);

    if (buf[0] != Resp_STK_OK) {
        error1(ERRORPROTOSYNC, buf[0]);
        return -5;
      }
    }
  

  return n_bytes;
}

#endif



/* issue the 'program enable' command to the AVR device */



static int stk500_program_enable() {
  
  unsigned char buf[16];
  int tries=0;

retry:
  
  tries++;

  buf[0] = Cmnd_STK_ENTER_PROGMODE;
  buf[1] = Sync_CRC_EOP;

  stk500_send(buf, 2);
  
  if (stk500_recv(buf, 1) < 0) {
    exit(1);
  }
  
  if (buf[0] == Resp_STK_NOSYNC) {
    if (tries > 33) {
      error(ERRORNOSYNC);
      return -1;
    }
    if (stk500_getsync()< 0)
      return -1;
    goto retry;
  }
  
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC,buf[0]);
    return -1;
  }

  if (stk500_recv(buf, 1) < 0) {
    exit(1);
  }
  
  if (buf[0] == Resp_STK_OK) {
    return 0;
  }
  
  else if (buf[0] == Resp_STK_NODEVICE) {
    error(ERRORNODEVICE);
    return -1;
  }

  if(buf[0] == Resp_STK_FAILED) {
    error(ERRORNOPROGMODE);
    return -1;
  }

  error1(ERRORUNKNOWNRESP,buf[0]);

  return -1;
}



static void stk500_disable() {
  
  unsigned char buf[16];
  int tries=0;

retry:
  
  tries++;

  buf[0] = Cmnd_STK_LEAVE_PROGMODE;
  buf[1] = Sync_CRC_EOP;

  stk500_send(buf, 2);
  
  if (stk500_recv(buf, 1) < 0) {
    exit(1);
  }
  
  if (buf[0] == Resp_STK_NOSYNC) {
    if (tries > 33) {
      error(ERRORNOSYNC);
      return;
    }
    if (stk500_getsync() < 0) {
      return;
    }
    goto retry;
  }
  
  else if (buf[0] != Resp_STK_INSYNC) {
    error1(ERRORPROTOSYNC, buf[0]);
    return;
  }

  if (stk500_recv( buf, 1) < 0) {
    exit(1);
  }
  
  if (buf[0] == Resp_STK_OK) {
    return;
  }
  else if (buf[0] == Resp_STK_NODEVICE) {
    error(ERRORNODEVICE);
    return;
  }

  error1(ERRORUNKNOWNRESP, buf[0]);

  return;
}



// Original avrdude error messages get copied to ram and overflow, wo use numeric codes.
void error1(int errno, unsigned char detail){
  DEBUGP("error: ");
  DEBUGP(errno);
  DEBUGP(" detail: 0x");
  DEBUGPLN(detail,HEX);
}



void error(int errno){
  DEBUGP("error" );
  DEBUGPLN(errno);
}



void dumphex(unsigned char *buf, int len) {
  for (int i = 0; i < len; i++) {
    if (i%16 == 0)
      DEBUGPLN();
      DEBUGP(buf[i],HEX);DEBUGP(" ");
  }
  DEBUGPLN();
}





// OLED работа с русским текстом 
String utf8rus(String source) {
  int i,k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };
  k = source.length(); i = 0;
  while (i < k) {
    n = source[i]; i++;
    if (n >= 0xC0) {
      switch (n) {
        case 0xD0: {
          n = source[i]; i++;
          if (n == 0x81) { n = 0xA8; break; }
          if (n >= 0x90 && n <= 0xBF) n = n + 0x2F;
          break;
        }
        case 0xD1: {
          n = source[i]; i++;
          if (n == 0x91) { n = 0xB7; break; }
          if (n >= 0x80 && n <= 0x8F) n = n + 0x6F;
          break;
        }
      }
    }
    m[0] = n; target = target + String(m);
  }
  return target;
}



// Вывод сообщения на OLED дисплей
void displayOLED(String str, int x, int y, int delayOLED, int language) {
  
  if(x > 128 || x < 0) x = 0;
  if(y > 64 || y < 0) y = 0;
  
  display.clearDisplay(); // Чистим дисплей
  display.setTextSize(1); // Устанавливаем размер шрифта (размер пикселей)
  display.setTextColor(WHITE); // Устанавливаем цвет текста
  
  display.setCursor(x, y); // Устанавливаем курсор  
  
  // Вводим русский текст
  if(language == 1) {
    display.println(utf8rus(str));
  }
  
  // Вводим английский текст
  if(language == 0) {
    display.println(str);
  }

  display.display(); // Устанавливаем изображение
  delay(delayOLED); // Задержка
}
