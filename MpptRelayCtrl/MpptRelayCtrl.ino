/*
MODIFIED by Sergio M C Figueiredo

History:
v. 0.2.1 - Original Nano_WebRelay8.ino (got from http://playground.arduino.cc/Code/NanoWebRelay8)
v. 0.3 - Serves HTML Page for use in browsers (mobile compatible);
v. 0.4 - Communication with EPSolar Tracer 2210RN MPPT Solar Controller port to MT-5. Code based from http://www.lekermeur.net/lndkavr/sources/lndk_avra08/Lndk_Avra08.ino
         More instructions at: http://blog.lekermeur.net/?p=2052
-----------
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
 *
 * created 04 Jan 2015 - by Iwan Zarembo <iwan@zarembo.de>
 */

// the debug flag during development
// decomment to enable debugging statements
//#define DEBUGGING 0;

#define _VERSION "0.3"

#include <Arduino.h>
#include <UIPEthernet.h>
#include <UIPServer.h>
#include <UIPClient.h>
#include <Metro.h>

#ifdef DEBUGGING
	#include <MemoryFree.h>
#endif

/*** The configuration of the application ***/
// Change the configuration for your needs
const uint8_t mac[6] = { 0xDE, 0xAE, 0xBD, 0x12, 0xFE, 0xED };
const IPAddress myIP(192, 168, 1, 20);

// Number of relays on board, max 9 are supported!
const int numRelays = 8;
// Output ports for relays, change it if you connected other pins, must be adjusted if number of relays changed
int outputPorts[] = { 2, 3, 4, 5, 6, 7, 8, 9 };
// this is for performance, must be adjusted if number of relays changed
boolean portStatus[] = { false, false, false, false, false, false, false, false };

/****** a little bit of security *****/
// max request size, arduino cannot handle big requests so set the max you really plan to process
const int maxSize = 512; // that is enough for 8 relays + a bigger header

// the get requests the application is listening to
const char REQ_ABOUT[] = "/api/about";
const char REQ_RELAY[] = "/api/r";
const char REQ_MPPT[] = "/api/mppt";

// Request parameters
const char GET[]  = "GET";
const char POST[] = "POST";

// http codes
const int RC_ERR = 500;
const int RC_OK  = 200;

// supported relay status
const int R_OFF = 0;
const int R_ON  = 1;
const int R_INV = 2;

/* ************ internal constants *************/
// post data tokenizer
const char TOK[] = "=&";

// new line
const char NL = '\n';
// carriage return
const char CR = '\r';

// JSON RESPONSES
// response of relay status, json start
const char RS_START[] = "{\"r\":[";
// response of relay status, json end
const char RS_END[] = "]}";
// response of relay status, json array separator;
const char RS_SEP = ',';

// response of the error JSON start
const char RS_ERR_START[] = "{\"e\":";
// response of the error JSON end
const char RS_ERR_END = '}';

// HEADERS RESPONSES
const char HD_START[] = "HTTP/1.1 ";
const char HD_END[] = " \nContent-Type: text/html\nConnection: close\n\n";

// RESPOSTA HTML
const char PRGM_HOME_PAGE[] PROGMEM = 
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
 "<meta charset=\"utf-8\"/>\n"
 "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"/>\n"
 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>\n"
 "<title>RELENET</title>\n"
 "<style>\n"
  "body{background-color:black;}\n"
  ".content{\n"
   "margin:auto;\n"
   "width:98%\n"
  "}\n"
  "ul{list-style-type:none;}\n"
  "li{\n"
   "float:left;\n"
   "width:140px;\n"
   "height:60px;\n"
   "max-width:80%;\n"
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
"<script>\n"
"var d=document;\n"
"var rlnm=['COZINHA','VARANDA','TV','TGU','INT1','INT2','INT3','INT4'];\n"

"var onrs=function(){\n"
   "console.log(this);\n"
    "if(4!=this.readyState){\n"
       "return;\n"
    "}\n"
    "if(200!=this.status){\n"
       "var li=d.querySelector(\".ld\");\n"
       "li.className=\"fail\";\n"
       "return;\n"
    "}\n"
    "procResp(this.response);\n"
  "};\n"
  
"function procResp(resp){\n"
   "var ul=d.getElementById(\"r\");\n"
   "var rls=JSON.parse(resp);\n"
   "for (var i=0;i<rls.r.length;i++) (function(rl,i){\n"
     "var li=d.getElementById(\"r\"+i);\n"
     "if (!li){\n"
       "var btn=d.createElement(\"BUTTON\");\n"
       "btn.type=\"button\";\n"
       "btn.onclick=function(){r_st(i,(rl[i]^1));};\n"
       "li=d.createElement(\"LI\");\n"
       "ul.appendChild(li);\n"
       "li.id=\"r\"+i;\n"
       "li.appendChild(d.createTextNode(rlnm[i]));\n"
       "li.appendChild(btn);\n"
     "}\n"
     "li.className=(rl[i]&1)?\"on\":\"off\";\n"
     "li.querySelector(\"button\").textContent=(rl[i]&1)?\"ON\":\"OFF\";\n"
   "}(rls.r,i));\n"
"}\n"

"function ajax(m,a,d){\n"
  "var rqp=new XMLHttpRequest();\n"
  "rqp.open(m,a,true);\n"
  "rqp.setRequestHeader('Content-Type','application/json;charset=UTF-8');\n"
  "rqp.onreadystatechange=onrs;\n"
  "setTimeout(function(){rqp.abort()},20000);\n"
  "rqp.send(d);\n"
"}\n"

"function r_st(id,st){\n"
  "var li=d.getElementById(\"r\"+id);\n"
  "if (li){li.className=\"ld\";}\n"
  "ajax('POST','/',id+'=2');\n"
"}\n"

"function r_get() {\n"
  "ajax('GET','/api/r',null);\n"
"}\n"
"</script>\n"

"<div class=\"content\">\n"
  "<ul id=\"r\">\n"
  "</ul>\n"
  "<hr/><p class=\"small\">WebRelay8 "_VERSION"</p>\n"
"</div>\n"

"<script>\n"
  "r_get();\n"
"</script>\n"

"</body>\n"
"</html>\n";

const char VERSION[] = "{\"version\":\""_VERSION"\"}";

// start the server on port 80
EthernetServer server = EthernetServer(80);

// *** Communication with EPSOLAR MPPT Solar Controller ***
#define TRACER_DEVICE	0x01

// SERIAL BUFFER
#define SER_SIZE 	96
const uint8_t MT5_CMD_STATUS[] PROGMEM = {0xEB,0x90,0xEB,0x90,0xEB,0x90,0x01,0xA0,0x00,0x6F,0x52,0x7F};
/*            for (ij = 0; ij < 3; ij ++) {
               Serial.write(0xEB); Serial.write(0x90);
            }
            Serial.write(0x01);    // Adresse
            Serial.write(0xA0);    // Commande
            Serial.write(0x00);    // Longueur de donnees
            Serial.write(0x6F);    // Check sum
            Serial.write(0x52);    // Check sum
            Serial.write(0x7F);*/
static uint8_t serbuf[SER_SIZE] = "";
int serind;
Metro mpptSerialMetro = Metro(10000);

struct MPPT_STATUS {
    uint16_t 
      mbat, 
      msol, 
      mcon, 
      modv, 
      mbfv,
      mcur;
    uint8_t 
      mlod, 
      movl, 
      mlsc, 
      msoc, 
      mbol, 
      mbod, 
      mful, 
      mchg,
      mtmp;
} mppt_status;
// 

void setup() {
	#ifdef DEBUGGING
		Serial.begin(9600);
	#endif
	
	// all pins will be turned off
	for (int i = 0; i < numRelays; i = i + 1) {
		pinMode(outputPorts[i], OUTPUT);
		digitalWrite(outputPorts[i], LOW);
	}
	
	Ethernet.begin(mac, myIP);
	#ifdef DEBUGGING
		Serial.print("Local IP: ");
		Serial.println(Ethernet.localIP());
	#endif
	server.begin();
}

void loop() {

	if (EthernetClient client = server.available()) {
		if (client) {
			size_t size;
			if((size = client.available()) > 0) {
				// check the the data is not too big
				if(size > maxSize) {
					returnHeader(client, RC_ERR);
					returnErr(client, 1);
				}
				String buffer = "";
				char c;
				#ifdef DEBUGGING
					Serial.println("New client request");
				#endif
				// read all data
				while (client.connected() && client.available()) {
					buffer = getNextLine(client);
					if (buffer.startsWith(GET)) {
						returnHeader(client, RC_OK);
						String getData = buffer.substring(4, buffer.length() - 9);
						#ifdef DEBUGGING
							Serial.print("Get Data: "); Serial.println(getData);
						#endif
						if (getData.equals(REQ_ABOUT)) {
							client.println(VERSION);
						} else if (getData.equals(REQ_RELAY)) {
							printRelayStatus(client);
						} else if (getData.equals(REQ_MPPT)) {
							printMpptStatus(client);
						} else {
							printHomePage(client);
						}
						break;
					} else if (buffer.startsWith(POST)) {
						analyzePostData(client);
						break;
					} else {
						// only get and post are supported
						returnHeader(client, RC_ERR);
						returnErr(client, 2);
					}
				} // end while
	
				#ifdef DEBUGGING
					Serial.println("Stopping the client");
				#endif
				delay(1);
				client.stop();
				#ifdef DEBUGGING
					//Serial.print("freeMemory()="); Serial.println(freeMemory());
				#endif
			} // end if request size bigger than 0
		} // end if client
	} // end if server available

      if (mpptSerialMetro.check()==1) {
            // send a command to mppt to receive status
            int ij;
            for (ij = 0; ij < sizeof(MT5_CMD_STATUS); ij ++) {
               int8_t myByte =  pgm_read_byte_near(&MT5_CMD_STATUS[0] + ij);
               Serial.write(myByte);
            }
      }

      // ---------------------------------------------------------------
      // Traitement des caracteres recus sur le port serie
      // ---------------------------------------------------------------
      while (Serial.available() > 0) {
          // get the new byte:
         char *ptr;
         ptr = (char *) &serbuf;
         ptr = ptr + serind;
         if (Serial.readBytes(ptr, 1) == 1) {
            serind++;
         }
         if ((serind == 36) && (serbuf[35] == 0x7F)) Mesures_Tracer();
         if (serind >= SER_SIZE) serind = 0;
      }

} // end loop


/**
 *
 * manda para a Ethernet uma string na memoria de programa.
 *
 */
void printPrgMem(EthernetClient &client, const prog_char* msg) {
#ifdef DEBUGGING
  Serial.print("PrintPrgMem!");
#endif
  char myChar;
  int len = strlen_P(msg);
  for (int k = 0; k < len; k++)
  {
    myChar =  pgm_read_byte_near(msg + k);
    client.write(myChar);
  }
#ifdef DEBUGGING
  Serial.print("PrintPrgMem.");
#endif
}

/**
 * Returns a HTML.
 */
void printHomePage(EthernetClient &client) {
  printPrgMem(client, &PRGM_HOME_PAGE[0]);
}

/**
 * It will get the data in case of a post request, by reading the last line.
 */
String getPostData(EthernetClient &client) {
	#ifdef DEBUGGING
		Serial.println("Reading post data from client.");
	#endif
	char c;
	while (client.connected() && client.available())  {
		c = client.read();
		// ignore all \r characters
		if (c != CR) {
			// not read until you have a line break
			if (c == NL) {
				// is the next char a carriage return or a line break?
				c = client.read();
				if (c == CR) {
					// then ignore it and read the next character
					c = client.read();
				}
				if (c == NL) {
					// read the line with the post data
					return getNextLine(client);
				}
			} // end if C == NL
		} // end c != CR
	} // end while
	return "";
}

/**
 * Performs the change on the relay itself.
 */
bool processRelayChange(int relay, int onOffStat) {
	#ifdef DEBUGGING
		Serial.print("The target status of the relay "); Serial.print(relay);
		Serial.print(" is: "); Serial.println(onOffStat);
	#endif
	if(onOffStat == R_OFF) {
		// only turn something of if it is turn on, otherwise ignore it.
		if(portStatus[relay] != false) {
			// turn it off
			digitalWrite(outputPorts[relay], LOW);
			portStatus[relay] = false;
		}
	} else if(onOffStat == R_ON) {
		// only turn something of if it is turn on, otherwise ignore it.
		if(portStatus[relay] != true) {
			// turn it off
			digitalWrite(outputPorts[relay], HIGH);
			portStatus[relay] = true;
		}
	} else if(onOffStat == R_INV) {
		bool target = !portStatus[relay];
		#ifdef DEBUGGING
			Serial.print("Curr status is:"); Serial.println(portStatus[relay]);
			Serial.print("Target status is:"); Serial.println(target);
		#endif
		digitalWrite(outputPorts[relay], target ? HIGH : LOW);
		portStatus[relay] = target;
	} else {
		return false;
	} // end switching a relay
	return true;
}

/**
 * Performs the status change on the relais. The POST data must follow the 
 * pattern: relay number = 0 or 1 or 2 => e.g. 0=1&1=2&2=1&3=0&4=1&5=1&6=1&7=1
 *   -> The relay number can only be an integer between 0 and the constant numRelays-1
 *   -> 0 turns the relay off
 *   -> 1 turns the relay on
 *   -> 2 changes the status of the relay. So a turned off relay would be switched on and the other way around.
 * The result of this method can be:
 * 	1. A HTTP 500 error if something went wrong processing the command.
 * 	2. A JSON String as you would get when sending a simple GET request.
 */
void analyzePostData(EthernetClient &client) {
	String command = getPostData(client);
	
	#ifdef DEBUGGING
		Serial.print("Processing relays: "); Serial.println(command);
	#endif
	
	int idx;
	while ( (idx = command.indexOf("&")) != -1 || command.length() == 3) {
		String param = command.substring(0, idx);
		if (param.length() != 3) {
			returnHeader(client, RC_ERR);
			returnErr(client, 3);
			break;
		} else {
			// get the relay number, this might fail, if not a number.
			int relay = param.charAt(0) - '0';
			#ifdef DEBUGGING
				Serial.print("Found index: "); Serial.println(idx);
				Serial.print("Current substring: "); Serial.println(param);
				Serial.print("Found Relay: "); Serial.println(relay);
			#endif
			// we only have 8 relays, so the number can only be between 0 and 7.
			if (relay < numRelays && relay >= 0) {
				int onOff = param.charAt(2) - '0';
				bool ok = processRelayChange(relay, onOff);
				if(!ok) {
					// stop processing, an error occurred!
					returnHeader(client, RC_ERR);
					returnErr(client, 5);
					return;
				}
			} // end relay check
			// make it smaller
			if (idx == -1) {
				// stop the loop as soon the last part without end is done
				break;
			}
			command = command.substring(idx + 1, command.length());
			#ifdef DEBUGGING
				Serial.print("Left Command: ");
				Serial.println(command);
			#endif
		} // end if param.length < 3
	} // end while loop

	// return the final status of all relays
        returnHeader(client, RC_OK);
	printRelayStatus(client);
}

/**
 * Returns a JSON with the current values of the relays to the client. The
 * JSON will look like: {"r":[0,0,0,0,0,0,0,0]}
 * This one means all releays are turned off.
 */
void printRelayStatus(EthernetClient &client) {
	client.print(RS_START);
	int i = 0;
	int lastRelay = numRelays-1;
	for (i = 0; i < numRelays; i++) {
		client.print(portStatus[i]);
		if(i < lastRelay) {
		  client.print(RS_SEP);
		}
	}
	client.println(RS_END);
}

/**
 * Returns a JSON with the current values of the mppt status to the client. The
 * JSON will look like: {"r":[0,0,0,0,0,0,0,0]}
 */
void printMpptStatus(EthernetClient &client) {
	client.print(MS_START);
	int i = 0;
	int lastRelay = numRelays-1;
	for (i = 0; i < numRelays; i++) {
		client.print(portStatus[i]);
		if(i < lastRelay) {
		  client.print(RS_SEP);
		}
	}
	client.println(MS_END);
}

/**
 * Returns a message to the client.
 */
void returnErr(EthernetClient &client, int rc) {
	client.print(RS_ERR_START);
	client.print(rc);
	client.println(RS_ERR_END);
}

/**
 * Returns a header with the given http code to the client.
 */
void returnHeader(EthernetClient &client, int httpCode) {
      
      mppt_status.mbat = (serbuf[10] << 8) | serbuf[9];
      mppt_status.msol = (serbuf[12] << 8) | serbuf[11];
      mppt_status.mcon = (serbuf[16] << 8) | serbuf[15];
      mppt_status.modv = (serbuf[18] << 8) | serbuf[17];
      mppt_status.mbfv = (serbuf[20] << 8) | serbuf[19];
      mppt_status.mlod = serbuf[21];
      mppt_status.movl = serbuf[22];
      mppt_status.mlsc = serbuf[23];
      mppt_status.msoc = serbuf[24];
      mppt_status.mbol = serbuf[25];
      mppt_status.mbod = serbuf[26];
      mppt_status.mful = serbuf[27];
      mppt_status.mchg = serbuf[28];
      mppt_status.mtmp = serbuf[29];
      mppt_status.mcur = (serbuf[31] << 8) | serbuf[30];

      // battery voltage * 100
      printPrgMem(client, PSTR("{\"batV\":"));      
      client.print(mppt_status.mbat);
      // panel fotovoltaico voltage * 100
      printPrgMem(client, PSTR(",\"pvV\":"));      
      client.print(mppt_status.msol);
      // load current * 100
      printPrgMem(client, PSTR(",\"loadA\":"));      
      client.print(mppt_status.mcon);
      // over discharge voltage * 100
      printPrgMem(client, PSTR(",\"ovDisV\":"));      
      client.print(mppt_status.modv);
      // battery full voltage * 100
      printPrgMem(client, PSTR(",\"batFullV\":"));      
      client.print(mppt_status.mbfv);
      // load on/off (0-off;1-on)
      printPrgMem(client, PSTR(",\"loadON\":"));      
      client.print(mppt_status.mlod);
      // over load (0-no;1-yes)
      printPrgMem(client, PSTR(",\"ovLoad\":"));      
      client.print(mppt_status.movl);
/*      // load short circuit
      printPrgMem(client, PSTR(",\"loadShortCircuit\":"));      
      client.print(mppt_status.mlsc);*/
      // full indicador
      printPrgMem(client, PSTR(",\"full\":"));      
      client.print(mppt_status.mful);
      // charging indicador
      printPrgMem(client, PSTR(",\"chg\":"));      
      client.print(mppt_status.mchg);
      // chargind current * 100
      printPrgMem(client, PSTR(",\"chgA\":"));      
      client.print(mppt_status.mcur);
      // temperature (Celsius + 30)
      printPrgMem(client, PSTR(",\"temp\":"));      
      client.print(mppt_status.mtmp);
      client.write('}'); // end json message
}

/**
 * Reads the next line from the client.
 * Sample POST:
 * POST / HTTP/1.1
 * Host: 192.168.178.22
 * User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:33.0) Gecko/20100101 Firefox/33.0
 * Accept: text/html,application/xhtml+xml,application/xml;q=0.9,* /*;q=0.8
 * Accept-Language: de
 * Accept-Encoding: gzip, deflate
 * DNT: 1
 * Content-Type: text/xml; charset=UTF-8
 * Content-Length: 4
 * Connection: keep-alive
 * Pragma: no-cache
 * Cache-Control: no-cache
 *
 * 1=1&2=0
 * end -----------------------
 *
 * Sample GET:
 * GET /?1=0 HTTP/1.1
 * Host: 192.168.178.22
 * User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:33.0) Gecko/20100101 Firefox/33.0
 * Accept: text/html,application/xhtml+xml,application/xml;q=0.9,* /*;q=0.8
 * Accept-Language: de
 * Accept-Encoding: gzip, deflate
 * DNT: 1
 * Connection: keep-alive
 * end -----------------------
 */
String getNextLine(EthernetClient &client) {
	char c;
	String buffer;
	while (client.connected() && client.available()) {
		c = client.read();
		// ignore all \r characters
		if (c != CR) {
			// not read until you have a line break
			if (c == NL) {
				// every entry in in a certain line
				return buffer;
			} else {
				buffer += c;
			}
		} // end c != CR
	} // end while
	return buffer;
}


/* ------------------------------------   S e n d _ T r a c e r _ C o m m a n d
 
void Send_Tracer_Command(uint8_t command, uint8_t datalen) {
   int ij;
   uint16_t	crc;
   msgbuf[0] = TRACER_DEVICE; 		//
   msgbuf[1] = command;                 // Commande
   msgbuf[2] = datalen;                 // Longueur de donnees
   for (ij = 0; ij < datalen; ij ++ ) {
      msgbuf[ij + 3] = tcmd[ij];        // Parametres s'il y a lieu
   }
   ij = datalen + 3;
   msgbuf[ij ++] = 0;
   msgbuf[ij ++] = 0;
   msgbuf[ij] = 0x7F;
   // Calcul et placement du checksum
   crc = Crc16(msgbuf, datalen + 5);
   msgbuf[datalen + 3] = (crc >> 8);
   msgbuf[datalen + 4] = (crc & 0xFF);
   for (ij = 0; ij < 3; ij ++) {
#ifdef SERIALDEBUG
      if (VinOn && (SerialDebug == 0)) {
#else
      if (VinOn) {
#endif
         Serial.write(0xEB); Serial.write(0x90);
      }
      else {
         Serial.print(0xEB, HEX); Serial.print(' '); 
         Serial.print(0x90, HEX); Serial.print(' ');
      }
   } 
   for (ij = 0; ij < (datalen + 6); ij ++) {
#ifdef SERIALDEBUG
      if (VinOn && (SerialDebug == 0)) 
#else
      if (VinOn) 
#endif
         Serial.write(msgbuf[ij]);
      else {
         // Forcer formatage hexa en 02X
         if (msgbuf[ij] < 16) Serial.print('0');
         Serial.print(msgbuf[ij], HEX);
         Serial.write(' ');
      }
   }
#ifdef SERIALDEBUG
   if ((! VinOn) || (SerialDebug > 0)) Serial.println();
#else
   if (! VinOn) Serial.println();
#endif
}
*/
/* ----------------------------------------------------------------   C r c 1 6
 */
uint16_t Crc16(uint8_t *buff, uint8_t len) {
   uint8_t ij, ik, r1, r2, r3, r4;
   uint16_t result;
   r1 = *buff++;
   r2 = *buff++;
   for (ij = 0; ij < (len - 2); ij ++ ) {
      r3 = *buff++;
      for (ik = 0; ik < 8; ik ++ ) {
         r4 = r1;
         r1 = r1 << 1;
         if ((r2 & 0x80) != 0) r1++;
         r2 = r2 << 1;
         if ((r3 & 0x80) != 0) r2++;
         r3 = r3 << 1;
         if ((r4 & 0x80) != 0) {
            r1 = r1 ^ 0x10;
            r2 = r2 ^ 0x41;
         }
      }
   }
   result = r1;
   result = (result << 8) | r2;
   return result;
}

/* ----------------------------------------------   M e s u r e s _ T r a c e r
   Si une sequence de mesures du regulateur est reconnue, l'enregistrer
 */
void Mesures_Tracer(void) {
   // Detection de mesure, si ok la sauvegarder
   //              5             10             15             20             25             30                36
   // EB 90 EB 90 EB 90 00 A0 18 DB 04 0F 00 00 00 09 00 4B 04 C8 05 01 00 00 25 00 00 00 00 2B 00 00 00 92 74 7F
   if (  (serbuf[0] == 0xEB) && (serbuf[1] == 0x90) 
      && (serbuf[2] == 0xEB) && (serbuf[3] == 0x90)
      && (serbuf[4] == 0xEB) && (serbuf[5] == 0x90)) {
      mppt_status.mbat = (serbuf[10] << 8) | serbuf[9];
      mppt_status.msol = (serbuf[12] << 8) | serbuf[11];
      mppt_status.mcon = (serbuf[16] << 8) | serbuf[15];
      mppt_status.modv = (serbuf[18] << 8) | serbuf[17];
      mppt_status.mbfv = (serbuf[20] << 8) | serbuf[19];
      mppt_status.mlod = serbuf[21];
      mppt_status.movl = serbuf[22];
      mppt_status.mlsc = serbuf[23];
      mppt_status.msoc = serbuf[24];
      mppt_status.mbol = serbuf[25];
      mppt_status.mbod = serbuf[26];
      mppt_status.mful = serbuf[27];
      mppt_status.mchg = serbuf[28];
      mppt_status.mtmp = serbuf[29];
      mppt_status.mcur = (serbuf[31] << 8) | serbuf[30];
      // serbuf[32] reserve
      // serbuf[33] checksum
      // serbuf[34] checksum
      // serbuf[35] 0x7F
   }
}
