/*-------------------------------------------------------------------
  12th Oct 2020
  Project: RGB_Light_Template
  Author: RealBadDad
  Platforms: ESP8266 - NodeMCU
  -------------------------------------------------------------------
  Description:
  Basic WEB interface with the following features

  1) Hosted WiFi - Station
  2) Hard coded WiFi SSID and PASSWORD
  3) MDNS responder
  4) SPIFFS filing system
  5) Storage of settings in file system
  6) HTML web page in index.html
  7) Style sheet in style.css
  8) Websocket data passing
  9) JSON data passing structures to and from index.html
  10) OTA update system for embedded code

  If your utilising the OTA download the Ardino IDE stores the binary
  files at this location, you will find a folder called arduino_build
  with the latest date with the files you require in it.
 
  C:\Users\"username"\AppData\Local\Temp

  Please note that the folder AppData is hidden and will need to be
  made visible and "username" is your logged in username folder.

  This release of the template has been filled in for the overwatch
  light project that you can find at
  https://realbaddad.com/overwatch-light/
  -------------------------------------------------------------------
  License:
  Please see attached LICENSE.txt file for details.
/*-------------------------------------------------------------------*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <NeoPixelBrightnessBus.h>

/* XXXX->EDIT REQUIRED HERE<-XXXX - WIFI Interface settings          */
/* Edit these for your Wifi Router settings                          */
/* Change SSID for your router SSID name                             */
/* Change PASSWORD for your router Password                          */
const char* ssid = "SSID";
const char* password = "PASSWORD";

 
/* XXXX->EDIT REQUIRED HERE<-XXXX - MDNS name and responder.         */
/* Give your unit a name so that you dont need to search for         */
/* addresses just change Overwatch to the name you wish to use       */
const char* host = "Overwatch";
MDNSResponder mdns;
/*Final name will be http://Overwatch.local/                         */

/* XXXX->EDIT REQUIRED HERE<-XXXX - LED hardware                     */
/* Make sure you alter these settings for the hardware you are using */
const uint16_t PixelCount = 74;
/* Main Ring       0  to 49                                          */
/* Top Segment     50 to 61                                          */
/* Pointers (both) 62 to 74                                          */

/* TopSeg_End defines where the pointers start as some lighting      */ 
/* sequences light all these LEDs up at once, if not requred just    */  
/* set to a number greater than PixelCount                           */
const int TopSeg_End      = 61;

/* XXXX->EDIT REQUIRED HERE<-XXXX                                    */
/* If using NeoEsp8266DmaWs2812xMethod on the NeopixelBrightnessBus  */
/* this setting will be defaulted to 2 if your using other types of  */
/* LEDs then this settign needs to be considered for hte method you  */
/* have used in wiring up the LEDs                                   */
/* The enable pin is ther shoudl you have used a level shifter for   */
/* the data line                                                     */
const uint8_t PixelPin = 2;
const uint8_t EnablePin = 3;

/* XXXX->EDIT REQUIRED HERE<-XXXX - Customise the WEB page           */
/* Heading 1 is the main heading and Heading 2 is the sub heading    */
String Heading1 = "Williams";
String Heading2 = "Overwatch Light";

/* XXXX->EDIT REQUIRED HERE<-XXXX                                    */
/* Alter the settings for the type of LEDs you have fittted.         */
/* Details on how to use the NeoPixelBus can be foud at              */ 
/* https://github.com/Makuna/NeoPixelBus/wiki                        */
NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp8266DmaWs2812xMethod  > strip(PixelCount, PixelPin);

/*-------------------------------------------------------------------*/
/* HTTP Server and Websockets */
/*-------------------------------------------------------------------*/
ESP8266WebServer httpServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdater;

struct ControlDataStruct {
  int SetRedLevel;
  int SetGreenLevel;
  int SetBlueLevel;
  int SetBrightnessLevel;
  int SetRateLevel;
  int SetStoreRequest;
  int SetONOFFRequest;
  int SetEffectType;
};
ControlDataStruct WebPageData = { 0, 0, 0, 0, 0, 0, 0, 0 };

struct DelaySettingsStruct {
  unsigned long previousMillis; 
  unsigned long delayMillis;
};
DelaySettingsStruct DelaySettings = {0, 0};

struct LightingEffectDataStruct {
  uint8_t ledpointer;
  uint8_t colourpointer;
  uint8_t startposition;
  uint8_t lasteffect;
  uint8_t flashcount;
};
LightingEffectDataStruct LightingEffectData = {0, 0, 0, 0, 0};

/*-------------------------------------------------------------------*/
/* Setup                                                             */
/*-------------------------------------------------------------------*/
void setup(void)
{
  /*Setup for all Debugging*/
  Serial.begin(115200);
  Serial.println();

  /*WIFI Setup*/
  // Wifi Mode is a AP (access point) STA (Station)
  WiFi.mode(WIFI_STA);
  WiFi.hostname(host);
  Serial.print("Connecting ");
  // Connect to the local Wifi router with pre-defined SSID and password
  WiFi.begin(ssid, password);
  // Wait for the connection to be made
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  /*WiFi connection is OK*/
  Serial.println ( "" );
  Serial.print ( "Connected to " ); Serial.println ( ssid );
  Serial.print ( "IP address: " ); Serial.println ( WiFi.localIP() );

  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  else
  {
    Serial.println("SPIFFS Started");
  }

  /* Setup MDNS*/
  if (mdns.begin(host, WiFi.localIP()))
  {
    Serial.println ( "MDNS responder started" );
  }

  httpUpdater.setup(&httpServer);
  /*On root address handle the main web page*/
  httpServer.on ( "/", handleRoot );
  httpServer.on ( "/Enable", handleEnable);
  httpServer.on ( "/Disable", handleDisable);
  httpServer.on ( "/Store", handleStoreRequest);
  /*Route to load style.css file*/
  httpServer.onNotFound(handleWebRequests);

  /*Start Server*/
  httpServer.begin();

  /*Start mdns service*/
  mdns.addService("http", "tcp", 80);
  mdns.addService("ws", "tcp", 81);
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.printf("HTTPServer ready! Open http://%s.local/update in your browser\n", host);

  /*Load all previously stored settigns for the project*/
  LoadSettings();

  /*Set Data Enable pin output*/
  pinMode(EnablePin, OUTPUT);
  digitalWrite(EnablePin, HIGH);

  /*This resets all the neopixels to an off state*/
  strip.Begin();
  strip.Show();
}

/*-------------------------------------------------------------------*/
/* handleEnable                                                      */
/* Page to handle direct html request to turn the LEDs on            */
/* This is used by my home automation system to control the lights   */
/*-------------------------------------------------------------------*/
void handleEnable()
{
  WebPageData.SetONOFFRequest = 1;
  Serial.println("Enable");
  httpServer.send(200, "text/plane",""); 
  handleRoot();
  UpdateWebPage(false);
}

/*-------------------------------------------------------------------*/
/* handleDisable                                                     */
/* Page to handle direct html request to turn the LEDs off           */
/* This is used by my home automation system to control the lights   */
/*-------------------------------------------------------------------*/
void handleDisable()
{
  WebPageData.SetONOFFRequest = 0;
  Serial.println("Disable");
  httpServer.send(200, "text/plane",""); 
  handleRoot();
  UpdateWebPage(false);
}

/*-------------------------------------------------------------------*/
/* handleStoreRequest                                                */
/* Page to handle direct html request to store the current settings  */
/* When the user requests /Store all the current settings are stored */
/* into a config file in the SPIFFS system.                          */
/*-------------------------------------------------------------------*/
void handleStoreRequest()
{
  /* Ste flag for the main program to identify a store request has been made */
  WebPageData.SetStoreRequest = 1;
  Serial.println("StoreRequested");  
  httpServer.send(200, "text/plane",""); 
  handleRoot();
  UpdateWebPage(false);
}

/*-------------------------------------------------------------------*/
/* handleWebRequests                                                 */
/*                                                                   */
/*-------------------------------------------------------------------*/
void handleWebRequests() {
  if (loadFromSpiffs(httpServer.uri())) return;
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  /*Load each file in from spiffs*/
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " NAME:" + httpServer.argName(i) + "\n VALUE:" + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
  Serial.println(message);
}

/*-------------------------------------------------------------------*/
/* loadFromSpiffs                                                    */
/* Load  file from spiffs and extract data type                      */
/*-------------------------------------------------------------------*/
bool loadFromSpiffs(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) path += "index.htm";

  if (path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if (path.endsWith(".html")) dataType = "text/html";
  else if (path.endsWith(".htm")) dataType = "text/html";
  else if (path.endsWith(".css")) dataType = "text/css";
  else if (path.endsWith(".js")) dataType = "application/javascript";
  else if (path.endsWith(".png")) dataType = "image/png";
  else if (path.endsWith(".gif")) dataType = "image/gif";
  else if (path.endsWith(".jpg")) dataType = "image/jpeg";
  else if (path.endsWith(".ico")) dataType = "image/x-icon";
  else if (path.endsWith(".xml")) dataType = "text/xml";
  else if (path.endsWith(".pdf")) dataType = "application/pdf";
  else if (path.endsWith(".zip")) dataType = "application/zip";
  else if (path.endsWith(".ttf")) dataType = "font/ttf";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (httpServer.hasArg("download")) dataType = "application/octet-stream";
  if (httpServer.streamFile(dataFile, dataType) != dataFile.size()) {
  }

  dataFile.close();
  return true;
}

/*-------------------------------------------------------------------*/
/* UpdateWebPage                                                     */
/* Update connected clients with the current project data            */
/*-------------------------------------------------------------------*/
void UpdateWebPage(bool InitialSettings)
{
  /*Create JSON buffer for data transmission*/
  StaticJsonDocument<200> jsonBuffer;
  String jsonStr = "";

  /*Fill buffer with curretn settings*/
  jsonBuffer["RedLevel"] = WebPageData.SetRedLevel;
  jsonBuffer["GreenLevel"] = WebPageData.SetGreenLevel;
  jsonBuffer["BlueLevel"] = WebPageData.SetBlueLevel;
  jsonBuffer["Brightness"] = WebPageData.SetBrightnessLevel;
  jsonBuffer["Rate"] = WebPageData.SetRateLevel;
  jsonBuffer["EffectType"] = WebPageData.SetEffectType;
  jsonBuffer["UserReqOnOFF"] = WebPageData.SetONOFFRequest;
  /* Add data to only be sent on initial setup */
  if (InitialSettings == true)
  {
    /* Additional non functional page data to setup names and haeadings on web page */
    jsonBuffer["Heading1"] = Heading1;
    jsonBuffer["Heading2"] = Heading2;
  }

  /* Format data inot a string for transmission */
  serializeJson(jsonBuffer, jsonStr);

  /*Debug print data being sent*/
  Serial.print("To web page - ");
  Serial.println(jsonStr);

  /*Send data through websocket*/
  webSocket.broadcastTXT(jsonStr);

}

/*-------------------------------------------------------------------*/
/* webSocketEvent                                                    */
/* Event has been sensed on the websocket link                       */
/*-------------------------------------------------------------------*/
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  StaticJsonDocument<200> jsonBuffer;

  /*Text string received through websocket*/
  if (type == WStype_TEXT)
  {
    /*Creata a JSON structure from the text data*/
    deserializeJson(jsonBuffer, payload);
    /*Debug print received data string*/
    String jsonStr = "";
    serializeJson(jsonBuffer, jsonStr);
    Serial.print("From web page - ");
    Serial.println(jsonStr);

    /*Extract each known data type, if data type hasnt been*/ 
    /*sent then keep the current settings                  */
    if(!jsonBuffer["RedLevel"].isNull())
    {
      WebPageData.SetRedLevel = (int)jsonBuffer["RedLevel"];
    }
    if(!jsonBuffer["GreenLevel"].isNull())
    {
      WebPageData.SetGreenLevel = (int)jsonBuffer["GreenLevel"];
    }
    if(!jsonBuffer["BlueLevel"].isNull())
    {
      WebPageData.SetBlueLevel = (int)jsonBuffer["BlueLevel"];
    }
    if(!jsonBuffer["Brightness"].isNull())
    {
      WebPageData.SetBrightnessLevel = (int)jsonBuffer["Brightness"];
    }
    if(!jsonBuffer["Rate"].isNull())
    {
      WebPageData.SetRateLevel = (int)jsonBuffer["Rate"];
    } 
    if(!jsonBuffer["EffectType"].isNull())
    {
      WebPageData.SetEffectType = (int)jsonBuffer["EffectType"];
    } 
    if(!jsonBuffer["UserReqOnOFF"].isNull())
    {
      WebPageData.SetONOFFRequest = (int)jsonBuffer["UserReqOnOFF"];
    }
       
    /*Now new data has been received we must update all other */
    /*web pages that are attached to keep them in sync        */
    UpdateWebPage(false);
  }
  /* Each tie a new websocket connection is made send webpage data */
  else if (type == WStype_CONNECTED)
  {
    /*Send webpage data but with extra data for first connection*/    
    UpdateWebPage(true);
  }
}

/*-------------------------------------------------------------------*/
/* StoreSettings                                                     */
/* Store all current settings as a JSON string t a file called       */
/* config.json in the SPIFFS file system                             */
/*-------------------------------------------------------------------*/
void StoreSettings(void)
{
  StaticJsonDocument<200> jsonBuffer;

  /*Open config.json file */
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    /*ERROR opening file*/
    Serial.println("failed to open config file for writing");
  }

  /*Create a JSON buffer of the curretn settings to store*/
  jsonBuffer["RedLevel"] = WebPageData.SetRedLevel;
  jsonBuffer["GreenLevel"] = WebPageData.SetGreenLevel;
  jsonBuffer["BlueLevel"] = WebPageData.SetBlueLevel;
  jsonBuffer["Brightness"] = WebPageData.SetBrightnessLevel;
  jsonBuffer["Rate"] = WebPageData.SetRateLevel;
  jsonBuffer["EffectType"] = WebPageData.SetEffectType;
  jsonBuffer["UserReqOnOFF"] = WebPageData.SetONOFFRequest;

  /*Convert JSON data to string for writing to file*/
  String jsonStr = "";
  serializeJson(jsonBuffer, jsonStr);
  /*Debug print string being writen to file*/
  Serial.print("Writing to file - ");
  Serial.println(jsonStr);
  /*Store string to config.json file*/
  serializeJson(jsonBuffer, configFile);
  /*Close file*/
  configFile.close();
}

/*-------------------------------------------------------------------*/
/* LoadSettings                                                      */
/* Loads setting stored to a file called config.json stored in the   */ 
/* SPIFFS file system.                                               */
/* If no config file is found or settings are missing in the file a  */
/* default value is used                                             */
/*-------------------------------------------------------------------*/
void LoadSettings (void)
{
  StaticJsonDocument<200> jsonBuffer;
  /*Attemt to open config.json file*/
  if (SPIFFS.exists("/config.json"))
  {
    //file exists, reading and loading
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile)
    {
      Serial.println("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      deserializeJson(jsonBuffer, buf.get());

      String jsonStr = "";
      serializeJson(jsonBuffer, jsonStr);
      Serial.print("Reading from file - ");
      Serial.println(jsonStr);

      WebPageData.SetRedLevel = (int)jsonBuffer["RedLevel"];
      WebPageData.SetGreenLevel = (int)jsonBuffer["GreenLevel"];
      WebPageData.SetBlueLevel = (int)jsonBuffer["BlueLevel"];
      WebPageData.SetBrightnessLevel = (int)jsonBuffer["Brightness"];
      WebPageData.SetRateLevel = (int)jsonBuffer["Rate"];
      WebPageData.SetEffectType = (int)jsonBuffer["EffectType"];
      WebPageData.SetONOFFRequest = (int)jsonBuffer["UserReqOnOFF"];

    }
    else
    {
      Serial.println("Failed to open config file");
    }
  }
  else
  {
    Serial.println("No config file found");
  }
}
                     
/*-------------------------------------------------------------------*/
/* LightingEffects                                                   */
/*                                                                   */
/*-------------------------------------------------------------------*/
void LightingEffects(void)
{
  unsigned long currentMillis = millis();
  int position = 0;
  int NewLevel = 0;
  RgbColor ImmColour;
  
  /*Request to store current settings*/
  if (WebPageData.SetStoreRequest > 0)
  {
    WebPageData.SetStoreRequest = 0;
    StoreSettings();
    Serial.println("Settings Store activated");
  }

  /* Light is off*/
  if (WebPageData.SetONOFFRequest == 0)
  {
    if (LightingEffectData.ledpointer <= strip.PixelCount())
    {
      LightingEffectData.ledpointer = strip.PixelCount() + 10;
      DelaySettings.delayMillis = 0;
      strip.ClearTo(RgbColor(0, 0, 0));
      strip.Show();
    }
  }
  
  /*Light is on*/
  else
  {
    if(WebPageData.SetEffectType>5)
    {
      DelaySettings.delayMillis = WebPageData.SetRateLevel;
      if (currentMillis - DelaySettings.previousMillis >= 400)
      {
        // save the last time you altered the LEDs
        DelaySettings.previousMillis = currentMillis;

        if(LightingEffectData.colourpointer>0)
        {
          LightingEffectData.colourpointer=0;
          LightingEffectData.flashcount++;
          switch(WebPageData.SetEffectType)
          {   
            case 6: /*6--->DINNER - GREEN FLASHING ON & OFF SLOW*/  
              ImmColour = RgbColor(0, 255, 0);
            break;
    
            case 7: /*7--->BEDTIME - BLUE FLASHING ON & OFF SLOW */  
              ImmColour = RgbColor(0, 0, 255);
            break;
    
            case 8: /*8--->TOO LOUD - RED FLASHING ON & OFF FAST */  
              ImmColour = RgbColor(255, 0, 0);
            break;
      
            default:
              LightingEffectData.colourpointer = 0;
              LightingEffectData.startposition = 0;
              LightingEffectData.flashcount = 0;
              LightingEffectData.ledpointer = strip.PixelCount() ;
            break;
          }
        }
        else
        {
          LightingEffectData.colourpointer=1;
          ImmColour = RgbColor(0, 0, 0);
        }
        LightingEffectData.ledpointer = 0;
        while (LightingEffectData.ledpointer < strip.PixelCount())
        {
          strip.SetPixelColor(LightingEffectData.ledpointer, ImmColour);
          LightingEffectData.ledpointer++;
        }   
        strip.SetBrightness(100);
        if(LightingEffectData.flashcount > 10)
        {
          WebPageData.SetEffectType=LightingEffectData.lasteffect;
          LightingEffectData.colourpointer = 0;
          LightingEffectData.startposition = 0;
          LightingEffectData.flashcount = 0;
          LightingEffectData.ledpointer = strip.PixelCount();
          DelaySettings.previousMillis = currentMillis;
          UpdateWebPage(false);
        }     
      }
      /* Update all LEDs */
      strip.Show();
    }
    else
    {
      /*Clear flash counter for immediate effects, so next trigger starts from 0*/
      LightingEffectData.flashcount = 0;
      
      if((LightingEffectData.lasteffect!=WebPageData.SetEffectType) || (LightingEffectData.ledpointer > strip.PixelCount()))
      {
        LightingEffectData.lasteffect=WebPageData.SetEffectType;
        LightingEffectData.colourpointer = 0;
        LightingEffectData.startposition = 0;
        LightingEffectData.ledpointer = strip.PixelCount();
        DelaySettings.previousMillis = currentMillis;
      }

      if (currentMillis - DelaySettings.previousMillis >= DelaySettings.delayMillis)
      {
        // save the last time you altered the LEDs
        DelaySettings.previousMillis = currentMillis;
        // update delay time if its been changed
        DelaySettings.delayMillis = WebPageData.SetRateLevel;
        
        switch(WebPageData.SetEffectType)
        {
          case 0: /*0--->Solid - Fade*/
            if (LightingEffectData.ledpointer < strip.PixelCount())
            {
              LightingEffectData.ledpointer++;
            }
            else
            {
              LightingEffectData.ledpointer = 0;
            }
         
            if(LightingEffectData.ledpointer < TopSeg_End)
            {
              strip.SetPixelColor(LightingEffectData.ledpointer, RgbColor(WebPageData.SetRedLevel, WebPageData.SetGreenLevel, WebPageData.SetBlueLevel)); 
            }
            else
            {
              while (LightingEffectData.ledpointer < strip.PixelCount())
              {
                strip.SetPixelColor(LightingEffectData.ledpointer, RgbColor(WebPageData.SetRedLevel, WebPageData.SetGreenLevel, WebPageData.SetBlueLevel));
                LightingEffectData.ledpointer++;
              }   
            }      
          break;
  
          case 1: /*1--->Solid - Spiral*/
            if (LightingEffectData.ledpointer < strip.PixelCount())
            {
              LightingEffectData.ledpointer++;
            }
            else
            {
              LightingEffectData.ledpointer = 0;
            }
         
            if(LightingEffectData.ledpointer < TopSeg_End)
            {
              strip.SetPixelColor(LightingEffectData.ledpointer, RgbColor(WebPageData.SetRedLevel, WebPageData.SetGreenLevel, WebPageData.SetBlueLevel)); 
            }
            else
            {
              while (LightingEffectData.ledpointer < strip.PixelCount())
              {
                strip.SetPixelColor(LightingEffectData.ledpointer, RgbColor(WebPageData.SetRedLevel, WebPageData.SetGreenLevel, WebPageData.SetBlueLevel));
                LightingEffectData.ledpointer++;
              }   
            }   
          break;
  
          case 2: /*2--->Rainbow - fixed rainbow*/
            if (LightingEffectData.ledpointer < strip.PixelCount())
            {
              LightingEffectData.ledpointer++;
            }
            else
            {
              LightingEffectData.ledpointer = 0;
            }
            
            LightingEffectData.colourpointer = (LightingEffectData.ledpointer*(256/strip.PixelCount())) & 0xFF;
            if(LightingEffectData.ledpointer < TopSeg_End)
            {
              strip.SetPixelColor(LightingEffectData.ledpointer, ColourWheel(LightingEffectData.colourpointer)); 
            }
            else 
            {
              while (LightingEffectData.ledpointer < strip.PixelCount())
              {
                strip.SetPixelColor(LightingEffectData.ledpointer, ColourWheel(LightingEffectData.colourpointer));
                LightingEffectData.ledpointer++; 
              }
            }
          break;
  
          case 3: /*3--->Rainbow Cycle - fade whole light through the colours*/
            LightingEffectData.ledpointer = 0;
            if (LightingEffectData.colourpointer < 0xFF)
            {
              LightingEffectData.colourpointer++;
            }
            else
            {
              LightingEffectData.colourpointer = 0;
            }
            while(LightingEffectData.ledpointer < strip.PixelCount())
            {
              strip.SetPixelColor(LightingEffectData.ledpointer, ColourWheel(LightingEffectData.colourpointer & 0xFF));
              LightingEffectData.ledpointer++; 
            }                
          break;
          
          case 4: /*4--->Rainbow Circle - rainbow colours moving round the edge*/
            /* Step the rainbow sequences start position one location at a time */   
            if (LightingEffectData.startposition < strip.PixelCount())
            {
              LightingEffectData.startposition++;
            }
            else
            {
              LightingEffectData.startposition= 0;
            }
            /* Fill the rainbow sequence in */
            LightingEffectData.ledpointer=0;
            while(LightingEffectData.ledpointer < strip.PixelCount())
            {
              position=LightingEffectData.ledpointer+LightingEffectData.startposition;
              
              if(position >= strip.PixelCount())
              {
                position = position - strip.PixelCount();
              }
              
              LightingEffectData.colourpointer = (LightingEffectData.ledpointer*(256/strip.PixelCount())) & 0xFF;     
              strip.SetPixelColor(position, ColourWheel(LightingEffectData.colourpointer)); 
              LightingEffectData.ledpointer++;
            }        
          break;

          case 5: /*5--->Flame - Flame / Fire effect*/     
            /* Calculate a new random brightness level */
            NewLevel = random(WebPageData.SetBrightnessLevel);
            /* Set Pixel to correct colour */
            strip.SetPixelColor(random(strip.PixelCount()), RgbColor(NewLevel*WebPageData.SetRedLevel/255, NewLevel*WebPageData.SetGreenLevel/255, NewLevel*WebPageData.SetBlueLevel/255));
        
            /* Delay to next flicker update */
            DelaySettings.delayMillis = random(((255-WebPageData.SetRateLevel)/6)); 
          break;
  
          default:
            LightingEffectData.colourpointer = 0;
            LightingEffectData.startposition = 0;
            LightingEffectData.ledpointer = strip.PixelCount() ;
          break;
        }
        /* set brightness of all leds */
        strip.SetBrightness(WebPageData.SetBrightnessLevel);
      }
      
      /* Update all LEDs */
      strip.Show();
    } 
  }
}     

/*-------------------------------------------------------------------*/
/* ColourWheel                                                       */
/* Input a value 0 to 255 to get a color value.                      */
/* The colours are a transition r - g - b - back to r.               */
/*-------------------------------------------------------------------*/
RgbColor ColourWheel(uint8_t WheelPos) 
{
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) 
  {
    return RgbColor(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) 
  {
    WheelPos -= 85;
    return RgbColor(0, WheelPos * 3, 255 - WheelPos * 3);
  } else 
  {
    WheelPos -= 170;
    return RgbColor(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}

/*-------------------------------------------------------------------*/
/* handleRoot                                                        */
/* Server root web page                                              */
/*-------------------------------------------------------------------*/
void handleRoot()
{
  File file = SPIFFS.open("/index.html", "r");
  httpServer.streamFile(file, "text/html");
  file.close();
}

/*-------------------------------------------------------------------*/
/* Main program loop                                                 */
/*                                                                   */
/*-------------------------------------------------------------------*/
void loop(void)
{
  webSocket.loop();
  mdns.update();
  httpServer.handleClient();
  LightingEffects();
}
