/*

GitHub OTA Example

This example shows how to use the ESP-OTA-Pull library to retrieve code from GitHub. 

OVERVIEW:
This program will reach out to GitHub and grab new code that will send a serial output.

BOARD SETUP:
Compatible with any >4MB ESP32 Dev Module, ESP32S2 Dev Module, or ESP32S3 Dev Module.
  Program will compile for any ESP32 board, but will not install an OTA if not set as one of the above board types.
To work, use the Default 4MB partition formatting (1.2MB app)
Connect the board to your computer and open a serial terminal at 115200 baud. This is used both for the OTA status, and to show it worked afterwards.
The code that is installed after the OTA can be found in the examples folder of this library, called "Serial OTA Verification".

HOW IT WORKS:
Hosting binaries on GitHub is a great way to ensure your projects are up-to-date. There are a few notes to keep in mind;
* GitHub supports linking to both a static release file, or alternatively just the latest version of that file.
  * Example: To link to the OTA Directory JSON, you can use;
  * https://github.com/JimSHED/ESP32-OTA-Pull-GitHub/releases/latest/download/otadirectory.json to get the file "otadirectory.json" in the release marked latest
    * WARNING: If there is no file of that name in the latest release, it will not look back in previous releases or anything, it will just fail out. 
  * https://github.com/JimSHED/ESP32-OTA-Pull-GitHub/releases/download/V1.2.0/otadirectory.json to get the file "otadirectort.json" in the release tagged "V1.2.0"
* We can use the same option to either link to the latest .bin or a set .bin in the JSON directory.
* While it is possible to link to files in the GitHub blob that aren't released yet, it is not reccomended, and is untested.

GITHUB SETUP:
* In our situation, we are going to pull the right update from the release tagged "example".
* The JSON file is in the release, called "otadirectory.json"
* The JSON will direct the board to the right image file based on the board type;
  * ESP32 gets "esp32.bin" https://github.com/JimSHED/ESP32-OTA-Pull-GitHub/releases/download/example/esp32.bin
  * ESP32S2 gets "esp32s2.bin" https://github.com/JimSHED/ESP32-OTA-Pull-GitHub/releases/download/example/esp32s2.bin
  * ESP32S3 gets "esp32s3.bin" https://github.com/JimSHED/ESP32-OTA-Pull-GitHub/releases/download/example/esp32s3.bin
* Since we don't know if blink is the latest release of the library (probably isn't), we are going to statically link to the JSON and the binaries.
* You can see all this by going to https://github.com/JimSHED/ESP32-OTA-Pull-GitHub/releases/tag/example

*/

#include "ESP32OTAPull.h"

//Enter your WiFi SSID and password here. 
#define SSID "WIFI_SSID"
#define PASSWORD "WIFI_PASSWORD"

//The example binaries on GitHub are listed are V1.0.1, so as long as the below number is less than that, we will have an update.
#define VERSION "1.0.0"

void callback_percent(int offset, int totallength);

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  delay(3000);

  Serial.println("ESP32 OTA Pull GitHub Example");
  Serial.println("Connecting to WiFi...");

  WiFi.begin(SSID, PASSWORD);
    while (!WiFi.isConnected())
    {
      Serial.print(".");
      delay(250);
    }

  Serial.println();

  delay(2000);

  Serial.println("Checking for OTA Update.");
  Serial.println("Since 'UPDATE_AND_BOOT' is set, the system will immediately install the update (if it finds one) and restart.");
  Serial.println("If it doesn't find an update for whatever reason, an error will print telling you why.");

  delay(2000);

  ESP32OTAPull ota; //Create ota object

  ota.EnableSerialDebug(); //Enables verbose output, telling you what the OTA is doing under the hood. Handy for finding issues.

  ota.SetCallback(callback_percent); //Setting a callback
  //The callback can be used to make things happen as the update installs.
  //You pass it a function, that expects the parameters int offset and int totallength
  //You can then use this information for progress bars and similar.
  //In our situation, we print the progress in bytes and as a percent.

  int ret = ota.CheckForOTAUpdate("https://github.com/JimSHED/ESP32-OTA-Pull-GitHub/releases/download/example/otadirectory.json", VERSION, ESP32OTAPull::UPDATE_AND_BOOT);

  Serial.print("OTA Update Check Returned the Status Code: "); Serial.println(ret);
  Serial.print("This means: "); Serial.println(errtext(ret));

  Serial.println("If you made it this far, something went wrong. Check the above message.");

}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
  Serial.println("If you are seeing this, the OTA failed. Please restart or try again.");
}

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

void callback_percent(int offset, int totallength)
{
	static int prev_percent = -1;
	int percent = 100 * offset / totallength;
	if (percent != prev_percent)
	{
		Serial.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
		prev_percent = percent;
	}
}
