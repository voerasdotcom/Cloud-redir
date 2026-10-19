/**
 * CloudRedir Frontend Application
 *
 * This application runs as a regular homebrew app and allows the user to
 * configure the CloudRedir sysmodule. It handles the OAuth2 device authorization
 * flow for Google Drive and writes the configuration to a file that the sysmodule reads.
 */

#include <switch.h>
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// Simple JSON helper for creating configuration (we'll write JSON manually)
// In a real project, we would use a JSON library like mjson or nlohmann/json.
// For this skeleton, we'll format the JSON as a string.

// Structure to hold configuration
struct Config {
    std::string cloud_provider;
    std::string access_token;
    std::string sync_folder;
};

// Global configuration (to be saved to file)
Config g_config;

// Callback for libcurl to write response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Function to perform HTTP POST request
std::string httpPost(const std::string& url, const std::string& post_data) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        // Follow redirects if needed
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

// Function to perform HTTP GET request
std::string httpGet(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

// Step 1: Get device and user codes from Google's OAuth device endpoint
bool requestDeviceCodes(std::string& device_code, std::string& user_code, int& expires_in, std::string& interval) {
    // Replace with your own client ID (you need to create a Google Cloud project and enable Drive API)
    const std::string client_id = "YOUR_CLIENT_ID.apps.googleusercontent.com";
    const std::string scope = "https://www.googleapis.com/auth/drive.file"; // Limited to files created by the app
    const std::string url = "https://oauth2.googleapis.com/device/code";
    std::string post_data = "client_id=" + client_id + "&scope=" + scope;

    std::string response = httpPost(url, post_data);
    // Parse JSON response (simplified)
    // In reality, use a JSON parser. We'll do a very basic string search for the skeleton.
    size_t pos;

    // device_code
    pos = response.find("\"device_code\":\"");
    if(pos == std::string::npos) return false;
    pos += 15;
    size_t end = response.find('\"', pos);
    device_code = response.substr(pos, end - pos);

    // user_code
    pos = response.find("\"user_code\":\"");
    if(pos == std::string::npos) return false;
    pos += 13;
    end = response.find('\"', pos);
    user_code = response.substr(pos, end - pos);

    // expires_in
    pos = response.find("\"expires_in\":");
    if(pos == std::string::npos) return false;
    pos += 13;
    end = response.find(',', pos);
    expires_in = std::stoi(response.substr(pos, end - pos));

    // interval
    pos = response.find("\"interval\":");
    if(pos == std::string::npos) return false;
    pos += 11;
    end = response.find(',', pos);
    interval = response.substr(pos, end - pos);

    return true;
}

// Step 2: Poll for token
bool pollForToken(const std::string& device_code, const std::string& client_id, std::string& access_token) {
    const std::string url = "https://oauth2.googleapis.com/token";
    std::string post_data = "client_id=" + client_id +
                            "&device_code=" + device_code +
                            "&grant_type=urn:ietf:params:oauth:grant-type:device_code";

    auto start = std::chrono::steady_clock::now();
    while(true) {
        std::string response = httpPost(url, post_data);
        // Check if we got an access token
        size_t pos = response.find("\"access_token\":\"");
        if(pos != std::string::npos) {
            pos += 16;
            size_t end = response.find('\"', pos);
            access_token = response.substr(pos, end - pos);
            return true;
        }

        // Check for error: authorization_pending means user hasn't authorized yet
        if(response.find("\"error\":\"authorization_pending\"") != std::string::npos) {
            // Wait for the interval (we'll use 5 seconds as default, but should use from device_code response)
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } else {
            // Other error (e.g., expired_token, access_denied)
            fprintf(stderr, "Token poll error: %s\n", response.c_str());
            return false;
        }

        // Check if we've exceeded the expires_in time (we don't have it here, but we can track time)
        auto now = std::chrono::steady_clock::now();
        if(std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 1800) { // 30 minutes max
            fprintf(stderr, "Token poll timed out.\n");
            return false;
        }
    }
    return false;
}

// Save configuration to a file in the sysmodule's save data location
bool saveConfigToFile(const Config& config) {
    // We'll save to: sd:/sysmodules/cloudredir/config.json
    FILE* file = fopen("sd:/sysmodules/cloudredir/config.json", "w");
    if(!file) {
        perror("Failed to open config file for writing");
        return false;
    }

    // Write JSON manually
    fprintf(file, "{\n");
    fprintf(file, "  \"cloud_provider\": \"%s\",\n", config.cloud_provider.c_str());
    fprintf(file, "  \"access_token\": \"%s\",\n", config.access_token.c_str());
    fprintf(file, "  \"sync_folder\": \"%s\"\n", config.sync_folder.c_str());
    fprintf(file, "}\n");

    fclose(file);
    return true;
}

int main(int argc, char* argv[]) {
    // Initialize services
    socketInitializeDefault();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Set default config
    g_config.cloud_provider = "googledrive";
    g_config.access_token = "";
    g_config.sync_folder = "/CloudRedir/saves";

    // Clear console
    consoleInit(NULL);
    printf("\x1b[2J\x1b[H"); // Clear screen and home cursor
    printf("CloudRedir Frontend Configuration\n");
    printf("=================================\n\n");

    // Step 1: Request device and user codes
    printf("Requesting device authorization from Google...\n");
    std::string device_code, user_code, interval;
    int expires_in = 0;
    if(!requestDeviceCodes(device_code, user_code, expires_in, interval)) {
        printf("Failed to request device codes. Check your internet connection and client ID.\n");
        printf("Press B to exit.\n");
        while(aptMainLoop()) {
            hidScanInput();
            if(hidKeysDown(HID_CONTROLLER_P1) & KEY_B) break;
            svcSleepThread(10000000LL);
        }
        goto cleanup;
    }

    printf("\nPlease go to the following URL on another device:\n");
    printf("  https://www.google.com/device\n\n");
    printf("And enter the code:\n");
    printf("  %s\n\n", user_code.c_str());
    printf("Waiting for authorization...\n");

    // Step 2: Poll for token (replace with your actual client ID)
    const std::string client_id = "YOUR_CLIENT_ID.apps.googleusercontent.com";
    if(!pollForToken(device_code, client_id, g_config.access_token)) {
        printf("Failed to obtain access token. Authorization may have been denied or timed out.\n");
        printf("Press B to exit.\n");
        while(aptMainLoop()) {
            hidScanInput();
            if(hidKeysDown(HID_CONTROLLER_P1) & KEY_B) break;
            svcSleepThread(10000000LL);
        }
        goto cleanup;
    }

    printf("\nAuthorization successful! Saving configuration...\n");

    // Save configuration to file
    if(saveConfigToFile(g_config)) {
        printf("Configuration saved successfully to sd:/sysmodules/cloudredir/config.json\n");
        printf("The sysmodule will now use this configuration.\n");
    } else {
        printf("Failed to save configuration.\n");
    }

    printf("\nPress B to exit.\n");

    // Wait for user to exit
    while(aptMainLoop()) {
        hidScanInput();
        if(hidKeysDown(HID_CONTROLLER_P1) & KEY_B) break;
        svcSleepThread(10000000LL);
    }

cleanup:
    curl_global_cleanup();
    socketExit();
    consoleExit(NULL);
    return 0;
}