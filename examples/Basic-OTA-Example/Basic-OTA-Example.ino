/*
Basic-OTA-Example - simple demonstration of ESP32-OTA-Pull library for doing "pull" based OTA updates
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

// Edit these defaults, or opt in to a local settings.h by defining
// BASIC_OTA_EXAMPLE_USE_SETTINGS below. The header must define all four names.
// #define BASIC_OTA_EXAMPLE_USE_SETTINGS
#if defined(BASIC_OTA_EXAMPLE_USE_SETTINGS)
#include "settings.h"
#else
static const char JSON_URL[] = "https://example.com/myimages/Basic-OTA-Example.json"; // URL of the JSON filter file
static const char SSID[] = "<WiFi SSID>";
static const char PASS[] = "<WiFi Password>";
static const char VERSION[] = "1.0.0"; // Current firmware version
#endif

// Now create a text file called "Basic-OTA-Example.json" that describes the image you are going to post.  Example below.
// * The "Board" line, if provided, should match the ARDUINO_BOARD string that is predefined for the board you selected
// * If you include a "Device" line, the update will only match the single device that matches the provided MAC address.
//   Omit it if you want to update ALL devices.
// * The "Version" line should describe the posted image's version.  Update will only occur if it is different than VERSION.
// * Add a "Config" line to further filter if you like.  E.g. "Config": "32MB",
// * "URL" should point to the .bin file to be downloaded/installed

/*
	{
		"Configurations": [
			{
			"Board": "ESP32_DEV",
			"Device": "24:63:28:AD:FF:04",
			"Version": "1.0.0",
			"URL": "https://example.com/myimages/Basic-OTA-Example.bin"
			}
		]
	}
*/

/*
  Post the above file at <URL_JSON>.

  Compile your sketch with "Sketch/Export Compiled Binary", and post the resulting .bin file at the <URL>
  you specified in the JSON file.
*/

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
	snprintf(exampleImageURL, sizeof(exampleImageURL), "https://example.com/Basic-OTA-Example-%s-%s.bin", ARDUINO_BOARD, VERSION);

	Serial.printf("Basic-OTA-Example v%s\n", VERSION);
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
	Serial.printf("(Board, Device, and Config are optional; Version and URL are required.)\n");
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

	// First example: update should NOT occur, because Version string in JSON matches local VERSION value.
	ESP32OTAPull ota;

	//ota.EnableSerialDebug();
	ota.SetCallback(pull_ota_callback);
	Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s'.\n", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str());
	Serial.printf("Checking %s to see if an update is available...\n", JSON_URL);
	int ret = ota.CheckForOTAUpdate(JSON_URL, VERSION);
	Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));

	delay(2000);

	// Second example: update *will* happen because we are pretending we have an earlier version
	Serial.printf("But if we pretend like we're running version 0.0.0, we SHOULD see an update happen.\n");
	ret = ota.CheckForOTAUpdate(JSON_URL, "0.0.0");
	Serial.printf("(If the update succeeds, the reboot should prevent us ever getting here.)\n");
	Serial.printf("CheckOTAForUpdate returned %d (%s)\n\n", ret, errtext(ret));
}

void loop()
{
}

