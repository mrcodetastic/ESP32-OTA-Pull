/*
HTTPS-OTA-Example - demonstrates HTTPS support in ESP32-OTA-Pull library
Copyright (C) 2022-3 Mikal Hart
All rights reserved.

https://github.com/mikalhart/ESP32-OTA-Pull

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <Arduino.h>
#include "ESP32OTAPull.h"

// First, edit these values appropriately
#if __has_include("settings.h") // optionally override with values in settings.h
#include "settings.h"
#else
static const char *JSON_URL = "https://example.com/myimages/HTTPS-OTA-Example.json"; // this is where you'll post your JSON filter file
static const char *SSID 	= "<WiFi SSID>";
static const char *PASS     = "<WiFi Password>";
static const char *VERSION  = "1.0.0"; // The current version of this program
#endif

// Root CA certificate for your HTTPS server (example for Let's Encrypt)
// You can get this from your browser or use openssl to extract it
// This is just an example - replace with your actual root CA
static const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

// ----------------------------------------------------------------------------------------------------- //

const char *errtext(int code)
{
	switch(code)
	{
		case ESP32OTAPull::UPDATE_AVAILABLE:
			return "An update is available but wasn't installed";
		case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
			return "No profile matches";
		case ESP32OTAPull::NO_UPDATE_AVAILABLE:
			return "Profile matched, but update not applicable";
		case ESP32OTAPull::UPDATE_OK:
			return "An update was done, but no reboot";
		case ESP32OTAPull::HTTP_FAILED:
			return "HTTP GET failure";
		case ESP32OTAPull::WRITE_ERROR:
			return "Write error";
		case ESP32OTAPull::JSON_PROBLEM:
			return "Invalid JSON";
		case ESP32OTAPull::OTA_UPDATE_FAIL:
			return "Update fail (no OTA partition?)";
		default:
			if (code > 0)
				return "Unexpected HTTP response code";
			break;
	}
	return "Unknown error";
}

void DisplayInfo()
{
	char exampleImageURL[256];
	snprintf(exampleImageURL, sizeof(exampleImageURL), "https://example.com/HTTPS-OTA-Example-%s-%s.bin", ARDUINO_BOARD, VERSION);

	Serial.printf("HTTPS-OTA-Example v%s\n", VERSION);
	Serial.printf("You need to post a JSON (text) file similar to this:\n");
	Serial.printf("{\n");
	Serial.printf("  \"Configurations\": [\n");
	Serial.printf("    {\n");
	Serial.printf("      \"Board\": \"%s\",\n", ARDUINO_BOARD);
	Serial.printf("      \"Device\": \"%s\",\n", WiFi.macAddress().c_str());
	Serial.printf("      \"Version\": \"%s\",\n", VERSION);
	Serial.printf("      \"URL\": \"%s\"\n", exampleImageURL);
	Serial.printf("    }\n");
	Serial.printf("  ]\n");
	Serial.printf("}\n");
	Serial.printf("\n");
	Serial.printf("(Board, Device, Config, and Version are all *optional*.)\n");
	Serial.printf("\n");
	Serial.printf("Post the JSON at, e.g., %s\n", JSON_URL);
	Serial.printf("Post the compiled bin at, e.g., %s\n\n", exampleImageURL);
}

void pull_ota_callback(int offset, int totallength)
{
	Serial.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
#if defined(LED_BUILTIN) // flicker LED on update
	static int status = LOW;
	status = status == LOW && offset < totallength ? HIGH : LOW;
	digitalWrite(LED_BUILTIN, status);
#endif
}

// ----------------------------------------------------------------------------------------------------- //

void setup()
{
	Serial.begin(115200);
	delay(2000); // wait for ESP32 Serial to stabilize
#if defined(LED_BUILTIN)
	pinMode(LED_BUILTIN, OUTPUT);
#endif

	DisplayInfo();

  Serial.printf("Connecting to WiFi '%s'...", SSID);
	WiFi.begin(SSID, PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

	delay(2000); // Give some time to obtain IP_ADDR 'STA_GOT_IP'. Or risk HTTP_ERROR prematurely.

	// HTTPS Example 1: Using a root CA certificate for secure connection
	ESP32OTAPull ota_secure;
	ota_secure.SetRootCA(root_ca)
			  .EnableSerialDebug()
			  .SetCallback(pull_ota_callback);

	Serial.printf("Example 1: Secure HTTPS connection with root CA certificate\n");
	Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s'.\n", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str());
	Serial.printf("Checking %s to see if an update is available...\n", JSON_URL);
	int ret = ota_secure.CheckForOTAUpdate(JSON_URL, VERSION);
	Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));

	delay(2000);

	// HTTPS Example 2: Using insecure connection (NOT RECOMMENDED for production)
	ESP32OTAPull ota_insecure;
	ota_insecure.SetInsecure(true)
				.EnableSerialDebug()
				.SetCallback(pull_ota_callback);

	Serial.printf("Example 2: Insecure HTTPS connection (certificate verification disabled)\n");
	Serial.printf("WARNING: This method is NOT RECOMMENDED for production use!\n");
	ret = ota_insecure.CheckForOTAUpdate(JSON_URL, "0.0.0"); // Force update check
	Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));

	delay(2000);

	// HTTPS Example 3: Method chaining with multiple configurations
	ESP32OTAPull ota_chained;
	ret = ota_chained.SetRootCA(root_ca)
					 .EnableSerialDebug()
					 .SetCallback(pull_ota_callback)
					 .AllowDowngrades(false)
					 .CheckForOTAUpdate(JSON_URL, VERSION);
	
	Serial.printf("Example 3: Method chaining example\n");
	Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));
}

void loop()
{
}
