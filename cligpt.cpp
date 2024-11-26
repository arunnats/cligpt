#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include "nlohmann/json.hpp"

// For convenience
using json = nlohmann::json;

// Handle response from libcurl
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *out)
{
    size_t totalSize = size * nmemb;
    out->append((char *)contents, totalSize);
    return totalSize;
}

// Function to load API key from .env
std::string loadApiKey()
{
    std::ifstream envFile(".env");
    std::string line;

    if (envFile.is_open())
    {
        std::getline(envFile, line);
        envFile.close();
    }
    return line;
}

// Function to save API key to .env
void saveApiKey(const std::string &apiKey)
{
    std::ofstream envFile(".env");
    if (envFile.is_open())
    {
        envFile << apiKey;
        envFile.close();
    }
}

// Function to manage API key with the -key flag
void manageApiKey()
{
    std::string currentKey = loadApiKey();
    int choice;

    std::cout << "API Key Management:\n";
    std::cout << "1) Remove API Key\n";
    std::cout << "2) Update API Key\n";
    std::cout << "3) View API Key\n";
    std::cout << "Enter your choice: ";
    std::cin >> choice;
    std::cin.ignore(); // Ignore leftover newline

    switch (choice)
    {
    case 1: // Remove API Key
        saveApiKey("");
        std::cout << "API key removed.\n";
        break;
    case 2: // Update API Key
    {
        std::string newKey;
        std::cout << "Enter new API key: ";
        std::getline(std::cin, newKey);
        saveApiKey(newKey);
        std::cout << "API key updated.\n";
        break;
    }
    case 3: // View API Key
        if (currentKey.empty())
        {
            std::cout << "No API key found.\n";
        }
        else
        {
            std::cout << "Current API key: " << currentKey.substr(0, 5) << "*****" << currentKey.substr(currentKey.size() - 3) << "\n";
        }
        break;
    default:
        std::cout << "Invalid choice.\n";
    }
    exit(0);
}

std::string sendToChatGPT(const std::string &prompt, const std::string &apiKey)
{
    const std::string apiUrl = "https://api.openai.com/v1/chat/completions";

    CURL *curl = curl_easy_init();
    std::string responseString;

    if (curl)
    {
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Create the JSON body
        json body = {
            {"model", "gpt-3.5-turbo"},
            {"messages", {{{"role", "user"}, {"content", prompt}}}}};

        std::string bodyString = body.dump();

        curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyString.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            std::cerr << "Curl request failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return responseString;
}

void processResponse(const std::string &response)
{
    try
    {
        auto jsonResponse = json::parse(response);
        std::string content = jsonResponse["choices"][0]["message"]["content"];
        std::cout << "\nChatGPT: " << content << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing response: " << e.what() << std::endl;
    }
}

int main(int argc, char *argv[])
{
    // Check for the -key flag
    if (argc > 1 && std::string(argv[1]) == "-key")
    {
        manageApiKey();
    }

    std::string apiKey = loadApiKey();

    // If no API key or key is invalid, prompt user for input
    while (apiKey.empty())
    {
        std::cout << "No API key found. Please enter your OpenAI API key: ";
        std::getline(std::cin, apiKey);
        saveApiKey(apiKey);
    }

    // Main loop
    while (true)
    {
        std::string prompt;
        std::cout << "\nYou: ";
        std::getline(std::cin, prompt);

        if (prompt == "exit")
        {
            break;
        }

        std::string response = sendToChatGPT(prompt, apiKey);
        processResponse(response);
    }

    return 0;
}
