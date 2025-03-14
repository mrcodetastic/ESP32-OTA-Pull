/*

Serial OTA Verification

This example doesn't actually demonstrate any functionality of the library.

It is the code that is OTA updated onto the board when you run the "GitHub-OTA-Example" Example. 

You can ignore this file, it is just included for reference and version control purposes. 

*/

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("If you can read this, the OTA update was a success!");
  delay(2000);
}
