#include <Arduino.h>
#include <LittleFS.h>
#include "FS.h"

// Upload the binary to the LittleFS:
// https://randomnerdtutorials.com/arduino-ide-2-install-esp32-littlefs/

#include "stm32_bootloader.h"

#define FILE_PATH_NAME                          "/STM32F4.bin"

/****it is tested that the maximum length is 126 for STM32L4 I2C ****/
#if (BOOTLOADER_PORT==BOOTLOADER_I2C)
  #define MAX_WRITE_BLOCK_SIZE                    64   
  #define MAX_READ_BLOCK_SIZE                     16             // this is because of the Arduino IIC buffer len is 32,
#else
  #define MAX_WRITE_BLOCK_SIZE                    256
  #define MAX_READ_BLOCK_SIZE                     256
#endif

File file;           //file to save stm32 firmware

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

#ifndef DUMMY_TEST
void downloadFromHTTP(void);
void wifiConnect(void);
#endif
void flashSTM32(void);
void verifySTM32(void);

pRESULT writeFlash(void);
pRESULT readSlaveFlashandVerify(void);

void setup() {
  Serial.begin(115200);

  platform_init();

  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

}

void loop() {
  listDir(LittleFS, "/", 3);

  File file = LittleFS.open(FILE_PATH_NAME, "r");
  if(!file){
    Serial.println("Failed to open file for reading");
  }
  else
  {
    Serial.print(FILE_PATH_NAME);
    Serial.println(" Opened!");

    checkAndEraseSTM();
    writeFlash();
    readSlaveFlashandVerify();
    endBootloader();
  }
  

  delay(2000);
}


pRESULT writeFlash(void)
{
  uint8_t loadAddress[4] = {0x08, 0x00, 0x00, 0x00};
  uint8_t block[256] = {0};
  int curr_block = 0, bytes_read = 0;

  File file = LittleFS.open(FILE_PATH_NAME);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return RES_FAIL;
  }
  Serial.print(FILE_PATH_NAME);
  Serial.println(" Opened!");

  while ((bytes_read = file.readBytes((char*)block,MAX_WRITE_BLOCK_SIZE)) > 0)
  {
    curr_block++;
    Serial.printf("Slave MCU IAP: Writing block: %d,block size: %d", curr_block,bytes_read);Serial.println("");
  
    pRESULT ret = flashSlavePage(loadAddress, block,bytes_read);
    if (ret == RES_FAIL)
    {
        return RES_FAIL;
    }
  
    memset(block, 0xff, bytes_read);
  }
  file.close();
  
  return RES_OK;
}

pRESULT readSlaveFlashandVerify(void)
{
    uint8_t readAddress[4] = {0x08, 0x00, 0x00, 0x00};
    uint8_t block[257] = {0};
    int curr_block = 0, bytes_read = 0;

    File file = LittleFS.open(FILE_PATH_NAME);
    if (!file || file.isDirectory()) {
      Serial.println("- failed to open file for reading");
      return RES_FAIL;
    }

    while ((bytes_read = file.readBytes((char*)block,MAX_READ_BLOCK_SIZE)) > 0)
    {
        curr_block++;
        Serial.printf("Slave MCU IAP: Reading block: %d, block size: %d", curr_block,bytes_read);Serial.println("");

        pRESULT ret = verifySlavePage(readAddress, block,bytes_read);
        if (ret == RES_FAIL)
        {
            Serial.println("Data verification failed");
            file.close();
            return RES_FAIL;
        }
        memset(block, 0xff, 256);
    }
    file.close();
    Serial.println("Data verification success");
    return RES_OK;
}
