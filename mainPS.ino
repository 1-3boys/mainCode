#include <Arduino_FreeRTOS.h>
#include <Keypad.h>
#include <semphr.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h> 

 
MFRC522 rfid(53, 9);

MFRC522::MIFARE_Key rfidKey; 

SemaphoreHandle_t xSerialSemaphore;
Servo lockServo;              


byte nuidPICC[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
byte readbackblock[18];
byte passwordLength = 0;

const byte ROWS = 4;
const byte COLS = 4;
byte keys [ROWS][COLS] = {
{13 , 12 , 11 , 10},
{200 , 9 , 6 , 3},
{0 , 8 , 5 , 2},
{100 , 7 , 4 , 1},
};
byte rowPins[ROWS] = {22, 24, 26, 28};
byte colPins[COLS] = {30, 32, 34, 36};
int frequency[8] = {262, 294, 330, 349, 392, 440, 494, 523};

bool open=false;
bool keySet=true;
bool flag=true;
bool rfidKeySet=true;
bool oneTime=false;


int block=2;
int inputCount=0;
int speaker = 40;

void keyPadInput( void *pvParameters );
void rfIdInput( void *pvParameters );
void servoControl( void *pvParameters );

Keypad save = Keypad (makeKeymap(keys), rowPins, colPins, ROWS, COLS);


void setup() {

  Serial.begin(115200);

  pinMode(speaker, OUTPUT);

  open=false;
  keySet=true;
  rfidKeySet=true;

  lockServo.attach(A7);
  lockServo.write(95);

  SPI.begin();
  rfid.PCD_Init();
  for (byte i = 0; i < 6; i++) {
    rfidKey.keyByte[i] = 0xFF;
  }

  if ( xSerialSemaphore == NULL ) 
  {
    xSerialSemaphore = xSemaphoreCreateMutex();
    if ( ( xSerialSemaphore ) != NULL )
      xSemaphoreGive( ( xSerialSemaphore ) );
  }
  
  xTaskCreate(
   keyPadInput
   ,  "keyPadInput"
   ,  128  
   ,  NULL
   ,  2
   ,  NULL );

   xTaskCreate(
   rfIdInput
   ,  "rfIdInput"
   ,  128  
   ,  NULL
   ,  1
   ,  NULL );

   xTaskCreate(
   servoControl
   ,  "servoControl"
   ,  128  
   ,  NULL
   ,  3
   ,  NULL );

  start();
}

void loop() {
}
void writeBlock(int blockNumber) {
  int largestModulo4Number=blockNumber/4*4;
  int trailerBlock=largestModulo4Number+3;
   
  byte status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &rfidKey, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) {
        if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println("bad");
         xSemaphoreGive( xSerialSemaphore );
      }
  }else{
    status = rfid.MIFARE_Write(blockNumber, nuidPICC, 16);
    if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println("good");
         xSemaphoreGive( xSerialSemaphore );
      }
  }
}

void readBlock(int blockNumber) {
  int largestModulo4Number=blockNumber/4*4;
  int trailerBlock=largestModulo4Number+3;
 
  byte status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &rfidKey, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) {
        if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println("bad");
         xSemaphoreGive( xSerialSemaphore );
      }
  }else{
    byte buffersize = 18;
    status = rfid.MIFARE_Read(blockNumber, readbackblock, &buffersize);
    if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println("good");
         xSemaphoreGive( xSerialSemaphore );
      }
  if (status != MFRC522::STATUS_OK) {
          if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println("bad");
         xSemaphoreGive( xSerialSemaphore );
      }
  }
  }
}

void start(){
  while(true){
    byte key = save.getKey();
    if(key){
      Serial.println(key);
      if(key==100 || key==200){
        keySet=false;
        break;
      }
      tone(speaker,frequency[2],100);
      nuidPICC[passwordLength++]=key;
    }
  }
}

void keyPadInput(void *pvParameters){
  while(true){
    if(keySet)
      continue;
    byte key = save.getKey();
    if(key){
       if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println(key);
         xSemaphoreGive( xSerialSemaphore );
      }
      if(key==100 || key==200){
        if(inputCount==passwordLength && flag){
          if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
            Serial.println("open");
            xSemaphoreGive( xSerialSemaphore );
          }
          tone(speaker,349,200);
          delay(200);                                                                                          
          tone(speaker,440,200);
          delay(200);
          tone(speaker,555,200);
          open=true;
          delay(1000);
          open=false;
        }else{
          tone(speaker,frequency[6],1000);
          if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println("fail");
          xSemaphoreGive( xSerialSemaphore );
          }
          delay(1000);
        }
        inputCount=0;
        flag=true;
      }else{
      tone(speaker,frequency[2],100);
        if(key!=nuidPICC[inputCount++]) flag=false;
      }
    }
    vTaskDelay(1);
  }
  vTaskDelete( NULL );
}

void rfIdInput(void *pvParameters){
  while(true){
    if(keySet)
      continue;
    if ( ! rfid.PICC_IsNewCardPresent()){
      continue;
    } // 카드 접촉이 있는가?
      
    if ( ! rfid.PICC_ReadCardSerial()){
      continue;
    }

    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
      continue;
    }
    if(rfidKeySet){
      if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println("write start");
          xSemaphoreGive( xSerialSemaphore );
      }
      writeBlock(2);
      if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.println("write end");
          xSemaphoreGive( xSerialSemaphore );
          }
      rfidKeySet=false;
    }else{
      readBlock(2);
    open=true;
    for (int i=0 ; i<16 ; i++){
      if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
          Serial.print( nuidPICC[i]);
          Serial.println(": ori");
          Serial.print(readbackblock[i]);
          Serial.println(": card");
         xSemaphoreGive( xSerialSemaphore );
      }
     if(nuidPICC[i]!=readbackblock[i]) open=false;
    }
    if(open){
      if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
      Serial.println("open");
      xSemaphoreGive( xSerialSemaphore );
      }
      tone(speaker,349,200);
      delay(200);
      tone(speaker,440,200);
      delay(200);
      tone(speaker,555,200);
    }else{
      if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){
      Serial.println("fail");
      xSemaphoreGive( xSerialSemaphore );
      }
      tone(speaker,frequency[6],1000);
    }
    delay(1000);
    open=false;
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    vTaskDelay(5);
  }
  vTaskDelete( NULL );
}

void servoControl(void *pvParameters){
  while(true){
    if(keySet)
      continue;
    if(oneTime){
      lockServo.write(200);
      vTaskDelay(20);
      lockServo.write(95);
      oneTime=false;
    }
    if(open){
      lockServo.write(5);
      oneTime=true;
      delay(1000);
    }
    vTaskDelay(5);
  } 
  vTaskDelete( NULL );
}


