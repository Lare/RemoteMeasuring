#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Ethernet2.h> //https://github.com/adafruit/Ethernet2 (for use with older etherneshield use standard ethernet library. For some reason old ethernet shield does not work properly)
#include <Timezone.h>    //https://github.com/JChristensen/Timezone
#include <TimeLib.h>      //Required by Timezone, installed from standard ide libraries (Time by Michael Margolis v.1.5.0)
#include <SD.h>
#include <Wire.h>
#include <DS1307RTC.h>

// *Global configuration*
// Prepare sdcard, if 1 clearing all related stuff from sdcard
int prepareSDcard = 0;

// Environment: 0 = production, 1 = Testing
int environment = 1;

// Global variable for Url where to put data
char pageName[40];

// Global Variable for data to send in rest API
char restData[64];

// Global variable for timestamp
unsigned long time = 0;

// Global variable to store if DHCP or Static IP is used
int staticIP = 0;

// Web server for getting time

// Store current millis and set interval to avoid delay usage
unsigned long sensorpreviousMillis = 0; // last sensor time update
long sensorinterval = 15000; // interval at which to do sensor readings (milliseconds)
//long sensorinterval = 300000; // interval at which to do sensor readings (milliseconds)
unsigned long timecheckpreviousMillis = 0; // last ntp time update
unsigned long timecheckinterval = 3600000; // interval at which to do time sync (milliseconds)
//unsigned long timecheckinterval = 5000; // interval at which to do time sync (milliseconds)

// *ONEWIRE*
// Data wire is plugged into pin 22 on the Arduino
#define ONE_WIRE_BUS 22

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

/*
// *************Production environment-specific settings, comment out in testing environment***********

// Sensor addresses. Make sure that sensors are in desired order. First address is for sensor1, second for sensor2 and so on.
DeviceAddress temp[]=
  {
    { 0x10, 0x75, 0xD3, 0x3B, 0x02, 0x08, 0x00, 0x7D },
    { 0x10, 0x05, 0xC0, 0x3B, 0x02, 0x08, 0x00, 0xD7 },
    { 0x28, 0xB6, 0x2F, 0xF2, 0x04, 0x00, 0x00, 0xD4 },
    { 0x10, 0x05, 0xC0, 0x3B, 0x02, 0x08, 0x00, 0xD7 },
    { 0x10, 0x05, 0xC0, 0x3B, 0x02, 0x08, 0x00, 0xD7 },
    { 0x28, 0xDE, 0x1D, 0x81, 0x07, 0x00, 0x00, 0x75 }
  };
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address when searching devices.

// Determine how many sensors is configured in the system
int numberOfConfiguredSensors = sizeof(temp)/sizeof(temp[0]);

// *Ethernet*
// Enter a MAC address for your controller below.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x10, 0x22, 0x8F };
// IP address in case DHCP fails
IPAddress ip(192, 168, 2, 239);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 2, 1);
IPAddress dnServer(193, 210, 18, 18);

// Host for send data (Ip-address or domain name)
char serverName[] = "rest.example.com";

// ***************** End of production environment settings *****************
*/

// *************Testing environment-specific settings, comment out in production environment***********
// Sensor addresses make sure that sensors are in desired order. First address is for sensor1, second for sensor2 and so on.
DeviceAddress temp[]=
  {
    { 0x28, 0xDE, 0x1D, 0x81, 0x07, 0x00, 0x00, 0x75 }    
  //  { 0x10, 0xD6, 0xBF, 0x3B, 0x02, 0x08, 0x00, 0x48 },  
  //  { 0x28, 0x46, 0xE7, 0xBB, 0x02, 0x00, 0x00, 0x18 }
  };
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address when searching devices.

// Determine how many sensors is configured in the system
int numberOfConfiguredSensors = sizeof(temp)/sizeof(temp[0]);

// *Ethernet*
// Enter a MAC address for your controller below.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x10, 0x91, 0xC7 };

// IP address in case DHCP fails
IPAddress ip(192, 168, 2, 240);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 2, 1);
IPAddress dnServer(193, 210, 18, 18);

// Host for send data (Ip-address or domain name)
char serverName[] = "resttest.example.com";

// ***************** End of testing environment settings *****************

// Port to send data
int serverPort = 80;

// Web server where to get time from
char timeServer[] = "www.example.com";

// *Rest client settings*
EthernetClient clientrest;

// *SD*
//  * SD card attached to SPI bus as follows:
// ** MOSI - pin 11 on Arduino Uno/Duemilanove/Diecimila
// ** MISO - pin 12 on Arduino Uno/Duemilanove/Diecimila
// ** CLK - pin 13 on Arduino Uno/Duemilanove/Diecimila
// ** CS - depends on your SD card shield or module.
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8

const int chipSelect = 4;
#define SS_SD_CARD 4
#define SS_ETHERNET 53

// *Timezone*
//US Eastern Time Zone (Helsinki)
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, 180};    //Daylight time = UTC + 3 hours
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, 120};     //Standard time = UTC + 2 hours
Timezone myTZ(myDST, mySTD);

//If TimeChangeRules are already stored in EEPROM, comment out the three
//lines above and uncomment the line below.
//Timezone myTZ(100);       //assumes rules stored at EEPROM address 100

TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev
time_t utc, local;

// *Ethernet client for getting time*
EthernetClient clienttime;

void setup() {
   
  // Start Serial
  Serial.begin(115200);
  
  Serial.println("Starting temperature measurement system version 0.1 ...");
  if (environment == 0) {
    // Because of problems on booting when power outage occured (may be problem with slow router startup) added a delay
    Serial.println("Waiting 10 seconds for router to get ready in case of complete power outage");
    delay(10000);
  }
  
  // Display number of configured sensors in system
  Serial.println();
  Serial.print("Number of configured sensors in system: ");
  Serial.println(numberOfConfiguredSensors);
  
  // Display all onewire sensor addresses that are really found
  Serial.println();
  displayOnewireDevices();
  Serial.println();

  // Set SPI-bus SS-things. By default Ethernet is always selected
  // Other devices are selected when needed.
  pinMode(SS_SD_CARD, OUTPUT);
  pinMode(SS_ETHERNET, OUTPUT);
  digitalWrite(SS_SD_CARD, HIGH);  // SD Card not active
  digitalWrite(SS_ETHERNET, HIGH); // Ethernet not active

  // To use ethernet or SDcard
  pinMode(SS_SD_CARD, OUTPUT);
  pinMode(SS_ETHERNET, OUTPUT);
  digitalWrite(SS_SD_CARD, HIGH);  // SD Card not active
  digitalWrite(SS_ETHERNET, HIGH); // Ethernet not active

  // SD card initialisation
  Serial.println();
  Serial.print("Initializing SD card...");
  digitalWrite(SS_SD_CARD, LOW); // SD Card ACTIVE
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    digitalWrite(SS_SD_CARD, HIGH); // SD Card not active
    digitalWrite(SS_ETHERNET, LOW); // Ethernet active
  } else {
    Serial.println("card initialized.");

    // List files on SDcard
    Serial.println();
    Serial.println("Files on SDcard:");
    File root;
    root = SD.open("/");
    printDirectory(root, 0);

    if (prepareSDcard == 1) {
      Serial.println();
      Serial.println("Deleting logfiles:");
      for (int i = 1; i <= numberOfConfiguredSensors; i++) {
        char fileName[13];
        sprintf(fileName, "sensor%d.txt", i);
        Serial.println(fileName);
        SD.remove(fileName);
        sprintf(fileName, "temps%d.txt", i);
        Serial.println(fileName);
        SD.remove(fileName);
      }
    }
    Serial.println();
    digitalWrite(SS_SD_CARD, HIGH); // SD Card not active
    digitalWrite(SS_ETHERNET, LOW); // Ethernet active
  }

  Serial.println("Starting ethernet. In case of dhcp issue, this may take a while...");
  // Start the Ethernet connection and the server
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip, dnServer, gateway, subnet);
    staticIP = 1;
  }

  Serial.println();
  Serial.print("My IP-address is: ");
  Serial.println(Ethernet.localIP());

  // Get the time for the first time
  webUnixTime(clienttime);

    // Read onewire sensors for the first time
  readOneWireSensors();
  
}

void loop() {
  
  // If using DHCP this keeps IP address reseved. If using static IP, comment out the next line
  if (staticIP == 0) {
    Ethernet.maintain();
  }
  
  // read temps periodically
  unsigned long sensorcurrentMillis = millis();
  if (sensorcurrentMillis - sensorpreviousMillis > sensorinterval) {
    sensorpreviousMillis = sensorcurrentMillis;
    readOneWireSensors();
  }

  // Trying to empty buffers if there is something
  for (int i = 1; i <= numberOfConfiguredSensors; i++) {
    restoreDataBuffer(i);
  }

  // Time sync periodically
  unsigned long timecheckcurrentMillis = millis();
  if (timecheckcurrentMillis - timecheckpreviousMillis > timecheckinterval) {
    timecheckpreviousMillis = timecheckcurrentMillis;
    webUnixTime(clienttime);        
  }

  // Time sync in case time has not been synced
  if (time == 0) {
    Serial.println("Time not configured, trying to get time");
    webUnixTime(clienttime);
  }

  // Serial.print("Free ram: ");
  //   Serial.println(freeRam ()); // Debug free ram amount. If result more than 1000, there should be no problems.
}

// **********Time functions**********

// Get unix time from a web server
void webUnixTime (Client &clienttime) {
  
  // Just choose any reasonably busy web server, the load is really low
  if (clienttime.connect(timeServer, 80)) {
    // Make an HTTP 1.1 request which is missing a Host: header
    // compliant servers are required to answer with an error that includes
    // a Date: header.
    clienttime.print(F("GET / HTTP/1.1 \r\n\r\n"));

    char buf[5];      // temporary buffer for characters
    clienttime.setTimeout(5000);
    if (clienttime.find((char *)"\r\nDate: ") // look for Date: header
        && clienttime.readBytes(buf, 5) == 5) { // discard
      unsigned wday = clienttime.parseInt();    // day
      clienttime.readBytes(buf, 1);    // discard
      clienttime.readBytes(buf, 3);    // month
      int wyear = clienttime.parseInt();    // year
      byte whour = clienttime.parseInt();   // hour
      byte wminute = clienttime.parseInt(); // minute
      byte wsecond = clienttime.parseInt(); // second

      int daysInPrevMonths;
      switch (buf[0]) {
        case 'F': daysInPrevMonths =  31; break; // Feb
        case 'S': daysInPrevMonths = 243; break; // Sep
        case 'O': daysInPrevMonths = 273; break; // Oct
        case 'N': daysInPrevMonths = 304; break; // Nov
        case 'D': daysInPrevMonths = 334; break; // Dec
        default:
          if (buf[0] == 'J' && buf[1] == 'a')
            daysInPrevMonths = 0;   // Jan
          else if (buf[0] == 'A' && buf[1] == 'p')
            daysInPrevMonths = 90;    // Apr
          else switch (buf[2]) {
              case 'r': daysInPrevMonths =  59; break; // Mar
              case 'y': daysInPrevMonths = 120; break; // May
              case 'n': daysInPrevMonths = 151; break; // Jun
              case 'l': daysInPrevMonths = 181; break; // Jul
              default: // add a default label here to avoid compiler warning
              case 'g': daysInPrevMonths = 212; break; // Aug
            }
      }

      // This code will not work after February 2100
      // because it does not account for 2100 not being a leap year and because
      // we use the day variable as accumulator, which would overflow in 2149
      wday += (wyear - 1970) * 365; // days from 1970 to the whole past year
      wday += (wyear - 1969) >> 2;  // plus one day per leap year
      wday += daysInPrevMonths;  // plus days for previous months this year
      if (daysInPrevMonths >= 59  // if we are past February
          && ((wyear & 3) == 0)) // and this is a leap year
        wday += 1;     // add one day
      // Remove today, add hours, minutes and seconds this month
      time = (((wday - 1ul) * 24 + whour) * 60 + wminute) * 60 + wsecond;
    }
  }
  delay(10);
  clienttime.flush();
  clienttime.stop();

  if (time != 0) {
    setTime(time);
    Serial.println("System time updated from time server");
    RTC.set(time);
    Serial.println("Time set to RTC");
  } else {
    Serial.println ("Time not available from server. Using RTC time.");
    time = RTC.get();
    if (time > 0) {
      Serial.println("System time updated from RTC");
      setTime(time);
    } else {
      Serial.println("Error getting time from RTC or RTC time not configured!");
    }
  }
}

//Function to create timestamp
String timestamp(time_t t) {
  String datetimejson = "time=";
  datetimejson += String(year(t));
  datetimejson += String("-");
  int varmonth = month(t);
  if (varmonth < 10) {
    datetimejson += String("0");
  }
  datetimejson += String(varmonth);
  datetimejson += String("-");
  int varday = day(t);
  if (varday < 10) {
    datetimejson += String("0");
  }
  datetimejson += String(varday);
  datetimejson += String("+");
  int varhour = hour(t);
  if (varhour < 10) {
    datetimejson += String("0");
  }
  datetimejson += String(varhour);
  datetimejson += String(":");
  int varminute = minute(t);
  if (varminute < 10) {
    datetimejson += String("0");
  }
  datetimejson += String(varminute);
  datetimejson += String(":");
  int varsecond = second(t);
  if (varsecond < 10) {
    datetimejson += String("0");
  }
  datetimejson += String(varsecond);
  //finally add needed text to create a complete json-string
  datetimejson += String("&value=");
  //Serial.print(datetimejson);
  //Serial.print(tz);
  //Serial.println();
  return datetimejson;
}

// **********Onewire functions**********

void displayOnewireDevices() {
  // Start up the onwire library
  sensors.begin();

  // Grab a count of devices on the onewire
  int numberOfSensors = sensors.getDeviceCount();

  // locate devices on the bus
  Serial.println("Locating onewire devices currently present in system...");
  Serial.print("Found ");
  Serial.print(numberOfSensors, DEC);
  Serial.println(" device(s):");

  // Loop through each device, print out address
  for (int i = 1; i <= numberOfSensors; i++) {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i)) {
      Serial.print("Device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      // Serial.println();
    } else {
      Serial.print("Ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }
}

// This function reads one wire sensors and calls other functions to process data further
void readOneWireSensors() {
  sensors.requestTemperatures(); // read sensors from onewire
  utc = now();
  int sensorid = 0;
  String timeValueString = "";
  String verifyString = "";
  // translate GMT to local time
  local = myTZ.toLocal(utc, &tcr);
  for (byte i = 0; i < numberOfConfiguredSensors; i++) {
    sensorid ++;
    timeValueString = timestamp(local);
    timeValueString += String(printTemperature(temp[i]));
    verifyString = timeValueString.substring(31, 38);
    Serial.println();
    Serial.print("Sensor");
    Serial.print(sensorid);
    Serial.print(": temperature / verifystring: ");
    Serial.println(verifyString);
    // If result is NAN or out of range, we do not want to try send it anywhere
    if (verifyString == " NAN" || verifyString == "9999.99") {
      Serial.print("Couldn't get the temperature from sensor");
      Serial.print(sensorid);
      Serial.println(", doing nothing. Please check connections.");
    } else {
      readAndPostOnewireSensorData(timeValueString, sensorid);
    }
  }
}

// This function reads the sensors and puts the data on rest
void readAndPostOnewireSensorData (String timeValueString, int sensorid) {

  char timeValue[47];
  // We need to know in other functions, which function has called the other function
  int callerid = 0;
  timeValueString.toCharArray(timeValue, 47);
  sprintf(pageName, "/sensor%d", sensorid);
  // restdata must be url encoded -- I guess not :)
  sprintf(restData, timeValue);
  if (!postPage(serverName, serverPort, pageName, restData, sensorid, callerid)) {
    Serial.print("Sensor");
    Serial.print(sensorid);
    Serial.println(" failed to upload data");
  } else {
    Serial.print("Sensor");
    Serial.print(sensorid);
    Serial.println(" data uploading passed");    
  }
}

// Printing onewire results
float printTemperature(DeviceAddress deviceAddress) {
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == -127.00 || tempC == 85.00) {
    // Returning value that is not accetable by server configuration, so not stored anywhere
    return 9999.99;
  } else {
    return tempC;
  }
}

// function to print a onewire device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
  Serial.print('\n');
}

// Function to put data to a rest server in correct format
byte postPage(char* domainBuffer, int thisPort, char* page, char* sensorData, int sensorid, int callerid) {
  char inChar;
  char outBuf[64];
  String inData;
  String httpstatus;

  Serial.print(F("connecting to rest server..."));

  if (clientrest.connect(domainBuffer, thisPort) == 1) {
    Serial.println(F(" connected"));

    // send the header
    sprintf(outBuf, "POST %s HTTP/1.1", page);
    clientrest.println(outBuf);
    sprintf(outBuf, "Host: %s", domainBuffer);
    clientrest.println(outBuf);
    clientrest.println(F("Connection: close\r\nContent-Type: application/x-www-form-urlencoded"));
    sprintf(outBuf, "Content-Length: %u\r\n", strlen(sensorData));
    clientrest.println(outBuf);

    // send the body (variables)
    clientrest.print(sensorData);
    Serial.print("Sensor data to send: ");
    Serial.println(sensorData);
  } else {
    if (callerid == 0) {
      Serial.println(F(" failed, writing data to log file"));
      sdWriteData(sensorData, sensorid);
    }
    return 0;
  }

  // int connectLoop = 0;

  while (clientrest.connected()) {
    int i = 0;
    while (clientrest.available()) {

      inChar = clientrest.read();
      inData += inChar;

      // Before printing, wait until new line reached
      if (inChar == '\n') {
        i++;
        Serial.print(inData);
        if (i == 1) {
          httpstatus = inData.substring(9, 12);          
        }
        inData = ""; // Clear buffer
      }
      // connectLoop = 0;
    }
    
    // delay(1);
    // connectLoop++;
    // if(connectLoop > 10000)
    // {
    //  Serial.println();
    //  Serial.println(F("Timeout"));
    //  clientrest.stop();
    // }

  }
  Serial.println("}");
  Serial.println();
    switch (httpstatus.toInt()) {
      case 401:
        Serial.println("401 Unauthorized -> logging data if not logged already");
        if (callerid == 0) {
          Serial.println(" failed, writing data to log file");
          sdWriteData(sensorData, sensorid);
        }
        Serial.println("disconnecting.");
        clientrest.stop();
        return 0;
        break;
      case 404:
        Serial.println("404 Not found. Server configuration problem. Please check rewrite rules and .htaccess -> Logging data if not logged already");
        if (callerid == 0) {
          Serial.println(" failed, writing data to log file");
          sdWriteData(sensorData, sensorid);
        } 
        Serial.println("disconnecting.");
        clientrest.stop();
        return 0;
        break;
      case 500:
        Serial.println("500 Internal server error. Server configuration problem -> Logging data if not logged already");
        if (callerid == 0) {
          Serial.println(" failed, writing data to log file");
          sdWriteData(sensorData, sensorid);
        } 
        Serial.println("disconnecting.");
        clientrest.stop();
        return 0;
        break;
      case 405:
        Serial.println("405 Method not allowed. Propably unacceptable data sent. (Wrong format or timestamp which is already stored in database) -> Not logging data");
        Serial.println("disconnecting.");
        clientrest.stop();
        return 1;
        break;
      default:
        // Any undefined status code will be considered as sent
        Serial.print("HTTP Status code: ");
        Serial.print(httpstatus);
        Serial.println(". Data considered to be sent, or we do not want to send or log it -> Not logging data");
        Serial.println("disconnecting.");
        clientrest.stop();
        return 1;
        break;
    }
}

/* // Debug freeram


  int freeRam () {
   extern int __heap_start, *__brkval;
   int v;
   return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
  }

*/

// **********Datalogging functions**********

// Write data to logfile
void sdWriteData (char* logData, int sensorid) {

  digitalWrite(SS_ETHERNET, HIGH); // Ethernet not active
  digitalWrite(SS_SD_CARD, LOW); // SD Card ACTIVE
  char sensorFile[13];
  sprintf(sensorFile, "sensor%d.txt", sensorid);
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open(sensorFile, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(logData);
    dataFile.close();
    
    // print to the serial port too:
    Serial.print(logData);
    Serial.println(" ...written to logfile");
  } else { 
    // if the file isn't open, pop up an error:
    Serial.print("error opening ");
    Serial.println(sensorFile);
  }

  digitalWrite(SS_SD_CARD, HIGH); // SD Card not active
  digitalWrite(SS_ETHERNET, LOW); // Ethernet active
}

// Read data from logfile and put it to a rest server

void restoreDataBuffer(int sensorid) {
  digitalWrite(SS_ETHERNET, HIGH); // Ethernet not active
  digitalWrite(SS_SD_CARD, LOW); // SD Card ACTIVE
  char sensorFile[13] = "";
  char tempSensorFile[13] = "";
  char logData[47] = "";
  unsigned long tempSensorFileSize = 0;
  unsigned long sensorFileSize = 0;
  int callerid = 1;
  sprintf(sensorFile, "sensor%d.txt", sensorid);
  sprintf(tempSensorFile, "temps%d.txt", sensorid);
  sprintf(pageName, "/sensor%d", sensorid);

  if (SD.exists(tempSensorFile)) {
    File tempDataFile = SD.open(tempSensorFile);
    tempSensorFileSize = tempDataFile.size();
    tempDataFile.close();
  } else {
    Serial.print("Sensor");
    Serial.print(sensorid);
    Serial.println(" temporary file not found, crating it.");
    File tempDataFile = SD.open(tempSensorFile, FILE_WRITE);
    tempSensorFileSize = tempDataFile.size();
    tempDataFile.close();
  }
  if (tempSensorFileSize > 0 && tempSensorFileSize < 30) {
    SD.remove(tempSensorFile);
    File tempDataFile = SD.open(tempSensorFile, FILE_WRITE);
    tempDataFile.close();
  }

  File dataFile = SD.open(sensorFile);

  // If tempfile and buffer is same size, we know that all data has been passed to server and we can delete both files
  sensorFileSize = dataFile.size();
  if (sensorFileSize > 0) {
    Serial.println();
    Serial.print("Sensor");
    Serial.print(sensorid);
    Serial.print(" file size is total: ");
    Serial.println(sensorFileSize);
  }
  if (sensorFileSize == tempSensorFileSize && sensorFileSize != 0) {
    dataFile.close();
    SD.remove(tempSensorFile);
    SD.remove(sensorFile);
    Serial.println();
    Serial.print(tempSensorFile);
    Serial.println(" deleted.");
    Serial.print(sensorFile);
    Serial.println(" deleted.");
    Serial.println();
  }

  if (dataFile) {
    // go to position pointed at last stage and read one line:
    Serial.print("Temporary file size for sensor");
    Serial.print(sensorid);
    Serial.print(" is: ");
    Serial.println(tempSensorFileSize);
    dataFile.seek(tempSensorFileSize);
    Serial.print("Current position in file ");
    Serial.print(tempSensorFile);
    Serial.print(" is: ");
    Serial.println(dataFile.position());
    Serial.println();
    String line = dataFile.readStringUntil('\r');
    line.toCharArray(logData, 47);
    sprintf(restData, logData);
    digitalWrite(SS_SD_CARD, HIGH); // SD Card not active
    digitalWrite(SS_ETHERNET, LOW); // Ethernet active
    Serial.println("Trying to send data from buffer to rest server");
    if (!postPage(serverName, serverPort, pageName, restData, sensorid, callerid)) {
      Serial.print("Sensor");
      Serial.print(sensorid);
      Serial.println(" failed to send data to rest server");
      digitalWrite(SS_ETHERNET, HIGH); // Ethernet not active
      digitalWrite(SS_SD_CARD, LOW); // SD Card ACTIVE
      dataFile.close();
    } else {
      Serial.print("Sensor");
      Serial.print(sensorid);
      Serial.println(" send data to rest server passed");
      // write the data to tempsensorfile
      File tempDataFile = SD.open(tempSensorFile, FILE_WRITE);
      tempDataFile.println(logData);
      tempDataFile.close();      
    }

    // close the file:
    dataFile.close();
  } else {
    // if the file didn't open, print an error:
    // Serial.print("error opening ");
    // Serial.println(sensorfile);
  }

  digitalWrite(SS_SD_CARD, HIGH); // SD Card not active
  digitalWrite(SS_ETHERNET, LOW); // Ethernet active
}

void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

