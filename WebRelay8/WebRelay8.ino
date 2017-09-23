#include <Arduino.h>
#include <EEPROM.h>
#include <UIPEthernet.h>
#include <UIPServer.h>
#include <UIPClient.h>
#include <ethernet_comp.h>
#include <SmcfJsonDecoder.h>
#include <WebMVC.h>
#include "WebRelayService.h"

/*
MODIFIED by Sergio M C Figueiredo

History:
v. 0.2.1 - Original Nano_WebRelay8.ino (got from http://playground.arduino.cc/Code/NanoWebRelay8)
v. 0.3 - Serves HTML Page for use in browsers (mobile compatible);

Installing libraries (if you have not done yet):

    C:\> cd [path to Arduino distribution]\libraries
    C:\..\libraries> git clone https://github.com/ntruchsess/arduino_uip UIPEthernet
    C:\..\libraries> git clone https://github.com/smarcelobr/arduino-SmcfJsonDecoder.git SmcfJsonDecoder
    C:\..\libraries> git clone https://github.com/smarcelobr/arduino-WebMVC.git WebMVC


Original comment (the same rights are applicable for this modified version):
	Nano_WebRelay8.ino - Sketch for Arduino Nano with ATmega328 implementation to control
	relays via HTTP requests.
	Copyright (c) 2015 Iwan Zarembo <iwan@zarembo.de>
	All rights reserved.

	It is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Dieses Programm ist Freie Software: Sie können es unter den Bedingungen
	der GNU General Public License, wie von der Free Software Foundation,
	Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
	veröffentlichten Version, weiterverbreiten und/oder modifizieren.

	Dieses Programm wird in der Hoffnung, dass es nützlich sein wird, aber
	OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
	Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
	Siehe die GNU General Public License für weitere Details.

	Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
	Programm erhalten haben. Wenn nicht, siehe <http://www.gnu.org/licenses/>.
  */

/**
 * Nano_WebRelay8 provides a small webserver to control releays connected to 
 * your Arduino Nano with ATmega328.
 *
 * What the sketch does:
 * It is a small web server which listens to http requests to (de-)activate one
 * or more relays.
 *
 * Features:
 * - Initial GET request returns a HTML PAGE for control relay with a browser (mobile compatible)
 * - A GET request to '\api\r' will return a JSON array with the current status of the relay.
 *   e.g.  {"r":[0,0,1,0,0,0,0,0]}
 *   -> 0 means the relay is turned off
 *   -> 1 means the relay is turned on
 *   Due to performance reasons only the status is returned. But you can see that 
 * 	 the first  entry is for the first relay, the second for the second and so on.
 * - Only POST requests can change the status of the relays. The request data must 
 *   follow the pattern: 
 * 		relay number = 0 or 1 or 2 => e.g. 0=1&1=2&2=1&3=0&4=1&5=1&6=1&7=1
 *   -> The relay number can only be an integer between 0 and 7
 *   -> 0 turns the relay off
 *   -> 1 turns the relay on
 *   -> 2 changes the status of the relay. So a turned off relay would be 
 *        switched on and the other way around
 * - A GET request to /api/about will show a JSON with the version of the sketch 
 *   running on the board.
 *   e.g.  {"version" : "0.2.1" }
 *
 * Additionally it is implementing a little bit of security like the maximum 
 * request size (default set to 512 bytes).
 * Basic authentication is not used, because it gives the user only a false 
 * sense of security. Everyone can listen to the plain network are see the 
 * credentials.
 *
 * Hardware and Libraries used:
 * - An Ethernet Shield with the ENC28J60 chip, which requires the library
 *   See https://github.com/ntruchsess/arduino_uip or 
 *   http://playground.arduino.cc/Hardware/ArduinoEthernet
 * - An 8 Channel 5V Relay Module
 *
 * Error messages:
 * The application only returns error codes and a HTTP 500 error to save the 
 * already very limited memory.
 * - 1 = The request is too big, please send one with less bytes or check 
 * 		 the maxSize constant to increase the value.
 * - 2 = Only GET and POST is supported, but something else has been received.
 * - 3 = The command must be at least 3 chars long! e.g. 0=1
 * - 4 = The application only supports relay numbers between 0 and the value in 
 * 		 numRelays. Change it if you have more relays.
 * - 5 = The application only supports the values 0 for off, 1 for on and 2 to 
 * 		 invert the status, everything else will not work!
 * - 6 = Json parsing fail on request content
 *
 * created 04 Jan 2015 - by Iwan Zarembo <iwan@zarembo.de>
 */

// the debug flag during development
// decomment to enable debugging statements
#define DEBUGGING;

#define _VERSION "0.5"

/*** The configuration of the application ***/
const int relayPins[NUM_RELAYS] PROGMEM = {2,3,4,5,6,7,8,9};
RelayService relayService(relayPins); // pin number each relay

// the get requests the application is listening to
const char RESOURCE_ABOUT[] PROGMEM = "/api/v ";
const char RESOURCE_STATE[] PROGMEM = "/api/r ";
const char RESOURCE_NAME[] PROGMEM = "/api/n ";
const char RESOURCE_HOME_PAGE[] PROGMEM = "/ ";

/* ************ internal constants *************/
const char VIEW_ABOUT[] PROGMEM = "{\"version\":\""_VERSION"\"}";

// RESPOSTA HTML
const char VIEW_HOME_PAGE[] PROGMEM = 
"<!DOCTYPE html>\n"
"<html lang=\"en\">"
"<head>"
 "<meta charset=\"utf-8\"/>"
 "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"/>"
 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
 "<title>RELENET</title>"
 "<style>"
  "body{background-color:black;}"
  ".content{\n"
   "margin:auto;\n"
   "width:99%\n"
  "}\n"
  "ul{list-style-type:none;margin:0;padding:0}\n"
  "li{\n"
   "float:left;\n"
   "width:43%;\n"
   "min-width:130px;\n"
   "max-width:160px;\n"
   "height:60px;\n"
   "border:1px solid ligthslategray;\n"
   "color:white;\n"
   "font-weight:bold;\n"
   "margin:0.3em 0.3em;\n"
   "padding:5px;\n"
  "}\n"
  "hr{clear:left;}\n"
  "button{display:block;margin:1em;border:1px solid lightslategray;font-weight:bold;font-family:\"Arial\"}\n"
  ".on{background-color:darkorange}\n"
  ".off{background-color:darkblue}\n"
  ".ld{background-color:darkgray}\n"
  ".fail{background-color:darkred}\n"
  ".small{\n"
   "font-size:x-small;\n"
   "color:gray;\n"
  "}\n"
"</style>\n"
"</head>\n"

"<body>\n"

"<div class=\"content\">\n"
  "<ul id=\"r\">\n"
  "</ul>\n"
  "<hr/><p class=\"small\">Relenet "_VERSION"</p>\n"
"</div>\n"

"<script>\n"
"var d=document;\n"
"var onrs=function(){\n"
//   "console.log(this);\n"
    "if(4!=this.readyState){\n"
       "return;\n"
    "}\n"
    "if(200!=this.status){\n"
       "var li=d.querySelector(\".ld\");\n"
       "li.className=\"fail\";\n"
       "return;\n"
    "}"
    "procResp(this.response);\n"
  "};"
  
"function procResp(resp){\n"
   "var ul=d.getElementById(\"r\");\n"
   "var rls=JSON.parse(resp);\n"
   "for (var i=0;i<rls.length;i++) (function(rl){\n"
     "var li=d.getElementById(\"r\"+rl.id);\n"
     "if(!li){\n"
       "var btn=d.createElement(\"BUTTON\");\n"
       "btn.type=\"button\";\n"
       "btn.onclick=function(){r_st(rl.id,(rl.s^1));};\n"
       "li=d.createElement(\"LI\");\n"
       "ul.appendChild(li);\n"
       "li.id=\"r\"+rl.id;\n"
       "li.appendChild(d.createTextNode(rl.n));\n"
       "li.appendChild(btn);\n"
     "}\n"
     "li.className=(rl.s&1)?\"on\":\"off\";\n"
     "li.querySelector(\"button\").textContent=(rl.s&1)?\"ON\":\"OFF\";\n"
   "}(rls[i]));\n"
"}\n"

"function ajax(m,a,d){\n"
  "var rqp=new XMLHttpRequest();\n"
  "rqp.open(m,a,true);\n"
  "rqp.setRequestHeader('Content-Type','application/json;charset=UTF-8');\n"
  "rqp.onreadystatechange=onrs;\n"
  "setTimeout(function(){rqp.abort()},10000);\n"
  "rqp.send(d);\n"
"}\n"

"function r_st(id,st){\n"
  "var li=d.getElementById(\"r\"+id);\n"
  "if (li){li.className=\"ld\";}\n"
  "ajax('POST','/api/r','{\"'+id+'\":2}');\n"
"}\n"

"ajax('GET','/api/r',null);\n"
"</script>\n"

"</body>\n"
"</html>\n";

class RelayStateChangeCtrl: public WebController {
public:
  void execute(WebDispatcher &webDispatcher, WebRequest &request);
} relayStateChangeCtrl;

class RelayNameChangeCtrl: public WebController {
public:
  void execute(WebDispatcher &webDispatcher, WebRequest &request);  
} relayNameChangeCtrl;

RedirectToViewCtrl redirectToHTMLCtrl(CONTENT_TYPE_HTML),redirectToJSONCtrl(CONTENT_TYPE_JSON);

#define NUM_ROUTES 4
const WebRoute routes[ NUM_ROUTES ] PROGMEM = {
  {RESOURCE_ABOUT,&redirectToJSONCtrl,VIEW_ABOUT},
  {RESOURCE_STATE,&relayStateChangeCtrl,NULL},
  {RESOURCE_NAME,&relayNameChangeCtrl,NULL},
  {RESOURCE_HOME_PAGE,&redirectToHTMLCtrl,VIEW_HOME_PAGE}
};

// start the server on port 80
EthernetServer server = EthernetServer(80);

void setup() {
	#ifdef DEBUGGING
		Serial.begin(9600);

                Serial.print(F("\nEEPROM:"));
                for (int i=0; i<128; i++) {
                    if (!(i%16)==0) {
                      Serial.print(" ");
                    } else {
                      Serial.println();
                    }
                    Serial.print(EEPROM.read(i));
                }
                
                // relay names
                char dest[10];
                for (uint8_t i=0; i<NUM_RELAYS; i++) {
                  relayService.getName(i,dest);
                  Serial.print(F("\nNAME["));
                  Serial.print(i);
                  Serial.print(F("]: "));
                  Serial.println(dest);                  
                  Serial.println(EE_checksum(12+(i*11),10));
                }
	#endif
	
        // inicia random com algo bem aleatorio
        randomSeed(analogRead(7));
        
        uint8_t ip[4], mac[6];
        if (!EE_getMAC(mac)) {
          // sorteia um MAC novo para o endereco e grava na EEPROM
          for (int i=0;i<6;i++) {
             long num = random(256*255);
             Serial.print(num);
             Serial.print(F(";"));
             mac[i]=(uint8_t)num;
           }
           Serial.println();
          // grava na eeprom
          EE_saveMAC(mac);
        }
	if (!EE_getIP(ip)) {;
          // Change the configuration for your needs
          ip[0]=192;ip[1]=168;ip[2]=1;ip[3]=20;
        }
        IPAddress myIP(ip[0],ip[1],ip[2],ip[3]);        
        Ethernet.begin(mac, myIP);
	#ifdef DEBUGGING
		Serial.print(F("Local IP: "));
		Serial.println(Ethernet.localIP());
                Serial.print(F("     MAC: "));
                for (int i=0;i<6;i++) {
                  if (i>0) Serial.print(F(":"));
                  Serial.print(mac[i]);
                }
                Serial.println();
	#endif
	server.begin();
}

void loop() {
  processWebRequests();
//  processSerialRequests(); 
}

void processWebRequests() {
  /* Crio o WebDispatcher local a este funcao para uso racional de memoria. Assim, todo o 
        MVC eh criado na memoria da pilha e liberado ao final desta funcao. */
  WebDispatcher webDispatcher(server);
  webDispatcher.setRoutes(routes,NUM_ROUTES);
  webDispatcher.process();
} // end loop

/**
 * callback for jsonparse when changename.
 * Se retornar qualquer coisa diferente de 0, para o processamento e retorna na hora.
 *
 */
int jsonDecoderChangeStatus(int jsonElementType, void* value,void*context) {
  ChangeStatusRequest &chgStatusReq = *((ChangeStatusRequest*)context);
  switch (jsonElementType) {
    case JSON_ELEMENT_OBJECT_KEY: {
	char *key = (char*)value; // alias to point of char.
	if (strlen(key)!=1) {
		return ERROR_INVALID_RELAY_ID; 
	}
        chgStatusReq.numRelay=(key[0]-'0');
        break;
    }
    case JSON_ELEMENT_NUMBER_LONG: {
	chgStatusReq.newOnOffStat=*((int*)value);
        return relayService.changeStatus(chgStatusReq);
	break;
    }
    default:
       chgStatusReq.numRelay=100; // force to a invalid num relay
  }

  return JSON_ERR_NO_ERROR;
}

void sendJsonViaWeb(WebDispatcher &webDispatcher, WebRequest &request, uint8_t err, void (*sendJson_fn) (EthernetClient&)) {
	request.response.contentType_P=CONTENT_TYPE_JSON;
	if (err) {
		request.response.httpStatus=RC_BAD_REQ;
	} else {
		request.response.httpStatus=RC_OK;
	}
	webDispatcher.sendHeader(request);
	sendJson_fn(request.client);
}

void RelayStateChangeCtrl::execute(WebDispatcher &webDispatcher, WebRequest &request) {
#ifdef DEBUGGING
	Serial.println(F("StCtrl"));
#endif
       	int err=0;
        if (request.method==METHOD_POST) {
	        char jsonStr[50];
		if (webDispatcher.getNextLine(request.client,jsonStr,50)) {
			#ifdef DEBUGGING
				Serial.print(F("Processing relays: ")); Serial.println(jsonStr);
			#endif
	
			ChangeStatusRequest chgStatusReq;
			SmcfJsonDecoder jsonDecoder;
			err=jsonDecoder.decode(jsonStr, jsonDecoderChangeStatus, &chgStatusReq);
		} else {
			err=3;//todo: corrigir codigo de erro
		}
	} // if POST
	sendJsonViaWeb(webDispatcher, request,err,printJsonRelays);
}

/**
 * callback for jsonparse when changename.
 * Se retornar qualquer coisa diferente de 0, para o processamento e retorna na hora.
 *
 */
int jsonDecoderChangeName(int jsonElementType, void* value,void*context) {
  ChangeNameRequest &chgNameReq = *((ChangeNameRequest*)context);
  switch (jsonElementType) {
    case JSON_ELEMENT_OBJECT_KEY: {
	char *key = (char*)value; // alias to point of char.
	if (strlen(key)!=1) {
		return ERROR_INVALID_RELAY_ID; 
	}
        chgNameReq.numRelay=(key[0]-'0');
        break;
    }
    case JSON_ELEMENT_STRING: {
	chgNameReq.newName=(char*)value;
        return relayService.changeName(chgNameReq);
	break;
    }
    default:
       chgNameReq.numRelay=100; // force to a invalid num relay
  }

  return JSON_ERR_NO_ERROR;
}

void RelayNameChangeCtrl::execute(WebDispatcher &webDispatcher, WebRequest &request) {
#ifdef DEBUGGING
	Serial.println(F("NameCtrl"));
#endif
        int err=0;
	if (request.method==METHOD_POST) {	
		char jsonStr[50];
		if(webDispatcher.getNextLine(request.client,jsonStr,sizeof(jsonStr))) {
			#ifdef DEBUGGING
				Serial.print(F("ChgName: ")); Serial.println(jsonStr);
			#endif
 			ChangeNameRequest chgNameReq;
			SmcfJsonDecoder jsonDecoder;
			int err=jsonDecoder.decode(jsonStr, jsonDecoderChangeName, &chgNameReq);
		} else err=3; // não veio os dados do post. todo: corrigir o codigo de erro
	}
	sendJsonViaWeb(webDispatcher,request,err,printJsonRelays);
}

/**
 * Returns a JSON with the current names e status of the relays to the client. The
 * JSON will look like: [{"id":0,"n":"COZINHA","s":0},{"id":1,"n":"VARANDA","s":0}] ...
 */
void printJsonRelays(EthernetClient &client) {
  #ifdef DEBUGGING
    Serial.print(F("rns"));
  #endif
	client.print('[');
	for (int i = 0; i < NUM_RELAYS; i++) {
		if (i!=0) {
			client.print(',');
		}
                client.print(F("{\"id\":"));
                client.print(i);
                client.print(F(",\"n\":\""));
		char name[10];
		relayService.getName(i,name);
		client.print(name);
		client.print(F("\",\"s\":"));
		client.print(relayService.getStatus(i));
		client.print('}');
	}
	client.println(']');
}

