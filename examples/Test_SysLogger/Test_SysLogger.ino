/*
**  Program   : ESP_SysLogger
*/
#define _FW_VERSION "v2.0.1 (20-12-2022)"
/*
**  Copyright (c) 2019 .. 2023 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************/

#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time

#include "SPIFFS_SysLogger.h"
#define _FSYS SPIFFS

ESPSL sysLog;                   // Create instance of the ESPSL object

/*
** you can add your own debug information to the log text simply by
** defining a macro using ESPSL::writeDbg( ESPSL::buildD(..) , ..)
*/
#if defined(_Time_h)
/* example of debug info with time information */
  #define writeToSysLog(...) ({ sysLog.writeDbg( sysLog.buildD("(%4d)[%02d:%02d:%02d][%-12.12s] " \
                                                               , number++                         \
                                                               , hour(), minute(), second()       \
                                                               , __FUNCTION__)                    \
                                                ,__VA_ARGS__); })
#else
/* example of debug info with calling function and line in calling function */
  #define writeToSysLog(...) ({ sysLog.writeDbg( sysLog.buildD("(%4d)[%-12.12s(%4d)] "          \
                                                               , number++                       \
                                                               , __FUNCTION__, __LINE__)        \
                                                ,__VA_ARGS__); })
#endif

#if defined(ESP32) 
  #define LED_BUILTIN 2
#endif

uint32_t  statusTimer;
uint32_t  number = 0;

//-------------------------------------------------------------------------
void listFileSys(bool doSysLog)
{
  Serial.println("===============================================================");
#if defined(ESP8266)
  {
  Dir dir = _FSYS.openDir("/");         // List files on _FSYS
  while (dir.next())  
  {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("FS File: %s, size: %d\n", fileName.c_str(), fileSize);
    if (doSysLog)
    {
      writeToSysLog("FS File: %s, size: %d\r\n", fileName.c_str(), fileSize);
    }
  }
}
#else
{
  File root = _FSYS.open("/");
  File file = root.openNextFile();
  while(file)
  {
    String fileName = file.name();
    size_t fileSize = file.size();
    Serial.printf("FS File: %s, size: %d\r\n", fileName.c_str(), fileSize);
    if (doSysLog)
    {
      writeToSysLog("FS File: %s, size: %d\r\n", fileName.c_str(), fileSize);
    }
    file = root.openNextFile();
  }
}
#endif
  Serial.println("===============================================================");
  
} // listFileSys()


//-------------------------------------------------------------------------
void showBareLogFile() 
{
  Serial.println("\n=====================================================");
  //writeToSysLog("Dump logFile [sysLog.dumpLogFile()]");
  sysLog.dumpLogFile();

} // showBareLogFile()


//-------------------------------------------------------------------------
void testReadnext() 
{
  char  lLine[200] = {0};
  int   lineCount;
  
  Serial.println("testing startReading() & readNextLine() functions...");
  
  Serial.println("\n=====from oldest for n lines=========================");
  Serial.println("sysLog.startReading()");
  sysLog.startReading();
  Serial.println("sysLog.readNextLine()");
  lineCount = 0;
  while( sysLog.readNextLine(lLine, sizeof(lLine)) )
  {
    Serial.printf("[%3d]==>> %s \r\n", ++lineCount, lLine);
  }
  Serial.println("\n=====from newest for n lines=========================");
  Serial.println("sysLog.startReading()");
  sysLog.startReading();
  Serial.println("sysLog.readPreviousLine()");
  lineCount = 0;
  while( sysLog.readPreviousLine(lLine, sizeof(lLine)) )
  {
    Serial.printf("[%3d]==>> %s \r\n", ++lineCount, lLine);
  }
  
  Serial.println("\nDone testing readNext()\n");

} // testReadNext()


//-------------------------------------------------------------------------
void dumpSysLog() 
{
  int seekVal = 0, len = 0;
  char cIn;
  
  File sl = _FSYS.open("/sysLog.dat", "r");
  Serial.println("\r\n===charDump=============================");
  Serial.printf("%4d [", 0);
  while(sl.available())
  {
    cIn = (char)sl.read();
    len++;
    seekVal++;
    if (cIn == '\n') 
    {
      Serial.printf("] (len [%d])\r\n", len);
      len = 0;
      Serial.printf("%4d [", seekVal);
    }
    else  Serial.print(cIn);
  }
  sl.close();
  Serial.println("\r\n========================================\r\n");

} //  dumpSysLog()

//-------------------------------------------------------------------------
void setup() 
{
  Serial.begin(115200);
#if defined(ESP8266)
  Serial.println("\nESP8266: Start ESP System Logger ....\n");
#elif defined(ESP32)
  Serial.println("\nESP32: Start ESP System Logger ....\n");
#else
  Serial.println("\nDon't know: Start ESP System Logger ....\n");
#endif

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

#if !defined(ESP32)
  Serial.println(ESP.getResetReason());
  if (   ESP.getResetReason() == "Exception" 
      || ESP.getResetReason() == "Software Watchdog"
      || ESP.getResetReason() == "Soft WDT reset"
      ) 
  {
    while (1) 
    {
      delay(500);
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
#endif

#if defined(ESP8266)
  {
    _FSYS.begin();
  }
#else
  {
    _FSYS.begin(true);
  }
#endif
  sysLog.setOutput(&Serial, 115200);
  sysLog.setDebugLvl(1);

  //--> max linesize is declared by _MAXLINEWIDTH in the
  //    library and is set @150, so 160 will be truncated to 150!
  //--> min linesize is set to @50
  //-----------------------------------------------------------------
  // use existing sysLog file or create one
  //sysLog.removeSysLog();
  if (!sysLog.begin(27, 60))     
  {   
    Serial.println("Error opening sysLog!\r\nCreated sysLog!");
    delay(5000);
  }
  sysLog.setDebugLvl(0);
  sysLog.status();
  testReadnext();

  int strtId = sysLog.getLastLineID();
  Serial.println("Fill with 10 lines ..");
  for(number=0; number<10; number++)
  {
    sysLog.writef("-----[ %07d ]------[ %04d ]-----------------------------", strtId, (number+strtId));
    //writeToSysLog("Just Started [%d]", (sysLog.getLastLineID() +1));
  }

  sysLog.setDebugLvl(0);
  sysLog.status();
  
  showBareLogFile();

  Serial.println("\nsetup() done .. \n");
  delay(1000);
  
} // setup()


//-------------------------------------------------------------------------
void loop() 
{
  sysLog.status();

  dumpSysLog();

  testReadnext();
  delay(10000);

  int strtId = sysLog.getLastLineID();
  Serial.printf("Fill from [%d] with 5 more lines ..\r\n", strtId);
  for(number=0; number<5; number++)
  {
    sysLog.writef("-----[ %07d ]------[ %04d ]-----------------------------", strtId, (number+strtId));
    //writeToSysLog("Just Started [%d]", (sysLog.getLastLineID() +1));
  }

} // loop()

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
