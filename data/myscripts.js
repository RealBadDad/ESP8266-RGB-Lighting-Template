/*------------------------------------------------------------------------------
  12th Oct 2020
  Project: RGB_Light_Template
  Author: RealBadDad
  Platforms: ESP8266 - NodeMCU
  ------------------------------------------------------------------------------
  Description: 
  RGB_Light_Template 
  Basic RGB lighting template providing:
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

  This file handles the passing of data through websocets to and from the 
  web page.

  This release of the template has been filled in for the overwatch
  light project that you can find at
  https://realbaddad.com/overwatch-light/

  ------------------------------------------------------------------------------
  License:
  Please see attached LICENSE.txt file for details.
------------------------------------------------------------------------------*/

var Socket;
var Heading1Text
var Heading2Text


/* Setup Websocket and function to extract data coming in */
function init() 
{
  Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
  Socket.onopen = function(evt) { console.log('websock open');};
  Socket.onclose = function(evt) { console.log('websock close');};
  Socket.onerror = function(evt) { console.log(evt);};
  Socket.onmessage = function(evt)
  {
    console.log(evt);
    AppData = JSON.parse(evt.data);
    console.log(AppData); 
    document.getElementById("Brightness").value = ExtractData(AppData.Brightness,document.getElementById("Brightness").value);
    document.getElementById("Rate").value = ExtractData(AppData.Rate,document.getElementById("Rate").value);

    document.getElementById("UserReqRed").value = ExtractData(AppData.RedLevel,document.getElementById("UserReqRed").value);
    document.getElementById("UserReqGreen").value = ExtractData(AppData.GreenLevel,document.getElementById("UserReqGreen").value);
    document.getElementById("UserReqBlue").value = ExtractData(AppData.BlueLevel,document.getElementById("UserReqBlue").value);

    document.getElementById("Effects").selectedIndex = ExtractData(AppData.EffectType,document.getElementById("Effects").selectedIndex);

    document.getElementById("Itemonoff").checked = ExtractData(AppData.UserReqOnOFF,document.getElementById("Itemonoff").value);
 
    document.getElementById("Heading1").innerHTML  = ExtractData(AppData.Heading1,document.getElementById("Heading1").innerHTML );
    document.getElementById("Heading2").innerHTML  = ExtractData(AppData.Heading2,document.getElementById("Heading2").innerHTML );
    document.getElementById("Title1").innerHTML  = ExtractData(AppData.Heading2,document.getElementById("Heading2").innerHTML );
    updateColors();
  }
}

/* Returns current setting if no new data has been provided */
function ExtractData(NewData, OldData)
{
  if(NewData != null)
  {
    return(NewData);
  }
  else
  {
    return(OldData);
  }
}

function sendText()
{   
}

/* Constructs and sends JSON application data
Brightness - requested illumination level
Rate - Rate of effect transitions
UserReqRed - Red level setting
UserReqGreen - Green Level setting
User ReqBlue - Blue Level setting
Effects - Which effect to display
Itemonoff - Light on / off setting */
function sendDataPacket()
{
  updateColors();
  AppData.Brightness = document.getElementById("Brightness").value;
  AppData.Rate = document.getElementById("Rate").value;

  AppData.RedLevel = document.getElementById("UserReqRed").value;
  AppData.GreenLevel = document.getElementById("UserReqGreen").value;
  AppData.BlueLevel = document.getElementById("UserReqBlue").value;

  AppData.EffectType = document.getElementById("Effects").selectedIndex;

  AppData.UserReqOnOFF = document.getElementById("Itemonoff").checked ? '1' : '0';
 
  Socket.send(JSON.stringify(AppData));
  console.log(AppData);
}    

/* Sets elemets colour to the user selected colour from the
Red Green and Blue user settings */
function updateColors() 
{
    var r = parseInt(document.getElementById("UserReqRed").value,10).toString(16);
    var g = parseInt(document.getElementById("UserReqGreen").value,10).toString(16);
    var b = parseInt(document.getElementById("UserReqBlue").value,10).toString(16);
    var h = "#" + pad(r) + pad(g) + pad(b);
    document.getElementById("coloursample").style.backgroundColor = h;
}

/* Pad single digits with 0's */
function pad(n)
{
  return(n.length<2) ? "0"+n : n;
}

/*Used to trigger an effect change request from a button press  */
/* parammeter Effect is the nuber identifier of the effect required */
function RequestEffect(Effect)
{
   document.getElementById("Effects").selectedIndex = Effect;
   sendDataPacket();
}

 