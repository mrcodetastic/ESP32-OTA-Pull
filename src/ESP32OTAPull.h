/*
ESP32-OTA-Pull - a library for doing "pull" based OTA ("Over The Air") firmware
updates, where the image updates are posted on the web.

MIT License

Copyright (c) 2022-3 Mikal Hart

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <string.h>

class ESP32OTAPull
{
public:
    enum ActionType { DONT_DO_UPDATE, UPDATE_BUT_NO_BOOT, UPDATE_AND_BOOT };

    // Return codes from CheckForOTAUpdate
    enum ErrorCode { UPDATE_AVAILABLE = -3, NO_UPDATE_PROFILE_FOUND = -2, NO_UPDATE_AVAILABLE = -1, UPDATE_OK = 0, HTTP_FAILED = 1, WRITE_ERROR = 2, JSON_PROBLEM = 3, OTA_UPDATE_FAIL = 4 };

private:
    void (*Callback)(int offset, int totallength) = NULL;
    String Board      = ARDUINO_BOARD;
    String Device     = "";
    String Config     = "";
    String CVersion   = "";
    bool DowngradesAllowed = false;
    bool SerialDebug = false;
    
    // HTTPS/SSL configuration
    const char* RootCA = NULL;
    const char* ClientCert = NULL;
    const char* ClientKey = NULL;
    bool InsecureConnection = false;

    static bool IsNumericVersion(const char* version)
    {
        if (version == NULL || *version == '\0')
            return false;
        bool expectingDigit = true;
        for (const char* p = version; *p != '\0'; ++p)
        {
            if (*p == '.')
            {
                if (expectingDigit)
                    return false;
                expectingDigit = true;
            }
            else if (isdigit(static_cast<unsigned char>(*p)))
                expectingDigit = false;
            else
                return false;
        }
        return !expectingDigit;
    }

    // Compare dot-separated numeric versions (for example, 1.10.0 > 1.9.0).
    static bool CompareVersions(const char* left, const char* right, int& result)
    {
        if (!IsNumericVersion(left) || !IsNumericVersion(right))
            return false;

        const char* a = left;
        const char* b = right;
        while (*a != '\0' || *b != '\0')
        {
            const char* aEnd = a;
            const char* bEnd = b;
            while (*aEnd != '\0' && *aEnd != '.') ++aEnd;
            while (*bEnd != '\0' && *bEnd != '.') ++bEnd;

            const char* aSignificant = a;
            const char* bSignificant = b;
            while (aSignificant < aEnd && *aSignificant == '0') ++aSignificant;
            while (bSignificant < bEnd && *bSignificant == '0') ++bSignificant;
            const size_t aLength = static_cast<size_t>(aEnd - aSignificant);
            const size_t bLength = static_cast<size_t>(bEnd - bSignificant);
            if (aLength != bLength)
            {
                result = aLength > bLength ? 1 : -1;
                return true;
            }

            const int segmentComparison = strncmp(aSignificant, bSignificant, aLength);
            if (segmentComparison != 0)
            {
                result = segmentComparison > 0 ? 1 : -1;
                return true;
            }
            a = *aEnd == '.' ? aEnd + 1 : aEnd;
            b = *bEnd == '.' ? bEnd + 1 : bEnd;
        }
        result = 0;
        return true;
    }

    bool ConfigureHTTPClient(HTTPClient& http, WiFiClientSecure& secureClient, const char* url)
    {
        if (url == NULL)
            return false;
        const bool useHTTPS = strncmp(url, "https://", 8) == 0;
        const bool useHTTP = strncmp(url, "http://", 7) == 0;
        if (!useHTTPS && !useHTTP)
            return false;

        http.setConnectTimeout(15000);
        http.setTimeout(15000);
        if (useHTTPS)
        {
            if ((ClientCert == NULL) != (ClientKey == NULL))
                return false;
            if (InsecureConnection)
            {
                secureClient.setInsecure();
                if (SerialDebug)
                    Serial.println("HTTPS: Insecure mode explicitly enabled; certificate verification is disabled");
            }
            else if (RootCA != NULL)
            {
                secureClient.setCACert(RootCA);
                if (SerialDebug)
                    Serial.println("HTTPS: Using provided root CA certificate");
            }
            else
            {
                if (SerialDebug)
                    Serial.println("HTTPS: Refusing connection because no root CA is configured");
                return false;
            }

            if (ClientCert != NULL)
            {
                secureClient.setCertificate(ClientCert);
                secureClient.setPrivateKey(ClientKey);
            }
            return http.begin(secureClient, url);
        }
        return http.begin(url);
    }

    int DoOTAUpdate(const char* URL, ActionType action)
    {
        HTTPClient http;
        WiFiClientSecure secureClient;
        if (!ConfigureHTTPClient(http, secureClient, URL))
            return HTTP_FAILED;

        const int httpResponseCode = http.GET();
        if (httpResponseCode != HTTP_CODE_OK)
        {
            http.end();
            return httpResponseCode > 0 ? httpResponseCode : HTTP_FAILED;
        }

        const int totalLength = http.getSize();
        if (totalLength <= 0)
        {
            http.end();
            return HTTP_FAILED; // A valid Content-Length is required.
        }
        if (!Update.begin(static_cast<size_t>(totalLength)))
        {
            http.end();
            return OTA_UPDATE_FAIL;
        }

        WiFiClient* stream = http.getStreamPtr();
        if (stream == NULL)
        {
            Update.abort();
            http.end();
            return WRITE_ERROR;
        }

        uint8_t buffer[1024];
        int offset = 0;
        unsigned long lastProgress = millis();
        const unsigned long inactivityTimeoutMs = 30000;
        while (offset < totalLength)
        {
            if (!http.connected() && stream->available() == 0)
                break;
            const size_t available = stream->available();
            if (available == 0)
            {
                if (millis() - lastProgress >= inactivityTimeoutMs)
                    break;
                delay(1);
                continue;
            }

            const size_t remaining = static_cast<size_t>(totalLength - offset);
            size_t bytesToRead = available < sizeof(buffer) ? available : sizeof(buffer);
            if (bytesToRead > remaining)
                bytesToRead = remaining;
            const size_t bytesRead = stream->readBytes(buffer, bytesToRead);
            if (bytesRead == 0 || Update.write(buffer, bytesRead) != bytesRead)
                break;

            offset += static_cast<int>(bytesRead);
            lastProgress = millis();
            if (Callback != NULL)
                Callback(offset, totalLength);
        }

        if (offset != totalLength)
        {
            Update.abort();
            http.end();
            return WRITE_ERROR;
        }

        http.end();
        if (!Update.end())
        {
            Update.abort();
            return OTA_UPDATE_FAIL;
        }
        if (action == UPDATE_BUT_NO_BOOT)
            return UPDATE_OK;
        delay(1000);
        ESP.restart();
        return UPDATE_OK;
    }

public:
    /// @brief Set the root CA certificate for HTTPS connections
    /// @param rootCA PEM-formatted root CA certificate string
    /// @return The current ESP32OTAPull object for chaining
    ESP32OTAPull &SetRootCA(const char *rootCA)
    {
        RootCA = rootCA;
        InsecureConnection = false;
        return *this;
    }

    /// @brief Set client certificate and private key for mutual TLS authentication
    /// @param clientCert PEM-formatted client certificate string
    /// @param clientKey PEM-formatted private key string
    /// @return The current ESP32OTAPull object for chaining
    ESP32OTAPull &SetClientCertificate(const char *clientCert, const char *clientKey)
    {
        ClientCert = clientCert;
        ClientKey = clientKey;
        return *this;
    }

    /// @brief Enable insecure HTTPS connections (skip certificate verification)
    /// @param insecure true to skip certificate verification (NOT RECOMMENDED for production)
    /// @return The current ESP32OTAPull object for chaining
    ESP32OTAPull &SetInsecure(bool insecure = true)
    {
        InsecureConnection = insecure;
        if (insecure)
            RootCA = NULL;
        return *this;
    }

    /// @brief Return the version string of the binary, as reported by the JSON
    /// @return The firmware version
    String GetVersion()
    {
        return CVersion;
    }

    /// @brief Override the default "Device" id (MAC Address)
    /// @param device A string identifying the particular device (instance) (typically e.g., a MAC address)
    /// @return The current ESP32OTAPull object for chaining
    ESP32OTAPull &OverrideDevice(const char *device)
    {
        Device = device;
        return *this;
    }

    /// @brief Override the default "Board" value of ARDUINO_BOARD
    /// @param board A string identifying the board (class) being targeted
    /// @return The current ESP32OTAPull object for chaining
    ESP32OTAPull &OverrideBoard(const char *board)
    {
        Board = board;
        return *this;
    }

    /// @brief Specify a configuration string that must match any "Config" in JSON
    /// @param config An arbitrary string showing the current configuration
    /// @return The current ESP32OTAPull object for chaining
    ESP32OTAPull &SetConfig(const char *config)
    {
        Config = config;
        return *this;
    }

    /// @brief Specify whether downgrades (posted version is lower) are allowed
    /// @param allow_downgrades true if downgrades are allowed
    /// @return The current ESP32OTAPull object for chaining
    ESP32OTAPull &AllowDowngrades(bool allow_downgrades)
    {
        DowngradesAllowed = allow_downgrades;
        return *this;
    }

    /// @brief Specify a callback function to monitor update progress
    /// @param callback Pointer to a function that is called repeatedly during update
    /// @return The current ESP32OTAPull object for chaining
    ESP32OTAPull &SetCallback(void (*callback)(int offset, int totallength))
    {
        Callback = callback;
        return *this;
    }

    /// @brief Enable extra debugging output on Serial if required.
    ESP32OTAPull &EnableSerialDebug()
    {
        SerialDebug = true;
        return *this;
    }

    /// @brief The main entry point for OTA Update
    /// @param JSON_URL The URL for the JSON filter file
    /// @param CurrentVersion The version # of the current (i.e. to be replaced) sketch
    /// @param ActionType The action to be performed.  May be any of DONT_DO_UPDATE, UPDATE_BUT_NO_BOOT, UPDATE_AND_BOOT (default)
    /// @return ErrorCode or HTTP failure code (see enum above)
    int CheckForOTAUpdate(const char* JSON_URL, const char *CurrentVersion, ActionType Action = UPDATE_AND_BOOT)
    {
        CurrentVersion = CurrentVersion == NULL ? "" : CurrentVersion;
        if (!IsNumericVersion(CurrentVersion))
            return JSON_PROBLEM;

        HTTPClient http;
        WiFiClientSecure secureClient;
        if (!ConfigureHTTPClient(http, secureClient, JSON_URL))
            return HTTP_FAILED;

        // Send HTTP GET request
        int httpResponseCode = http.GET();
		
        if (SerialDebug) {
            Serial.print("Got HTTP Response: ");
            Serial.println(httpResponseCode, DEC);
        }

        if (httpResponseCode != HTTP_CODE_OK) {
            http.end();
            return httpResponseCode > 0 ? httpResponseCode : HTTP_FAILED;
        }

        // Bound manifest parsing and reject unknown-length bodies (including chunked bodies).
        const int jsonLength = http.getSize();
        const int maxJsonLength = 16 * 1024;
        if (jsonLength <= 0 || jsonLength > maxJsonLength) {
            http.end();
            return JSON_PROBLEM;
        }

        Stream& rawStream = http.getStream();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, rawStream);
        http.end();

        if (error) {
            if (SerialDebug)  {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
            }
            return JSON_PROBLEM;
        }

        if (!doc.is<JsonObject>() || !doc["Configurations"].is<JsonArray>())
            return JSON_PROBLEM;

        JsonArray configurations = doc["Configurations"].as<JsonArray>();
        if (configurations.size() == 0)
            return NO_UPDATE_PROFILE_FOUND;

        // Validate every profile before matching or using any URL.
        for (JsonObject config : configurations)
        {
            if (config.isNull() || !config["URL"].is<const char*>() ||
                !config["Version"].is<const char*>() ||
                (!config["Board"].isNull() && !config["Board"].is<const char*>()) ||
                (!config["Device"].isNull() && !config["Device"].is<const char*>()) ||
                (!config["Config"].isNull() && !config["Config"].is<const char*>()))
                return JSON_PROBLEM;

            const char* url = config["URL"].as<const char*>();
            if (url[0] == '\0' ||
                !IsNumericVersion(config["Version"].as<const char*>()) ||
                (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0))
                return JSON_PROBLEM;
        }

        String _Board    = Board.isEmpty() ? ARDUINO_BOARD : Board;
        String _Device   = Device.isEmpty() ? WiFi.macAddress() : Device;
        String _Config   = Config.isEmpty() ? "" : Config;
        bool foundProfile = false;

        if (SerialDebug) {

            Serial.println("Looking for a configuration that matches:");
            Serial.print("Board: ");    Serial.println(_Board);
            Serial.print("Version: ");  Serial.println(CurrentVersion);
            Serial.print("Device: ");   Serial.println(_Device);
        }

        // Step through the configurations looking for a match
        for (JsonObject config : configurations)
        {
            String CBoard   = config["Board"].isNull() ? "" : config["Board"].as<const char*>();
            String CDevice  = config["Device"].isNull() ? "" : config["Device"].as<const char*>();
            CVersion        = config["Version"].as<const char*>();
            String CConfig  = config["Config"].isNull() ? "" : config["Config"].as<const char*>();

            if ((CBoard.isEmpty() || CBoard == _Board) &&
                (CDevice.isEmpty() || CDevice == _Device) &&
                (CConfig.isEmpty() || CConfig == _Config))
            {
                int versionComparison = 0;
                if (!CompareVersions(CVersion.c_str(), CurrentVersion, versionComparison))
                    return JSON_PROBLEM;

                if (versionComparison > 0 || (DowngradesAllowed && versionComparison != 0)) {
                    CVersion = config["Version"].as<const char*>();
                    return Action == DONT_DO_UPDATE ? UPDATE_AVAILABLE : DoOTAUpdate(config["URL"].as<const char*>(), Action);
                }
                foundProfile = true;
            }
        }
        return foundProfile ? NO_UPDATE_AVAILABLE : NO_UPDATE_PROFILE_FOUND;
    }
};
