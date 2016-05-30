#define FASTLED_ESP8266_RAW_PIN_ORDER

#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <Servo.h>
#include <Ticker.h>
#include <TickerScheduler.h>
#include <FastLED.h>

int baseVersion = 3;
int version = 1;

#define LED_ANIMATION		7
#define LED_ON				6
#define REFRESH_LED			5
#define WIFI_CONNECT		4
#define WIFI_CHECK			3
#define OTA_HANDLE			2
#define WEBSOCKET_HANDLE	1
#define HTTP_HANDLE			0

TickerScheduler taskManager(8);

//#############################################################################################################################################
//###																																		###
//###															LOCK																		###
//###																																		###
//#############################################################################################################################################
#pragma region Lock
int currentState = -1;
int newState = -1;
bool opening = false;

void initLock() {
	newState = EEPROM.read(10);
	currentState = newState;
}

void lock(bool twice) {
	opening = false;
	if (newState < 2) {
		newState = currentState + 1;
		toogleServo(true);
	}
}

void unlock(bool open) {
	if (newState >= 0) {
		if (open && currentState > 0) {
			opening = true;
		}
		else {
			opening = false;
		}
		newState = currentState - 1;
		toogleServo(false);
	}
}

void turnComplete() {
	if (opening && newState >= 0) {
		currentState = newState;
		unlock(newState != 0);
		return;
	}

	if (newState < 0) {
		newState = 0;
	}
	else if (newState > 2) {
		newState = 2;
	}
	currentState = newState;
	operationComplete();
}

void openComplete() {
	if (newState < currentState) {
		currentState = 0;
	}
	else {
		currentState = 2;
	}
	operationComplete();
}

void operationComplete() {
	EEPROM.write(10, currentState);
	EEPROM.commit();
	newState = currentState;
	opening = false;
}

void turnAborted() {
	
	newState = currentState;
	opening = false;
	//operationComplete();
}

void sendState() {
	broadcastWebSocketMessage("{ \"state\": " + String(currentState) + " }");
}

String getState() {
	if (currentState == 0) {
		return  "UNLOCKED";
	}
	else if (currentState == 1) {
		return  "LOCKED";
	}
	else if (currentState == 2) {
		return  "DOUBLE LOCKED";
	}
}
#pragma endregion


//#############################################################################################################################################
//###																																		###
//###																LED																		###
//###																																		###
//#############################################################################################################################################
#pragma region Led
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(24, 14, NEO_GRB + NEO_KHZ800);

int brightness = 60;
int maxBrightness = 200;
const int numberOfLeds = 24;
CRGBArray<numberOfLeds> leds;
int BoardLed = 16;
int currentFrame = 0;
int currentPixel = 0;
bool paused = false;

void initLed() {
	FastLED.addLeds<WS2812B, 14, GRB>(leds, numberOfLeds).setCorrection(CoolWhiteFluorescent);
	FastLED.setBrightness(0);
	fill_solid(leds, numberOfLeds, CRGB::White);
	showLed();
	//taskManager.add(REFRESH_LED, 10, showLed, true);

	taskManager.add(LED_ON, 15, turnLedOn, true);
	taskManager.add(REFRESH_LED, 20, RefreshLed, true);
	//taskManager.add(LED_ANIMATION, 60, stepLed, true);


	//setLedColor(strip.Color(0, 255, 255));
	//strip.show();

	pinMode(BoardLed, OUTPUT);
	digitalWrite(BoardLed, LOW);

	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(BUILTIN_LED, OUTPUT);

	digitalWrite(LED_BUILTIN, HIGH);
}

void turnLedOn() {
	int b = FastLED.getBrightness();
	if (b < brightness) {
		FastLED.setBrightness(b + 1);
		FastLED.show();
	}
	else {
		taskManager.remove(LED_ON);
		//ledStandby();
	}
}

void turnLedOff() {
	for (int i = brightness; i >= 0; i--) {
		FastLED.setBrightness(i);
		FastLED.show();
		delay(8);
	}
}

void updateLed() {

}

void ledConnecting() {

}

void ledStandby() {
	currentFrame = 0;
	taskManager.add(REFRESH_LED, 10, fadeToDark);
}

void fadeToLight() {
	if (currentFrame < 255) {
		fadeLightBy(leds, numberOfLeds, -currentFrame);
		FastLED.show();
		currentFrame++;
	}
	else {
		currentFrame = 0;
		taskManager.remove(REFRESH_LED);
		taskManager.add(REFRESH_LED, 10, fadeToDark);
	}
}

void fadeToDark() {
	if (currentFrame < 255) {
		fadeLightBy(leds, numberOfLeds, currentFrame);
		FastLED.show();
		currentFrame++;
	}
	else {
		currentFrame = 0;
		taskManager.remove(REFRESH_LED);
		taskManager.add(REFRESH_LED, 10, fadeToLight);
	}
}

void ledStartLock() {
	ledStart(true);
}

void ledStartUnlock() {
	ledStart(false);
}

void ledStopLock() {
	ledStop(true);
}

void ledStopUnlock() {
	ledStop(false);
}

void ledStart(bool lock) {

}

void ledStop(bool lock) {
	ledStandby();
}

void showLed() {
	FastLED.show();
}

bool ledMode = false;
int drawCount = 1;
void RefreshLed() {
	if (!paused) {
		if (currentPixel > 23) {
			currentPixel = 0;
		}

		if (!WiFi.isConnected()) {
			for (int i = numberOfLeds - 1; i >= 0; i--) {
				int pixel = currentPixel + i;
				if (pixel > 23) {
					pixel = pixel - 24;
				}

				if ((i >= 0 && i <= 4) || (i >= 11 && i <= 16)) {
					leds[pixel] = CRGB::SpringGreen;
				}
				else {
					leds[pixel] = CRGB::White;
				}
			}
		}
		else {
			for (int i = numberOfLeds - 1; i >= 0; i--) {
				int pixel = currentPixel + i;
				if (pixel > 23) {
					pixel = pixel - 24;
				}

				if (i >= 12) {
					if (currentState == 0) {
						leds[pixel] = CRGB::Green;
					}
					else if (currentState == 1) {
						leds[pixel] = CRGB::Red;
					}
					else if (currentState == 2) {
						leds[pixel] = CRGB::OrangeRed;
					}
				}
				else {
					//if (currentState != newState) {
					//	leds[pixel] = CRGB::White;
					//	leds[pixel].fadeToBlackBy(225);
					//}
					//else {
					//	leds[pixel] = CRGB::Black;
					//}
					leds[pixel] = CRGB::White;
					leds[pixel].fadeToBlackBy(200);
				}
			}
		}

		//if (ledMode) {
		//	if (drawCount <= 19) {
		//		leds[currentPixel] = CRGB::White;
		//	}
		//	else {
		//		leds[currentPixel] = CRGB::Black;
		//	}
		//	//if (drawCount == 24) {
		//	//	ledMode = false;
		//	//	drawCount = 1;
		//	//}
		//	//else {
		//	//	drawCount++;
		//	//}

		//	if (drawCount == 19) {
		//		int pixel = currentPixel += 5;
		//		if (pixel > 23) {
		//			pixel = pixel - 24;
		//		}
		//		currentPixel = pixel;
		//		ledMode = false;
		//		drawCount = 1;
		//	}
		//	else {
		//		drawCount++;
		//	}
		//}
		//else {
		//	if (drawCount <= 18) {
		//		leds[currentPixel] = CRGB::Black;
		//	}
		//	if (drawCount == 18) {
		//		ledMode = true;
		//		drawCount = 1;
		//	}
		//	else {
		//		drawCount++;
		//	}
		//}	

		currentPixel = currentPixel + 1;
		FastLED.show();
	}
}

void stepLed() {
	CRGB temp[numberOfLeds];
	for (int i = 0; i < numberOfLeds; i++) {
		int pixel = i + 1;
		if (pixel >= numberOfLeds) {
			pixel = pixel - numberOfLeds;
		}
		else if (pixel < 0) {
			pixel = numberOfLeds;
		}

		temp[pixel] = leds[i];
	}

	for (int i = 0; i < numberOfLeds; i++) {
		leds[i] = temp[i];
	}

	FastLED.show();
}

void pauseLed() {
	paused = true;
}

void resumeLed() {
	paused = false;
}

void disableLed() {
	taskManager.remove(REFRESH_LED);
}
#pragma endregion


//#############################################################################################################################################
//###																																		###
//###																SENSOR																	###
//###																																		###
//#############################################################################################################################################
#pragma region Sensor
const bool Debug = true;
const int debugDelay = 600;

const int detachDelay = 100; //100;
const int reverseDelay = 160; //160;
const int treshold = 15; //25; MOTOR RUNNING @ 9 - 14
const int maxPower = 30; //35;
const int stopDelay = 350; //300;
const int abortDelay = 2325; //2325;
const int sensorStartDelay = 170; //150;

int baseValue = 820; //820;
int startTime;
int stopTime;
int reverseTime;
int powerTime;
int detachTime;
bool trigger;
bool read;

const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;
int total = 0;

int inputPin = A0;

int smoothValue() {
	total = total - readings[readIndex];
	readings[readIndex] = analogRead(inputPin);
	total = total + readings[readIndex];
	readIndex = readIndex + 1;
	if (readIndex >= numReadings) {
		readIndex = 0;
	}
	return total / numReadings;
}

void readSensor() {
	if (read) {
		if (trigger) {
			int time = millis() - startTime;
			int value = smoothValue();
			if (time >= sensorStartDelay) {
				int power = baseValue - value;
				if (time >= abortDelay) {
					stopSensor();
					stopServo(true);
					turnAborted();
					detachTime = millis();
					return;
				}
				if (power >= treshold) {
					stopSensor();
					reverseTime = millis();
					return;
				}
				if (Debug && time >= debugDelay) {
					stopSensor();
					reverseTime = millis();
					return;
				}
			}

			delay(1);
		}
		else if (reverseTime != 0) {
			int value = smoothValue();
			int power = baseValue - value;
			if (((millis() - reverseTime) >= reverseDelay || Debug || power >= maxPower)) {
				reverseTime = 0;
				if (power >= maxPower) {
					reverse();
					openComplete();
					stopTime = millis();
				}
				else {
					if (!opening) {
						stopServo(true);
						turnComplete();
						detachTime = millis();
					}
					else {
						turnComplete();
					}
				}
			}

			delay(1);
		}
		else if (stopTime != 0) {
			if (((millis() - stopTime) >= stopDelay)) {
				stopTime = 0;
				stopServo(true);
				detachTime = millis();
			}
		}
		else if (detachTime != 0) {
			//pauseLed();
			if (((millis() - detachTime) >= detachDelay)) {
				detachTime = 0;
				detachServo();
				read = false;
				sendState();
				//resumeLed();
			}
		}
	}
}

void startSensor() {
	if (!trigger) {
		powerTime = 0;
		detachTime = 0;
		stopTime = 0;
		reverseTime = 0;
		total = 0;
		readIndex = 0;
		for (int thisReading = 0; thisReading < numReadings; thisReading++) {
			readings[thisReading] = 0;
		}

		read = true;
		trigger = true;
		startTime = millis();
	}
}

void stopSensor() {
	trigger = false;
}
#pragma endregion


//#############################################################################################################################################
//###																																		###
//###																SERVO																	###
//###																																		###
//#############################################################################################################################################
#pragma region Servo
Servo servo;
int brakePower = 35;
int servoPin = 13;
bool running = false;
int speed = 1000;
int center = 1500;

void toogleServo(bool direction) {
	stopSensor();
	attachServo();

	if (direction) {
		counterClockwiseServo();
	}
	else {
		clockwiseServo();
	}
	running = true;
	startSensor();
}

void stopServo(bool brake) {
	//if (brake) {
	//	int ms = servo.readMicroseconds();
	//	if (ms < center) {
	//		servo.write(center + speed + ms);
	//	}
	//	else if (ms > center) {
	//		servo.write(center - speed - ms);
	//	}
	//}
	//delay(brakePower);

	centerServo();
}

void centerServo() {
	servo.writeMicroseconds(center);
}

void clockwiseServo() {
	servo.writeMicroseconds(center - speed);
}

void counterClockwiseServo() {
	servo.writeMicroseconds(center + speed);
}

void reverse() {
	int ms = servo.readMicroseconds();
	if (ms > center) {
		clockwiseServo();
		//stepLedColor(strip.Color(255, 0, 0));
	}
	else if (ms < center) {
		counterClockwiseServo();
		//stepLedColor(strip.Color(0, 255, 0));
	}
}

void detachServo() {
	if (servo.attached()) {
		servo.detach();

		digitalWrite(BoardLed, HIGH);

		/*FastLED.setBrightness(brightness);
		FastLED.show();*/
	}
}

void attachServo() {
	if (!servo.attached()) {
		servo.attach(servoPin);

		digitalWrite(BoardLed, LOW);

		/*FastLED.setBrightness(maxBrightness);
		FastLED.show();*/
	}
}

void initServo() {
	attachServo();
	if (ESP.getResetReason() == "Power on" ||
		ESP.getResetReason() == "External System") {
		servo.writeMicroseconds(center + speed);
		delay(150);
		centerServo();
	}
	else {
		clockwiseServo();
		delay(20);
		counterClockwiseServo();
		delay(30);
		clockwiseServo();
		delay(20);
		centerServo();
	}
	delay(100);
	detachServo();
}

void resetServo() {
	attachServo();
	centerServo();
	delay(100);
	detachServo();
}

#pragma endregion


//#############################################################################################################################################
//###																																		###
//###																WEB SERVER																###
//###																																		###
//#############################################################################################################################################
#pragma region WebServer
int webSocketPort = 1670;
int webServerPort = 80;

ESP8266WebServer server(webServerPort);
WebSocketsServer webSocket = WebSocketsServer(webSocketPort);
String host = "smartlock";
char uptime[400];

void handleClient() {
	server.handleClient();
}

void writeHeaders() {
	taskManager.update();

	int sec = millis() / 1000;
	int min = sec / 60;
	int hr = min / 60;
	int dy = hr / 24;

	snprintf(uptime, 400, "%02d:%02d:%02d:%02d", dy, hr, min % 60, sec % 60);

	server.sendHeader("Cache-Control", "no-cache");
	server.sendHeader("Version", "v" + String(baseVersion) + "." + String(version));
	server.sendHeader("Uptime", uptime);
}

void handleNotFound() {
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for (uint8_t i = 0; i < server.args(); i++) {
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}
	writeHeaders();
	server.send(404, "text/plain", message);
}

void handleRoot() {
	String html = "\
	<html lang=\"en\"> \n\
		<head> \n\
			<meta charset=\"UTF-8\" /> \n\
			<title>Smart Lock</title> \n\
			<meta name=\"viewport\" content=\"width=device-width, height=device-height, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\"> \n\
			<meta name=\"apple-mobile-web-app-capable\" content=\"yes\"> \n\
			<meta name=\"apple-mobile-web-app-status-bar-style\" content=\"black\"/> \n\
			<meta name=\"apple-mobile-web-app-title\" content=\"Lamp\"> \n\
			<link rel=\"apple-touch-icon-precomposed apple-touch-icon shortcut icon\" href=\"http://www.freefavicon.com/freefavicons/network/lock-icon-152-184625.png\"/> \n\
		</head> \n\
		<body> \n\
			<table> \n\
				<tr> \n\
					<td> \n\
						<button name=\"1\" class=\"lock\"> LOCK </button> \n\
						<button name=\"0\" class=\"unlock\"> UNLOCK </button> \n\
					</td> \n\
				</tr> \n\
			</table> \n\
			<script> \n\
				var buttonStates = {}, buttonTexts = {}, buttonClasses = {}, buttons = document.getElementsByTagName('button'), eventName, i; \n\
				for (i = 0; i < buttons.length; i++) { \n\
					buttonTexts[buttons[i].name] = buttons[i].innerText; \n\
					buttonClasses[buttons[i].name] = buttons[i].className; \n\
					typeof window.orientation !== 'undefined' ? eventName = 'ontouchend' : eventName = 'onclick'; \n\
					buttons[i][eventName] = function () { \n\
						var button = this; \n\
						button.className = 'touch'; \n\
						if (!buttonStates[button.name]) { \n\
							buttonStates[button.name] = true; \n\
							button.innerText = '...'; \n\
							var xhttp = new XMLHttpRequest(); \n\
							xhttp.timeout = 1500; \n\
							xhttp.onreadystatechange = function () { \n\
								if (xhttp.readyState == 4) { \n\
									buttonStates[button.name] = false; \n\
									button.className = buttonClasses[button.name]; \n\
									button.innerText = buttonTexts[button.name]; \n\
								} \n\
							}; \n\
							xhttp.ontimeout = function () { \n\
								buttonStates[button.name] = false; \n\
								button.className = buttonClasses[button.name] + ' fail'; \n\
								button.innerText = 'Failed'; \n\
							}; \n\
							xhttp.open('POST', '/?s=' + button.name, true); \n\
							xhttp.send(); \n\
						} \n\
						return false; \n\
					} \n\
				} \n\
			</script> \n\
			<style> \n\
				body { text-align: center; margin: 0; background: #000; } \n\
				table { width: 100%; text-align: center; height: 100%; } \n\
				tr, td { margin: 0; padding: 0; } \n\
				button { font-size: 45px; border: 0; height: 250px; width: 250px; background: #FFF; border-radius: 50%; padding: 0; color: #000; margin: 25px; text-transform: uppercase; outline: 0 none; } \n\
				a:hover, a:active, a:focus { border: 0; outline: 0; } \n\
				button.touch { background: #999; color: #FFF; } \n\
				button.fail { opacity: 0.6; } \n\
				button.lock { background: red; color: #FFF; } \n\
				button.unlock { background: green; color: #FFF; } \n\
			</style> \n\
		</body> \n\
	</html>";

	writeHeaders();
	server.send(200, "text/html", html);
}

void handleDebug() {
	String html = "<html lang=\"en\"><head><script>var connection = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']); connection.onopen = function () { connection.send('Connect ' + new Date()); }; connection.onerror = function (error) { console.log('WebSocket Error ', error); }; connection.onmessage = function (e) { console.log('Server: ', e.data); connection.send('Request ' + new Date()); }; </script> </head> <body> </body> </html>";

	html = ESP.getResetReason();

	writeHeaders();
	server.send(200, "text/html", html);
}

void stopWebServer() {
	server.stop();

	taskManager.remove(HTTP_HANDLE);
}

void webServerInit() {
	server.on("/", HTTP_GET, handleRoot);
	server.on("/status", []() {
		writeHeaders();
		server.send(200, "text/html", "<p>v" + String(baseVersion) + "." + String(version) + "</p>\n<p>" + WiFi.localIP().toString() + "</p>\n<p>" + uptime + "</p>");
	});
	server.on("/", HTTP_POST, []() {

		String state = "";
		int args = server.args();
		for (uint8_t i = 0; i < server.args(); i++) {
			String argName = server.argName(i);
			argName.toLowerCase();

			if (argName.equals("s")) {
				state = server.arg(i);
			}
		}

		if (state != "") {
			int result;
			if (state == "1") {
				lock(false);
			}
			else if (state == "0") {
				unlock(false);
			}

			writeHeaders();
			server.send(200, "text/html", "OK");
		}
	});
	server.on("/debug", HTTP_GET, handleDebug);
	server.onNotFound(handleRoot);
	server.begin();

	Serial.println(String("http://") + host + String(".local"));

	taskManager.add(HTTP_HANDLE, 25, handleClient, true);
}

void handleWebSocket() {
	webSocket.loop();
}

void webSocketInit() {
	webSocket.begin();
	webSocket.onEvent(webSocketEvent);

	Serial.println(String("ws://") + host + String(":") + String(webSocketPort));

	taskManager.add(WEBSOCKET_HANDLE, 10, handleWebSocket, true);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
	IPAddress ip = webSocket.remoteIP(num);

	if (type == WStype_DISCONNECTED)
	{
		Serial.printf("%d.%d.%d.%d: Disconnected\n", ip[0], ip[1], ip[2], ip[3]);
	}
	else if (type == WStype_CONNECTED)
	{
		Serial.printf("%d.%d.%d.%d: Connected\n", ip[0], ip[1], ip[2], ip[3]);
	}
	else if (type == WStype_TEXT) {
		String	str = (char*)payload;
		StaticJsonBuffer<200> jsonBuffer;
		JsonObject& root = jsonBuffer.parseObject(str);

		bool Auth = false;
		if (String(root["password"].asString()) == "2586") {
			Auth = true;
		}

		Serial.printf("%d.%d.%d.%d: %s\n", ip[0], ip[1], ip[2], ip[3], payload);

		String operation = String(root["operation"].asString());
		if (operation == "auth") {
			if (Auth) {
				webSocket.sendTXT(num, "{ \"success\": true, \"auth\": true, \"state\": " + String(currentState) + ", \"ip\": \"" + (String(ip.toString())) + "\" }");
			}
			else {
				webSocket.sendTXT(num, "{ \"success\": true, \"auth\": false, \"ip\": \"" + (String(ip.toString())) + "\" }");
			}
		}
		else if (Auth) {
			if (operation == "getState") {
				webSocket.sendTXT(num, "{  \"success\": true, \"auth\": true, \"state\": " + String(currentState) + ", \"ip\": \"" + (String(ip.toString())) + "\" }");
			}
			else if (operation == "locktwice") {
				lock(true);

				//webSocket.sendTXT(num, "{  \"success\": true, \"auth\": true, \"state\": " + String(currentState) + ", \"ip\": \"" + (String(ip.toString())) + "\" }");
			}
			else if (operation == "lock") {
				lock(false);

				//webSocket.sendTXT(num, "{  \"success\": true, \"auth\": true, \"state\": " + String(currentState) + ", \"ip\": \"" + (String(ip.toString())) + "\" }");
			}
			else if (operation == "unlock") {
				unlock(false);

				//webSocket.sendTXT(num, "{  \"success\": true, \"auth\": true, \"state\": " + String(currentState) + ", \"ip\": \"" + (String(ip.toString())) + "\" }");
			}
			else if (operation == "open") {
				unlock(true);
				//webSocket.sendTXT(num, "{  \"success\": true, \"auth\": true, \"state\": " + String(currentState) + ", \"ip\": \"" + (String(ip.toString())) + "\" }");
			}
		}
		else {
			//webSocket.sendTXT(num, "{ \"success\": false, \"auth\": false }");
			webSocket.disconnect(num);
		}
	}
	//else if (type == WStype_BIN) {
	//	Serial.printf("[%u] get binary lenght: %u\n", num, lenght);
	//	hexdump(payload, lenght);

	//	// send message to client
	//	// webSocket.sendBIN(num, payload, lenght);
	//}
}

void broadcastWebSocketMessage(String message) {
	webSocket.sendTXT(0, message);
}

void stopWebSocket() {
	webSocket.disconnect();

	taskManager.remove(HTTP_HANDLE);
}
#pragma endregion


//#############################################################################################################################################
//###																																		###
//###																OTA																		###
//###																																		###
//#############################################################################################################################################
#pragma region OTA
volatile bool ledState = LOW;

void onOtaStart() {
	resetServo();
	stopSensor();
	stopWebServer();
}

void onOtaProgress(int progress) {
	int led = ceil((24 * progress) / 100);
	FastLED.setBrightness(maxBrightness);
	for (int i = 0; i < numberOfLeds; i++) {
		if (i <= led) {
			leds[i] = CRGB::SpringGreen;
		}
		else {
			leds[i] = CRGB::White;
			leds[i].fadeToBlackBy(200);
		}
	}

	FastLED.show();
}

void onOtaEnd() {
	turnLedOff();
}

void onOtaError() {
	fill_solid(leds, numberOfLeds, CRGB::Red);
	FastLED.show();
}

void otaHandle() {
	ArduinoOTA.handle();
}

void OtaInit() {
	const char *otaHost = (host + String("-ota")).c_str();

	ArduinoOTA.setPort(8266);
	ArduinoOTA.setHostname(otaHost);
	//ArduinoOTA.setPassword((const char *)"6666");

	ArduinoOTA.onStart([]() {
		onOtaStart();
		Serial.println("Software Update Started. v" + String(baseVersion) + "." + String(version));
	});

	ArduinoOTA.onEnd([]() {
		EEPROM.write(0, version + 1);
		EEPROM.write(5, true);
		EEPROM.commit();
		digitalWrite(BoardLed, HIGH);

		Serial.println("Software Update Complete. v" + String(baseVersion) + "." + String(version));
		onOtaEnd();
	});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		int p = (progress / (total / 100));
		onOtaProgress(p);
		Serial.println("Progress: " + String(p));

		digitalWrite(BoardLed, digitalRead(BoardLed) == HIGH ? LOW : HIGH);
	});

	ArduinoOTA.onError([](ota_error_t error) {
		onOtaError();

		if (error == OTA_AUTH_ERROR) {
			Serial.println("OTA_AUTH_ERROR");
		}
		else if (error == OTA_BEGIN_ERROR) {
			Serial.println("OTA_BEGIN_ERROR");
		}
		else if (error == OTA_CONNECT_ERROR) {
			Serial.println("OTA_CONNECT_ERROR");
		}
		else if (error == OTA_RECEIVE_ERROR) {
			Serial.println("OTA_RECEIVE_ERROR");
		}
		else if (error == OTA_END_ERROR) {
			Serial.println("OTA_END_ERROR");
		}

		digitalWrite(BoardLed, LOW);
	});

	ArduinoOTA.begin();

	taskManager.add(OTA_HANDLE, 500, otaHandle);
}
#pragma endregion



//#############################################################################################################################################
//###																																		###
//###																WIFI																	###
//###																																		###
//#############################################################################################################################################
#pragma region WifiConnection
ESP8266WiFiMulti WiFiMulti;
volatile int retryCount = 0;
int smartConfigStart = 0;
String ssid = "YiToLiK";
IPAddress ip = IPAddress(192, 168, 1, 102);
IPAddress gateway = IPAddress(192, 168, 1, 1);

void onWifiCheck() {
	ledState = ledState == HIGH ? LOW : HIGH;
	digitalWrite(BoardLed, ledState);

	if (WiFi.status() == WL_CONNECTED) {
		taskManager.remove(WIFI_CHECK);
		onConnect();
	}
	else if (retryCount > 150) {
		taskManager.remove(WIFI_CHECK);
		onFail();
	}
	retryCount++;
}

void Connect() {
	taskManager.remove(WIFI_CONNECT);
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	WiFi.setPhyMode(WIFI_PHY_MODE_11N);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	/*WiFi.setOutputPower(10.0);*/
	delay(10);
	WiFi.hostname(host);
	WiFi.mode(WIFI_STA);
	WiFi.begin();
	//WiFiMulti.run();

	taskManager.add(WIFI_CHECK, 200, onWifiCheck, true);
}

void initWifi() {
	taskManager.add(WIFI_CONNECT, 10, Connect);
}

void onConnect() {
	if (WiFi.SSID() == ssid) {
		WiFi.config(ip, gateway, IPAddress(255, 255, 255, 0));
	}

	digitalWrite(BoardLed, HIGH);
	MDNS.begin("smartlock");
	MDNS.addService("http", "tcp", webServerPort);
	MDNS.addService("ws", "tcp", webSocketPort);
	Serial.println(WiFi.localIP());

	OtaInit();

	webServerInit();
	webSocketInit();
}

void onFail() {
	WiFi.beginSmartConfig();
	digitalWrite(BoardLed, LOW);
	smartConfigStart = millis();
	while (1) {
		delay(1000);
		if (WiFi.smartConfigDone()) {
			onConnect();
			break;
		}
		else if (millis() - smartConfigStart >= 60000) {
			ESP.restart();
			delay(1000);
		}
	}
}
#pragma endregion


//#####################################################################b#######################################################################
//#############################################################################################################################################
//#############################################################################################################################################
#pragma region Setup
bool updated = false;
void setup() {
	initServo();

#pragma region version
	Serial.begin(74880);
	//Serial.begin(115200);
	Serial.println("BOOT...");
	EEPROM.begin(512);
	version = EEPROM.read(0);
	bool Updated = false;
	if (EEPROM.read(5)) {
		EEPROM.write(5, false);
		EEPROM.commit();

		Updated = true;

		Serial.print("Updated ");
	}
	Serial.println("v" + String(baseVersion) + "." + String(version));
#pragma endregion

	initLock();

	initLed();

	initWifi();
}
#pragma endregion

void loop() {
	readSensor();

	taskManager.update();
}