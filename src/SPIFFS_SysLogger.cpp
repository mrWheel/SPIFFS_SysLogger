/***************************************************************************
**  Program   : SPIFFS_SysLogger.cpp
**
**  Version   : 2.0.1   (20-12-2022)
**
**  Copyright (c) 2022 .. 2023 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
****************************************************************************
*/

#include "SPIFFS_SysLogger.h"

//-- Constructor
ESPSL::ESPSL() 
{ 
  _Serial   = NULL;
  _serialOn = false;
  _Stream   = NULL;
  _streamOn = false;
}

//-------------------------------------------------------------------------------------
//-- begin object
boolean ESPSL::begin(uint16_t depth, uint16_t lineWidth) 
{
  uint32_t  tmpID, recKey;
  
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::begin(%d, %d)..\n", __LINE__, depth, lineWidth);
#endif

  int16_t fileSize;

  if (lineWidth > _MAXLINEWIDTH) { lineWidth = _MAXLINEWIDTH; }
  if (lineWidth < _MINLINEWIDTH) { lineWidth = _MINLINEWIDTH; }
  memset(globalBuff, 0, (_MAXLINEWIDTH +15));
  
  //-- check if the file exists ---
  if (!SPIFFS.exists(_sysLogFile)) 
  {
    printf("ESPSL(%d)::begin(%d, %d) %s does not exist..\n", __LINE__, depth, lineWidth, _sysLogFile);
    if (create(depth, lineWidth))
    {
      _numLines   = depth;
      _lineWidth  = lineWidth;
    }
  }
  
  //-- check if the file can be opened ---
  _sysLog  = SPIFFS.open(_sysLogFile, "r+");    //-- open for reading and writing
  if (!_sysLog) 
  {
    printf("ESPSL(%d)::begin(): Some error opening [%s] .. bailing out!\r\n", __LINE__, _sysLogFile);
    return false;
  } //-- if (!_sysLog)

  if (_sysLog.available() > 0) 
  {
#ifdef _DODEBUG
    if (_Debug(3)) printf("ESPSL(%d)::begin(): read record [0]\r\n", __LINE__);
#endif
    if (!_sysLog.seek(0, SeekSet)) 
    {
      printf("ESPSL(%d)::begin(): seek to position [%04d] failed (now @%d)\r\n", __LINE__
                                                                               , 0
                                                                               , _sysLog.position());
    }

    int l = _sysLog.readBytesUntil('\n', globalBuff, _MAXLINEWIDTH) -1; 
        //printf("ESPSL(%d)::begin(): rec[0] [%s]\r\n", __LINE__, globalBuff);

#ifdef _DODEBUG
        if (_Debug(4)) printf("ESPSL(%d)::begin(): rec[0] [%s]\r\n", __LINE__, globalBuff);
#endif
        sscanf(globalBuff,"%u|%d;%d;%d;" 
                                , &recKey
                                , &tmpID
                                , &_numLines
                                , &_lineWidth);
     //printf("ESPSL(%d)::begin(): rec[%d] numLines[%d], lineWidth[%d]\r\n", __LINE__
     //                                                                       , recKey
     //                                                                       , _numLines
     //                                                                       , _lineWidth);
    Serial.flush();
    if (_numLines   < _MINNUMLINES)  { _numLines   = _MINNUMLINES; }
    if (_lineWidth  < _MINLINEWIDTH) { _lineWidth  = _MINLINEWIDTH; }
    _recLength = _lineWidth + _KEYLEN;
    //if (tmpID > _lastUsedLineID) { _lastUsedLineID = tmpID; }
#ifdef _DODEBUG
    if (_Debug(4)) printf("ESPSL(%d)::begin(): rec[%u] -> [%8d][%d][%d]\r\n", __LINE__
                                                                                , recKey
                                                                                , _lastUsedLineID
                                                                                , _numLines
                                                                                , _lineWidth);
#endif
  } 
  
  if ((depth != _numLines) || lineWidth != _lineWidth)
  {
    if (_Debug(1)) printf("ESPSL(%d)::begin(): (depth[%d] != numLines[%d]) || (lineWidth[%d] != _lineWidth[%d])\r\n", __LINE__
                                              , depth
                                              , _numLines
                                              , lineWidth
                                              , _lineWidth);
    _sysLog.close();
    removeSysLog();
    create(depth, lineWidth);
    _sysLog  = SPIFFS.open(_sysLogFile, "r+");    //-- open for reading and writing
    if (!_sysLog) 
    {
      printf("ESPSL(%d)::begin(): Some error opening [%s] .. bailing out!\r\n", __LINE__, _sysLogFile);
      return false;
    } //-- if (!_sysLog)

  }
  
  memset(globalBuff, 0, sizeof(globalBuff));
  
  checkSysLogFileSize("begin():", (_numLines + 1) * (_recLength +1));  //-- add '\n'
  
  if (_numLines != depth) 
  {
    printf("ESPSL(%d)::begin(): lines in file (%d) <> %d !!\r\n", __LINE__, _numLines, depth);
  }
  if (_lineWidth != lineWidth)
  {
    printf("ESPSL(%d)::begin(): lineWidth in file (%d) <> %d !!\r\n", __LINE__, _lineWidth, lineWidth);
  }

  init();
  //printf("ESPSL(%d):: after init() -> _lastUsedLineID[%d]\r\n", __LINE__, _lastUsedLineID);

  return true; // We're all setup!
  
} //-- begin()

//-------------------------------------------------------------------------------------
//-- begin object
boolean ESPSL::begin(uint16_t depth, uint16_t lineWidth, boolean mode) 
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%s)::begin(%d, %d, %s)..\n", __LINE__, depth, lineWidth, (mode? "CREATE":"KEEP"));
#endif
  _numLines   = depth;
  _lineWidth  = lineWidth;
  
  if (mode) 
  {
    _sysLog.close();
    removeSysLog();
    create(_numLines, _lineWidth);
  }
  return (begin(depth, lineWidth));
  
} // begin()

//-------------------------------------------------------------------------------------
//-- Create a SysLog file on SPIFFS
boolean ESPSL::create(uint16_t depth, uint16_t lineWidth) 
{
  File createFile;
  
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::create(%d, %d)..\n", __LINE__, depth, lineWidth);
#endif

  int32_t bytesWritten;
  
  _numLines   = depth;
  if (lineWidth > _MAXLINEWIDTH)
          lineWidth  = _MAXLINEWIDTH;
  else if (lineWidth < _MINLINEWIDTH) 
          lineWidth  = _MINLINEWIDTH;
  _lineWidth  = lineWidth;
  
  _recLength  = _lineWidth + _KEYLEN;

  //_nextFree = 0;
  memset(globalBuff, 0, sizeof(globalBuff));  
  //-- check if the file exists and can be opened ---
  createFile  = SPIFFS.open(_sysLogFile, "w");    //-- open for writing
  if (!createFile) 
  {
    printf("ESPSL(%d)::create(): Some error opening [%s] .. bailing out!\r\n", __LINE__, _sysLogFile);
    createFile.close();
    return false;
  } //-- if (!_sysLog)


  snprintf(globalBuff, _lineWidth, "%08d;%d;%d; META DATA SPIFFS_SysLogger", 0, _numLines, _lineWidth);
  fixLineWidth(globalBuff, _lineWidth);
  fixRecLen(globalBuff, 0, _recLength);
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::create(): rec(0) [%s](%d bytes)\r\n", __LINE__, globalBuff, strlen(globalBuff));
#endif
  bytesWritten = createFile.println(globalBuff) -1; //-- skip '\n'
  createFile.flush();
  if (bytesWritten != _recLength) 
  {
    printf("ESPSL(%d)::create(): ERROR!! written [%d] bytes but should have been [%d] for record [0]\r\n"
                                            ,__LINE__ , bytesWritten, _recLength);

    createFile.close();
    return false;
  }
  
  int r;
  for (r=0; r < _numLines; r++) 
  {
    yield();
    snprintf(globalBuff, _lineWidth, "=== empty log regel (%d) ========================================================================================", (r+1));
    fixLineWidth(globalBuff, _lineWidth);
    fixRecLen(globalBuff, (_EMPTYID *1), _recLength);
    //printf("ESPSL(%d)::create(): rec(%d) [%s](%d bytes)\r\n", __LINE__, r, globalBuff, strlen(globalBuff));
    bytesWritten = createFile.println(globalBuff) -1; //-- skip '\n'
    if (bytesWritten != _recLength) 
    {
      printf("ESPSL(%d)::create(): ERROR!! written [%d] bytes but should have been [%d] for record [%d]\r\n"
                                            ,__LINE__ , bytesWritten, _recLength, r);
      createFile.close();
      return false;
    }
    
  } //-- for r ....
  
  createFile.close();
  
  _lastUsedLineID = 1;
  _oldestLineID   = 0;

    return true;
  
} // create()

//-------------------------------------------------------------------------------------
//-- read SysLog file and find next line to write to
boolean ESPSL::init() 
{
  int32_t offset, recKey = 0;
  int32_t readKey;
  char logText[(_lineWidth + _KEYLEN +1)];
  memset(logText, 0, (_lineWidth + _KEYLEN +1));

#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::init()..\r\n", __LINE__);
#endif
      
  if (!_sysLog) 
  {
    printf("ESPSL(%d)::init(): Some error opening [%s] .. bailing out!\r\n", __LINE__, _sysLogFile);
    return false;
  } //-- if (!_sysLog)

  _oldestLineID   = 0;
  _lastUsedLineID = 0;
  recKey          = 0;

  while (_sysLog.available() > 0) 
  {
    recKey++;
    offset = (recKey * (_recLength +1)); 
    if (!_sysLog.seek(offset, SeekSet)) 
    {
      printf("ESPSL(%d)::init(): seek to position [%d/%04d] failed (now @%d)\r\n", __LINE__, recKey
                                                                                  , offset
                                                                                  , _sysLog.position());
      return false;
    }
#ifdef _DODEBUG
    if (_Debug(4)) printf("ESPSL(%d)::init(): -> read record (recKey) [%d/%04d]\r\n", __LINE__, recKey, offset);
#endif
    int l = _sysLog.readBytesUntil('\n', globalBuff, _recLength);
        sscanf(globalBuff,"%u|%[^\0]" , &_oldestLineID, logText);
        if (_oldestLineID > 0)
        {
          if (_oldestLineID >= _lastUsedLineID) { _lastUsedLineID = _oldestLineID; }
          //printf("ESPSL(%d):: init() -> _lastUsedLineID[%d] _oldestLineID[%d]\r\n", __LINE__, _lastUsedLineID, _oldestLineID);
        }
    
#ifdef _DODEBUG
    if (_Debug(4)) printf("ESPSL(%d)::init(): testing readKey[%010u] -> [%08d][%s]\r\n", __LINE__
                                                                                , _oldestLineID
                                                                                , logText);
#endif
  } //-- while ..
  
  if (_lastUsedLineID <= 0) { _lastUsedLineID = 0; }
  _oldestLineID = _lastUsedLineID +1;
  //printf("ESPSL(%d):: init() => _lastUsedLineID[%d] _oldestLineID[%d]\r\n", __LINE__, _lastUsedLineID, _oldestLineID);

  return false;

} // init()


//-------------------------------------------------------------------------------------
boolean ESPSL::write(const char* logLine) 
{
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL(%d)::write(%s)..\r\n", __LINE__, logLine);
#endif

  int32_t   bytesWritten;
  uint16_t  offset, seekToLine;
  int       nextFree;

  //_sysLog  = SPIFFS.open(_sysLogFile, "r+");    //-- open for reading and writing

#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL(%d)::write(): oldest[%8d], last[%8d], nextFree Slot[%3d]\r\n"
                                                      , __LINE__
                                                      , _oldestLineID
                                                      , _lastUsedLineID
                                                      , nextFree);
#endif
  
  char lineBuff[(_recLength + _KEYLEN +1)];
  memset(globalBuff, 0, (_recLength + _KEYLEN + 1));

  snprintf(globalBuff, _lineWidth, "%s", logLine);
  fixLineWidth(globalBuff, _lineWidth); 
  _lastUsedLineID++;
  fixRecLen(globalBuff, _lastUsedLineID, _recLength);
  seekToLine = (_lastUsedLineID % _numLines) +1; //-- always skip rec. 0 (status rec)
  offset = (seekToLine * (_recLength +1));
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL(%d)::write() -> slot[%d], seek[%d/%04d] [%s]\r\n", __LINE__
                                                                                , _lastUsedLineID
                                                                                , seekToLine, offset
                                                                                , lineBuff);
#endif
  if (!_sysLog.seek(offset, SeekSet)) 
  {
    printf("ESPSL(%d)::write(): seek to position [%d/%04d] failed (now @%d)\r\n", __LINE__, seekToLine
                                                                                , offset
                                                                                , _sysLog.position());
    //_sysLog.close();
    return false;
  }
  bytesWritten = _sysLog.println(globalBuff) -1; // don't count '\n'
  _sysLog.flush();
  //_sysLog.close();

  if (bytesWritten != _recLength) 
  {
      printf("ESPSL(%d)::write(): ERROR!! written [%d] bytes but should have been [%d]\r\n"
                                       , __LINE__, bytesWritten, _recLength);
      return false;
  }

  _oldestLineID = _lastUsedLineID +1; //-- 1 after last
  nextFree = (_lastUsedLineID % _numLines) + 1;  //-- always skip rec "0"

  return true;

} // write()


//-------------------------------------------------------------------------------------
boolean ESPSL::writef(const char *fmt, ...) 
{
  char lineBuff[(_MAXLINEWIDTH + 101)];
  memset(lineBuff, 0, (_MAXLINEWIDTH + 101));

#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL(%d)::writef(%s)..\r\n", __LINE__, fmt);
#endif

  va_list args;
  va_start (args, fmt);
  vsnprintf (lineBuff, (_MAXLINEWIDTH +100), fmt, args);
  va_end (args);

  //-- remove control chars
  for(int i=0; ((i<strlen(lineBuff)) && (lineBuff[i]!=0)); i++)
  {
    if ((lineBuff[i] < ' ') || (lineBuff[i] > '~')) { lineBuff[i] = '^'; }
  }

  bool retVal = write(lineBuff);

  return retVal;

} // writef()


//-------------------------------------------------------------------------------------
boolean ESPSL::writeDbg(const char *dbg, const char *fmt, ...) 
{
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL(%d)::writeDbg(%s, %s)..\r\n", __LINE__, dbg, fmt);
#endif

  int       nextFree;
  char dbgStr[(_MAXLINEWIDTH + 101)];
  memset(dbgStr, 0, (_MAXLINEWIDTH + 101));
  char totBuf[(_MAXLINEWIDTH + 101)];
  memset(totBuf, 0, (_MAXLINEWIDTH + 101));

  sprintf(dbgStr, "%s", dbg);
  //printf("ESPSL(%d)::writeDbg([%s], *fmt)..\r\n", __LINE__, dbgStr);
  
  va_list args;
  va_start (args, fmt);
  vsnprintf (totBuf, (_MAXLINEWIDTH +100), fmt, args);
  va_end (args);
  
  while ((strlen(totBuf) > 0) && (strlen(dbgStr) + strlen(totBuf)) > (_lineWidth -1)) 
  {
    totBuf[strlen(totBuf)-1] = '\0';
    yield();
  }
  if ((strlen(dbgStr) + strlen(totBuf)) < _lineWidth) 
  {
    strlcat(dbgStr, totBuf, _lineWidth);
  }

  //-- remove control chars
  for(int i=0; ((i<strlen(dbgStr)) && (dbgStr[i]!=0)); i++)
  {
    if ((dbgStr[i] < ' ') || (dbgStr[i] > '~')) { dbgStr[i] = '^'; }
  }
  //printf("ESPSL(%d)::writeDbg(): dbgStr[%s]..\r\n", __LINE__, dbgStr);
  bool retVal = write(dbgStr);
  
  return retVal;

} // writeDbg()


//-------------------------------------------------------------------------------------
char *ESPSL::buildD(const char *fmt, ...) 
{
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL(%d)::buildD(%s)..\r\n", __LINE__, fmt);
#endif
  memset(globalBuff, 0, sizeof(globalBuff));
  
  va_list args;
  va_start (args, fmt);
  vsnprintf (globalBuff, (_MAXLINEWIDTH), fmt, args);
  va_end (args);

  //-- remove control chars
  for(int i=0; ((i<strlen(globalBuff)) && (globalBuff[i]!=0)); i++)
  {
    if ((globalBuff[i] < ' ') || (globalBuff[i] > '~')) { globalBuff[i] = '^'; }
  }

  return globalBuff;
  
} // buildD()


//-------------------------------------------------------------------------------------
//-- set pointer to startLine
void ESPSL::startReading() 
{
  _readNext         = _lastUsedLineID +1;
  _readNextEnd      = _readNext + _numLines;
  _readPrevious     = _lastUsedLineID;
  _readPreviousEnd  = _readPrevious - _numLines;
  if (_readPreviousEnd < 0) { _readPreviousEnd = 0; }
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::startReading()..next[%d] to [%d]\r\n", __LINE__, _readNext, _readNextEnd);
  if (_Debug(1)) printf("ESPSL(%d)::startReading()..prev[%d] to [%d]\r\n", __LINE__, _readPrevious, _readPreviousEnd);
#endif

} // startReading()

//-------------------------------------------------------------------------------------
//-- start reading from _readNext
bool ESPSL::readNextLine(char *lineOut, int lineOutLen)
{
  uint32_t  offset;
  int32_t   recKey = _readNext;
  int32_t   lineID;
  uint16_t  seekToLine;  
  char      lineIn[_recLength];
  
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::readNextLine(%d)\r\n", __LINE__, _readNext);
#endif
  memset(globalBuff, 0, (_recLength+10));
  memset(lineIn, 0, _recLength);

  if (_readNext >= _readNextEnd) return false;
  
  if (!_sysLog)
  {
    printf("ESPSL(%d)::readNextLine(): _sysLog (%s) not open\r\n", __LINE__, _sysLogFile);
  }
  for(int r=0; r<_numLines; r++)
  {
    seekToLine = ((_readNext +r) % _numLines) +1;
    offset     = (seekToLine * (_recLength +1));
    if (!_sysLog.seek(offset, SeekSet)) 
    {
      printf("ESPSL(%d)::readNextLine(): seek to position [%d/%04d] failed (now @%d)\r\n", __LINE__
                                                                                         , seekToLine
                                                                                         , offset
                                                                                         , _sysLog.position());
      return true;
    }

    int l = _sysLog.readBytesUntil('\n', globalBuff, _recLength);
      sscanf(globalBuff,"%u|%[^\0]", &lineID, lineIn);     

#ifdef _DODEBUG
    if (_Debug(4)) printf("ESPSL(%d)::readNextLine(): [%5d]->recNr[%010d][%10d]-> [%s]\r\n"
                                                            , __LINE__
                                                            , seekToLine
                                                            , _readNext
                                                            , lineID
                                                            , lineIn);
#endif
    if (lineID > -1) { _readNext += r; break; }
    
  } // for ..
    _readNext++;
    if (lineID > (int)_EMPTYID) 
    {
      strlcpy(lineOut, rtrim(lineIn), lineOutLen);
      return true;
#ifdef _DODEBUG
    } else 
    {
      if (_Debug(4)) printf("ESPSL(%d)::readNextLine(): SKIP[%s]\r\n", __LINE__, lineIn);
#endif
    }

  return false;

} //  readNextLine()

//-------------------------------------------------------------------------------------
//-- start reading from _readNext
bool ESPSL::readPreviousLine(char *lineOut, int lineOutLen)
{
  uint32_t  offset;
  int32_t   recKey = _readNext;
  int32_t   lineID;
  uint16_t  seekToLine;  
  //File      tmpFile;
  char      lineIn[_recLength];
  
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::readPreviousLine(%d/%d)\r\n", __LINE__, _readPrevious, _readPreviousEnd);
#endif
  memset(globalBuff, 0, (_recLength+10));
  memset(lineIn, 0, _recLength);

  if (_readPrevious <= _readPreviousEnd) return false;
  
  if (!_sysLog)
  {
    printf("ESPSL(%d)::readPreviousLine(): _sysLog (%s) not open\r\n", __LINE__, _sysLogFile);
  }
  for(int r=0; r<_numLines; r++)
  {
    _readPrevious -= r;
    if (_readPrevious < 0) return false;
    seekToLine = (_readPrevious % _numLines) +1;
    offset     = (seekToLine * (_recLength +1));
    if (!_sysLog.seek(offset, SeekSet)) 
    {
      printf("ESPSL(%d)::readPreviousLine(): seek to position [%d/%04d] failed (now @%d)\r\n", __LINE__
                                                                                         , seekToLine
                                                                                         , offset
                                                                                         , _sysLog.position());
      return true;
    }

    int l = _sysLog.readBytesUntil('\n', globalBuff, _recLength);
        sscanf(globalBuff,"%u|%[^\0]", &lineID, lineIn);     

#ifdef _DODEBUG
    if (_Debug(4)) printf("ESPSL(%d)::readPreviousLine(): [%5d]->recNr[%010d][%10d]-> [%s]\r\n"
                                                            , __LINE__
                                                            , seekToLine
                                                            , _readPrevious
                                                            , lineID
                                                            , lineIn);
#endif
    if (lineID > -1) { break; }
    
  } // for ..
    
  _readPrevious--;
  if (lineID > (int)_EMPTYID) 
  {
    strlcpy(lineOut, rtrim(lineIn), lineOutLen);
    return true;
#ifdef _DODEBUG
  } else 
  {
    if (_Debug(4)) printf("ESPSL(%d)::readPreviousLine(): SKIP[%s]\r\n", __LINE__, lineIn);
#endif
  }

  return false;

} //  readPreviousLine()

//-------------------------------------------------------------------------------------
//-- start reading from startLine
bool ESPSL::dumpLogFile() 
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::dumpLogFile()..\r\n", __LINE__);
#endif

  int32_t   recKey;
  uint32_t  offset, seekToLine;
  memset(globalBuff, 0, sizeof(globalBuff));
      
  _sysLog  = SPIFFS.open(_sysLogFile, "r+");    //-- open for reading and writing

  checkSysLogFileSize("dumpLogFile():", (_numLines + 1) * (_recLength +1));  //-- add '\n'

  for (recKey = 0; recKey < _numLines; recKey++) 
  {
    seekToLine = (recKey % _numLines)+1;
    offset = (seekToLine * (_recLength +1));
    if (!_sysLog.seek(offset, SeekSet)) 
    {
      printf("ESPSL(%d)::dumpLogFile(): seek to position [%d] (offset %d) failed (now @%d)\r\n", __LINE__
                                                                                              , seekToLine
                                                                                              , offset
                                                                                              , _sysLog.position());
      return false;
    }
    uint32_t  lineID;
    int l = _sysLog.readBytesUntil('\n', globalBuff, _recLength);
#ifdef _DODEBUG
    if (_Debug(5)) printf("ESPSL(%d)::dumpLogFile():  >>>>> [%d] -> [%s]\r\n", __LINE__, l, globalBuff);
#endif
    sscanf(globalBuff,"%u|%[^\0]", &lineID, globalBuff);

    if (lineID == (_lastUsedLineID)) 
    {
            printf("(a)dumpLogFile(%d):: seek[%4d/%04d]ID[%8d]->[%s]\r\n", __LINE__, seekToLine, offset, lineID, globalBuff);

            println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    }
    else if (lineID == (_oldestLineID) ) //&& recKey < _numLines)
            printf("(b)dumpLogFile(%d):: seek[%4d/%04d]ID[%8d]->[%s]\r\n", __LINE__, seekToLine, offset, lineID, globalBuff);
    else if (lineID == _EMPTYID) 
            printf("(c)dumpLogFile(%d):: seek[%4d/%04d]ID[%8d]->[%s]\r\n", __LINE__, seekToLine, offset, lineID, globalBuff);
    else 
    {
       printf("(d)dumpLogFile(%d):: seek[%4d/%04d]ID[%8d]->[%s]\r\n", __LINE__, seekToLine, offset, lineID, globalBuff);
    }
  } //-- while ..

  return true;

} // dumpLogFile

//-------------------------------------------------------------------------------------
//-- erase SysLog file from SPIFFS
boolean ESPSL::removeSysLog() 
{
#ifdef _DODEBUG
  if (_Debug(1)) printf("ESPSL(%d)::removeSysLog()..\r\n", __LINE__);
#endif
  SPIFFS.remove(_sysLogFile);
  return true;
  
} // removeSysLog()

//-------------------------------------------------------------------------------------
//-- returns ESPSL status info
void ESPSL::status() 
{
  printf("ESPSL::status():       _numLines[%8d]\r\n", _numLines);
  printf("ESPSL::status():      _lineWidth[%8d]\r\n", _lineWidth);
  if (_numLines > 0) 
  {
    printf("ESPSL::status():   _oldestLineID[%8d] (%2d)\r\n", _oldestLineID
                                                           , (_oldestLineID % _numLines)+1);
    printf("ESPSL::status(): _lastUsedLineID[%8d] (%2d)\r\n", _lastUsedLineID
                                                           , (_lastUsedLineID % _numLines)+1);
  }
  printf("ESPSL::status():       _debugLvl[%8d]\r\n", _debugLvl);
  
} // status()

//-------------------------------------------------------------------------------------
void ESPSL::setOutput(HardwareSerial *serIn, int baud)
{
  _Serial = serIn;
  _Serial->begin(baud);
  _Serial->printf("ESPSL(%d): Serial Output Ready..\r\n", __LINE__);
  _serialOn = true;
  
} // setOutput(hw, baud)

//-------------------------------------------------------------------------------------
void ESPSL::setOutput(Stream *serIn)
{
  _Stream = serIn;
  _Stream->printf("ESPSL(%d): Stream Output Ready..\r\n", __LINE__);
  _streamOn = true;
  
} // setOutput(Stream)

//-------------------------------------------------------------------------------------
void ESPSL::print(const char *line)
{
  if (_streamOn)  _Stream->print(line);
  if (_serialOn)  _Serial->print(line);
  
} // print()

//-------------------------------------------------------------------------------------
void ESPSL::println(const char *line)
{
  if (_streamOn)  _Stream->println(line);
  if (_serialOn)  _Serial->println(line);
  
} // println()

//-------------------------------------------------------------------------------------
void ESPSL::printf(const char *fmt, ...)
{
  char lineBuff[(_MAXLINEWIDTH + 101)];
  memset(lineBuff, 0, (_MAXLINEWIDTH + 101));

  va_list args;
  va_start (args, fmt);
  vsnprintf(lineBuff, (_MAXLINEWIDTH +100), fmt, args);
  va_end (args);

  if (_streamOn)  _Stream->print(lineBuff);
  if (_serialOn)  _Serial->print(lineBuff);

} // printf()

//-------------------------------------------------------------------------------------
void ESPSL::flush()
{
  if (_streamOn)  _Stream->flush();
  if (_serialOn)  _Serial->flush();
  
} // flush()
  
//-------------------------------------------------------------------------------------
//-- check fileSize of _sysLogFile
boolean ESPSL::checkSysLogFileSize(const char* func, int32_t cSize)
{
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL(%d)::checkSysLogFileSize(%d)..\r\n", __LINE__, cSize);
#endif
  int32_t fileSize = sysLogFileSize();
  if (fileSize != cSize) 
  {
    printf("ESPSL(%d)::%s -> [%s] size is [%d] but should be [%d] .. error!\r\n"
                                                          , __LINE__
                                                          , func
                                                          , _sysLogFile
                                                          , fileSize, cSize);
    return false;
  }
  return true;

} // checkSysLogSize()


//-------------------------------------------------------------------------------------
//-- returns lastUsed LineID
uint32_t ESPSL::getLastLineID()
{
  return _lastUsedLineID;
  
} // getLastLineID()

//-------------------------------------------------------------------------------------
//-- set Debug Level
void ESPSL::setDebugLvl(int8_t debugLvl)
{
  if (debugLvl >= 0 && debugLvl <= 9)
        _debugLvl = debugLvl;
  else  _debugLvl = 1;
  
} // setDebugLvl

//-------------------------------------------------------------------------------------
//-- returns debugLvl
int8_t ESPSL::getDebugLvl()
{
  return _debugLvl;
  
} // getDebugLvl


//-------------------------------------------------------------------------------------
//-- returns debugLvl
boolean ESPSL::_Debug(int8_t Lvl)
{
  if (getDebugLvl() > 0 && Lvl <= getDebugLvl()) 
        return true;
  else  return false;
  
} // _Debug()


//===========================================================================================
const char* ESPSL::rtrim(char *aChr)
{
  for(int p=strlen(aChr); p>0; p--)
  {
    if (aChr[p] < '!' || aChr[p] > '~') 
    {
      aChr[p] = '\0';
    }
    else
      break;
  }
  return aChr;
} // rtrim()

//===========================================================================================
void ESPSL::fixLineWidth(char *inLine, int lineLen) 
{
  int16_t newLen;
  
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL(%d)::fixLineWidth([%s], %d) ..\r\n", __LINE__, inLine, lineLen);
#endif
  char fixLine[(lineLen+1)];
  memset(fixLine, 0, (lineLen+1));

  //printf("ESPSL(%d)::fixLineWidth(): inLine[%s]\r\n", __LINE__, inLine);

  snprintf(fixLine, lineLen, "%s", inLine);
  //printf("ESPSL(%d)::fixLineWidth(): fixLine[%s]\r\n", __LINE__, fixLine);

#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL(%d):: [%s]\r\n", __LINE__, fixLine);
#endif
  //-- first: remove control chars
  for(int i=0; i<strlen(fixLine); i++)
  {
    if ((fixLine[i] < ' ') || (fixLine[i] > '~')) { fixLine[i] = '^'; }
  }
  //-- add spaces at the end
  do {
      strlcat(fixLine, "     ", lineLen);
      newLen = strlen(fixLine);
  } while(newLen < (lineLen-1));
  //printf("ESPSL(%d)::fixLineWidth(): fixLine[%s]\r\n", __LINE__, fixLine);
  
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL(%d):: [%s]\r\n", __LINE__, fixLine);
#endif

  memset(inLine, 0, lineLen);
  strlcpy(inLine, fixLine, lineLen);
  //printf("ESPSL(%d):: in[%s] -> out[%s] (len[%d])\r\n", __LINE__, fixLine, inLine, lineLen);
  
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL(%d)::fixLineWidth(): Length of inLine is [%d] bytes\r\n", __LINE__, strlen(inLine));
#endif
  //printf("ESPSL(%d)::fixLineWidth(): inLine[%s]\r\n", __LINE__, inLine);
  
} // fixLineWidth()

//===========================================================================================
void ESPSL::fixRecLen(char *recIn, int32_t recKey, int recLen) 
{
  int newLen;
  
#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL(%d)::fixRecLen([%s], %d, %d) ..\r\n", __LINE__, recIn, recKey, recLen);
#endif
  char recOut[(recLen+1)];
  memset(recOut, 0, (recLen+1));

  snprintf(recOut, recLen, "%010d|%s", recKey, recIn);
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL(%d):: [%s]\r\n", __LINE__, recOut);
#endif
  //-- add spaces at the end
  do {
      strlcat(recOut, "     ", recLen);
      newLen = strlen(recOut);
  } while(newLen < (recLen-1));
  
#ifdef _DODEBUG
  if (_Debug(3)) printf("ESPSL(%d):: [%d]->[%-20.20s]\r\n", __LINE__, recKey, recOut);
#endif

  strlcpy(recIn, recOut, recLen);

#ifdef _DODEBUG
  if (_Debug(4)) printf("ESPSL(%d)::fixRecLen(): Length of record is [%d] bytes\r\n", __LINE__, strlen(recIn));
#endif
  
  //printf("ESPSL(%d):: record(%010d)->[%s]\r\n", __LINE__, recKey, recIn);
  
} // fixRecLen()

//===========================================================================================
int32_t  ESPSL::sysLogFileSize()
{
#if defined(ESP8266)
  {
    Dir dir = SPIFFS.openDir("/");         // List files on SPIFFS
    while (dir.next())  {
          yield();
          String fileName = dir.fileName();
          size_t fileSize = dir.fileSize();
#ifdef _DODEBUG
          if (_Debug(6)) printf("ESPSL)%d)::sysLogFileSize(): found: %s, size: %d\r\n"
                                                                             , __LINE__
                                                                             , fileName.c_str()
                                                                             , fileSize);
#endif
          if (fileName == _sysLogFile) {
#ifdef _DODEBUG
            if (_Debug(6)) printf("ESPSL(%d)::sysLogFileSize(): fileSize[%d]\r\n", __LINE__, fileSize);
#endif
            return (int32_t)fileSize;
          }
    }
  return 0;
  }
#else
  {
      File root = SPIFFS.open("/");
      File file = root.openNextFile();
      while(file)
      {
          String tmpS = file.name();
          String fileName = "/"+tmpS;
          size_t fileSize = file.size();
#ifdef _DODEBUG
          if (_Debug(4)) printf("ESPSL(%d)::FS Found: %s, size: %d\n", __LINE__, fileName.c_str(), fileSize);
#endif
          if (fileName == _sysLogFile) 
          {
#ifdef _DODEBUG
            if (_Debug(4)) printf("ESPSL(%d)::sysLogFileSize(): fileSize[%d]\r\n", __LINE__, fileSize);
#endif
            printf("ESPSL(%d)::sysLogFileSize(): file is still [%d]bytes\r\n", __LINE__, (int32_t)fileSize);
            return (int32_t)fileSize;
          }
          file = root.openNextFile();
      }
      return 0;
  }
#endif

} // sysLogFileSize()


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
