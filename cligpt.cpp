#include <iostream>
#include <curl/curl.h>
#include <deque>
#include <string>
#include "nlohmann/json.hpp"
#include <fstream>
#include "cligpt.h"

using json = nlohmann::json;

// Constants for text colors
const std::string CYAN = "\033[36m";
const std::string GREEN = "\033[32m";
const std::string RESET = "\033[0m";

// For storing the last 10 messages
std::deque<std::pair<std::string, std::string>> messageHistory;

// Handle response from libcurl
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *out)
{
    size_t totalSize = size * nmemb;
    out->append((char *)contents, totalSize);
    return totalSize;
}

// Read environment variables from .env file
std::string readEnv(const std::string &key, const std::string &defaultValue = "")
{
    std::ifstream file(".env");
    std::string line;
    while (std::getline(file, line))
    {
        if (line.find(key + "=") == 0)
        {
            return line.substr(key.length() + 1);
        }
    }
    return defaultValue;
}

// Write or update environment variables in .env file
void writeEnv(const std::string &key, const std::string &value)
{
    std::ifstream fileIn(".env");
    std::ofstream fileOut(".env.temp");
    bool updated = false;

    std::string line;
    while (std::getline(fileIn, line))
    {
        if (line.find(key + "=") == 0)
        {
            fileOut << key << "=" << value << "\n";
            updated = true;
        }
        else
        {
            fileOut << line << "\n";
        }
    }

    if (!updated)
    {
        fileOut << key << "=" << value << "\n";
    }

    fileIn.close();
    fileOut.close();
    std::remove(".env");
    std::rename(".env.temp", ".env");
}

// Setup .env file with default values
void setupEnv()
{
    std::ofstream file(".env");
    file << "GPT_KEY=\n";
    file << "GPT_NAME=ChatGPT\n";
    file << "PERSONALITY=Friendly AI\n";
    file << "USER_NAME=User\n";
    file.close();

    std::cout << GREEN
              << "Welcome to CLI GPT!\n"
              << "This is a command-line interface for interacting with ChatGPT.\n"
              << "You'll need an OpenAI API key to use this app.\n\n"
              << "Flags:\n"
              << "  -help        Show this help message\n"
              << "  -key         Manage your API key (view, update, remove)\n"
              << "  -customize   Customize ChatGPT's name, personality, or your name\n"
              << RESET;
}

void showHelp()
{
    std::cout << GREEN
              << "CLI GPT - Command Line Interface for ChatGPT\n"
              << "Usage:\n"
              << "  ./cligpt            Start the app\n"
              << "  ./cligpt -key       Manage your API key\n"
              << "  ./cligpt -customize Customize ChatGPT's settings\n"
              << "  ./cligpt -help      Show this help message\n"
              << RESET;
}

void customize()
{
    int choice;
    do
    {
        std::cout << GREEN << "Customize Settings:\n"
                  << "1. ChatGPT's Name\n"
                  << "2. ChatGPT's Personality\n"
                  << "3. Your Name\n"
                  << "4. Continue to App\n"
                  << "Enter your choice: \n"
                  << RESET;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 1)
        {
            std::string name;
            std::cout << "Enter ChatGPT's new name: ";
            std::getline(std::cin, name);
            writeEnv("GPT_NAME", name);
        }
        else if (choice == 2)
        {
            std::string personality;
            while (true)
            {
                std::cout << "Enter ChatGPT's new personality (max 200 characters): ";
                std::getline(std::cin, personality);
                if (personality.size() <= 200)
                {
                    writeEnv("PERSONALITY", personality);
                    break;
                }
                else
                {
                    std::cout << "Personality too long! Would you like to try again? (yes/no): ";
                    std::string retry;
                    std::getline(std::cin, retry);
                    if (retry == "no")
                        break;
                }
            }
        }
        else if (choice == 3)
        {
            std::string userName;
            std::cout << "Enter your new name: ";
            std::getline(std::cin, userName);
            writeEnv("USER_NAME", userName);
        }
        else if (choice == 4)
        {
            mainLoop();
        }
        else
        {
            std::cout << GREEN << "\nInvalid Choice. Try again \n"
                      << RESET;
            break;
        }

    } while (1);
}

// Menu to manage the key
void manageAPIKey()
{
    std::string apiKey = readEnv("GPT_KEY");

    int choice;
    std::cout << GREEN
              << "API Key Management:\n"
              << "1. View Key\n"
              << "2. Update Key\n"
              << "3. Remove Key\n"
              << "4. Continue to App\n"
              << "Enter your choice: "
              << RESET;

    std::cin >> choice;
    std::cin.ignore();

    if (choice == 1)
    {
        if (apiKey.empty())
        {
            std::cout << "No API Key found.\n";
        }
        else
        {
            std::cout << "API Key: " << apiKey.substr(0, 5) << "*****" << apiKey.substr(apiKey.length() - 3) << "\n";
        }
    }
    else if (choice == 2)
    {
        std::cout << "Enter new API Key: ";
        std::cin >> apiKey;
        writeEnv("GPT_KEY", apiKey);
    }
    else if (choice == 3)
    {
        writeEnv("GPT_KEY", "");
        std::cout << "API Key removed.\n";
    }
    else if (choice == 4)
    {
        mainLoop();
    }
}

// API call to OpenAI
std::string sendToChatGPT(const std::string &prompt, const std::string &apiKey, const std::string &personality)
{
    const std::string apiUrl = "https://api.openai.com/v1/chat/completions";

    CURL *curl = curl_easy_init();
    std::string responseString;

    if (curl)
    {
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Create the JSON body, including the memory
        json messages;

        // Add the system message to set the personality
        messages.push_back({{"role", "system"},
                            {"content", "You are " + personality + "."}});

        // Add past conversation history
        for (const auto &entry : messageHistory)
        {
            messages.push_back({{"role", "user"}, {"content", entry.first}});
            messages.push_back({{"role", "assistant"}, {"content", entry.second}});
        }

        // Add the new prompt
        messages.push_back({{"role", "user"}, {"content", prompt}});

        json body = {
            {"model", "gpt-3.5-turbo"},
            {"messages", messages}};

        std::string bodyString = body.dump();

        curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyString.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            std::cerr << "Curl request failed: " << curl_easy_strerror(res) << "\n";
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return responseString;
}

// Add responses to message history
void processResponse(const std::string &response, const std::string &prompt, const std::string &gptName)
{
    try
    {
        auto jsonResponse = json::parse(response);
        std::string content = jsonResponse["choices"][0]["message"]["content"];
        std::cout << GREEN << "\n"
                  << gptName << ": " << RESET << content << "\n";

        // Store the new question-response pair in memory
        messageHistory.push_back({prompt, content});

        // Ensure only the last 10 messages are kept
        if (messageHistory.size() > 10)
        {
            messageHistory.pop_front();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing response: " << e.what() << "\n";
    }
}

void mainLoop()
{
    // Read values from .env or initialize defaults
    if (!std::ifstream(".env"))
    {
        setupEnv();
    }

    std::string apiKey = readEnv("GPT_KEY");
    std::string gptName = readEnv("GPT_NAME", "ChatGPT");
    std::string userName = readEnv("USER_NAME", "User");
    std::string personality = readEnv("PERSONALITY", "Friendly AI");

    if (apiKey.empty())
    {
        std::cout << "No API Key found! Use './cligpt -key' to add one.\n";
    }

    std::cout << CYAN << "Welcome to " << gptName << "! " << "Enter 'exit' to quit.\n"
              << RESET;
    while (true)
    {
        std::cout << CYAN << "\n"
                  << userName << ": " << RESET;
        std::string prompt;
        std::getline(std::cin, prompt);

        if (prompt == "exit")
        {
            exit(0);
        }

        std::string response = sendToChatGPT(prompt, apiKey, personality);
        processResponse(response, prompt, gptName);
    }
}

int main(int argc, char *argv[])
{
    // Handle flags
    if (argc > 1)
    {
        std::string flag = argv[1];
        if (flag == "-help")
        {
            showHelp();
        }
        else if (flag == "-key")
        {
            manageAPIKey();
        }
        else if (flag == "-customize")
        {
            customize();
        }
        return 0;
    }

    mainLoop();

    return 0;
}
