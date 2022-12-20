# SPIFFS_SysLogger
A system Logger Library for the ESP micro controller chips that uses SPIFFS

## setup your code

You need to include "SPIFFS_SysLogger.h"
```
#include "SPIFFS_SysLogger.h"
```

Then you need to create the ESPSL object 
```
ESPSL sysLog;
```
In `setup()` add the following code to create or open a log file of 100 lines, 80 chars/line
```
   SPIFFS.begin();
   .
   .
   if (!sysLog.begin(100, 80)) 
   {
     Serial.println("sysLog.begin() error!");
     delay(10000);
   }
```
Adding an entry to the system log 
```
   sysLog.write("This is a line of text");
```
or
```
   sysLog.writef("This is line [%d] of [%s]", __LINE__, __FUNCTION__);
```
To read the sytem log file you first have to tell the SPIFFS_SysLogger
to start reading:
```
   sysLog.startReading();
```

To read the sytem log file from the jongest to the latest log line
you call the **readNextLine()** method:
```
   char lLine[100] = {0};
   Serial.println("\n=====from oldest to end==============================");
   sysLog.startReading();
   while( sysLog.readNextLine(lLine, sizeof(lLine)) )
   {
     Serial.printf("==>> [%s]\r\n", lLine);
   }
```

To read the sytem log file in reversed order you call the **readPreviousLine()**
method:
```
   char lLine[100] = {0};
   Serial.println("\n=====from end to oldest==============================");
   sysLog.startReading();
   while( sysLog.readPreviousLine(lLine, sizeof(lLine)) )
   {
     Serial.printf("==>> [%s]\r\n", lLine);
   }
```

With a simple macro you can add Debug info to your log-lines
```
  /* example of debug info with time information ----------------------------------------------*/
  #define writeToSysLog(...) ({ sysLog.writeDbg(sysLog.buildD("[%02d:%02d:%02d][%-12.12s] "     \
                                                               , hour(), minute(), second()     \
                                                               , __FUNCTION__)                  \
                                         ,__VA_ARGS__); })
```
or
```
  /* example of debug info with calling function and line in calling function -----------------*/
  #define writeToSysLog(...) ({ sysLog.writeDbg(sysLog.buildD("[%-12.12s(%4d)] "                \
                                                               , __FUNCTION__, __LINE__)        \
                                         ,__VA_ARGS__); })
```
This Logline:
```
     writeToSysLog("Reset Reason [%s]", ESP.getResetReason().c_str());
```
looks like this wth the first **writeToSysLog()** example (the first line):
```
   [12:30:22][setup       ] Reset Reason [External System] 
```
the macro add's the time, and the function-name that called this
**writeToSysLog()** macro
```
   [12:30:22][setup       ] <rest of the log text>
```

## Methods

#### ESPSL::begin(uint16_t depth,  uint16_t lineWidth)
Opens an existing system logfile. If there is no system logfile
or the **depth** or **lineWidth** the logfile was created with are not the same
it will create a logfile with **depth** lines each **lineWidth** chars wide.
<br>
  - The max. **lineWidth** is **150 chars**
  - The min. **lineWidth** is **50 chars**
  - The min. **depth** is **10 lines**
<br>
Return boolean. **true** if succeeded, otherwise **false**


#### ESPSL::begin(uint16_t depth,  uint16_t lineWidth, boolean mode)
if **mode** is **true**:<br>
It will create a new system logfile with **depth** lines each **lineWidth** chars wide.
<br>
if **mode** is **false**:<br>
Opens an existing system logfile. If there is no system logfile
it will create one with **depth** lines each **lineWidth** chars wide.
<br>
Return boolean. **true** if succeeded, otherwise **false**


#### ESPSL::status()
Display some internal var's of the system logfile to **Serial**.


#### ESPSL::setOutput(HardwareSerial *serIn, int baud)
Set the debug output to serIn.


#### ESPSL::setOutput(Stream *serIn)
Set the debug output to serIn.


#### ESPSL::print(const char *line)
Prints the **line** to **serIn**.


#### ESPSL::println(const char *line)
Prints the **line** to **serIn** (and add a '\\n' at the end)


#### ESPSL::printf(const char *fmt, ...)
Prints formatted line to **serIn** 


#### ESPSL::write(const char*)
This method will write a line of text to the system logfile.
<br>
Return boolean. **true** if succeeded, otherwise **false**


#### ESPSL::writef(const char *fmt, ...)
This method will write a formatted line of text to the system logfile.
The syntax is the same as **printf()**.
<br>
Return boolean. **true** if succeeded, otherwise **false**


#### ESPSL::writeDbg(const char *dbg, const char *fmt, ...)
This method will write a formatted line of text to the system logfile but with
a string in front of it. This 'string' can be formatted using ESPSL::build().
The syntax for **\*fmt, ...** is the same as **printf()**.
This method is ment to be used with the **writeToSysLog()** macro.
<br>
Return boolean. **true** if succeeded, otherwise **false**


#### ESPSL::buildD(const char *fmt, ...)
This method will return a formatted line of text.
The syntax for **\*fmt, ..** is the same as **printf()**.
This method is ment to be used to 'feed' the ESPSL:writeDbg() 
method.
<br>
Return char\*. 


#### ESPSL::startReading()
Sets the read pointer to **startLine** and the end pointer to
**startLine** + **numLines**.
<br>
This method should be called before using **readNextLine()** or **readPreviousLine()**.
<br>
Return boolean. **true** if succeeded, otherwise **false**


#### ESPSL::readNextLine()
Reads the next line from the system logfile and advances the read pointer one line.
<br>
Return bool. **true** if more records available, otherwise **false**.


#### ESPSL::readPreviousLine()
Reads the previous line from the system logfile and moves the read pointer one line back.
<br>
Return bool. **true** if more records available, otherwise **false**.


#### ESPSL::dumpLogFile()
This method is for debugging. It display's all the lines in the
system logfile to **Serial**.
<br>
Return boolean. **true** if succeeded, otherwise **false**


#### ESPSL::removeSysLog()
This methos removes a system logfile from SPIFFS.
<br>
Return boolean. **true** if succeeded, otherwise **false**


#### ESPSL::getLastLineID()
Internaly the ESP_SysLogger uses sequential **lineID**'s to uniquely
identify each log line in the file. With this method you can query
the last used **lineID**.
<br>
Return uint32_t. Last used **lineID**.


#### ESPSL::setDebugLvl(int8_t debugLvl)
If **_DODEBUG** is defines in the **ESP_SysLogger.h** file you can use this
method to set the debug level to display specific Debug lines to **Serial**.


... more to come
