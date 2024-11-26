#include <iostream>
#include <fstream>
#include <string>
#include <deque>
#include <curl/curl.h>
#include "nlohmann/json.hpp"

// For convenience
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
            fileOut << key << "=" << value << std::endl;
            updated = true;
        }
        else
        {
            fileOut << line << std::endl;
        }
    }

    if (!updated)
    {
        fileOut << key << "=" << value << std::endl;
    }

    fileIn.close();
    fileOut.close();
    std::remove(".env");
    std::rename(".env.temp", ".env");
}

void setupEnv()
{
    std::ofstream file(".env");
    file << "GPT_KEY=\n";
    file << "GPT_NAME=ChatGPT\n";
    file << "PERSONALITY=Friendly AI\n";
    file << "USER_NAME=User\n";
    file.close();

    std::cout << "Welcome to CLI GPT!\n"
              << "This is a command-line interface for interacting with ChatGPT.\n"
              << "You'll need an OpenAI API key to use this app.\n\n"
              << "Flags:\n"
              << "  -help        Show this help message\n"
              << "  -key         Manage your API key (view, update, remove)\n"
              << "  -customize   Customize ChatGPT's name, personality, or your name\n";
}

void showHelp()
{
    std::cout << "CLI GPT - Command Line Interface for ChatGPT\n"
              << "Usage:\n"
              << "  ./cligpt            Start the app\n"
              << "  ./cligpt -key       Manage your API key\n"
              << "  ./cligpt -customize Customize ChatGPT's settings\n"
              << "  ./cligpt -help      Show this help message\n";
}

void customize()
{
    int choice;
    do
    {
        std::cout << "Customize Settings:\n"
                  << "1. ChatGPT's Name\n"
                  << "2. ChatGPT's Personality\n"
                  << "3. Your Name\n"
                  << "4. Back to Main Menu\n"
                  << "Enter your choice: ";
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
                std::cout << "Enter ChatGPT's new personality (max 100 characters): ";
                std::getline(std::cin, personality);
                if (personality.size() <= 100)
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
    } while (choice != 4);
}

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
        for (const auto &entry : messageHistory)
        {
            messages.push_back({{"role", "user"}, {"content", entry.first}});
            messages.push_back({{"role", "assistant"}, {"content", entry.second}});
        }

        // Add the new prompt
        messages.push_back({{"role", "user"}, {"content", prompt}});

        json body = {
            {"model", "gpt-3.5-turbo"},
            {"messages", messages},
            {"temperature", 0.7}};

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

int main(int argc, char *argv[])
{
    // Handle flags
    if (argc > 1)
    {
        std::string flag = argv[1];
        if (flag == "-help")
        {
            showHelp();
            return 0;
        }
        else if (flag == "-key")
        {
            // Implement key management
            return 0;
        }
        else if (flag == "-customize")
        {
            customize();
            return 0;
        }
    }

    // Initialize .env if not found
    std::ifstream file(".env");
    if (!file)
    {
        setupEnv();
        return 0;
    }

    // Load settings
    std::string apiKey = readEnv("GPT_KEY");
    std::string gptName = readEnv("GPT_NAME", "ChatGPT");
    std::string personality = readEnv("PERSONALITY", "Friendly AI");
    std::string userName = readEnv("USER_NAME", "User");

    // Start interaction
    std::cout << CYAN << userName << RESET << ": Hello! Type 'exit' to quit.\n";
    while (true)
    {
        std::string prompt;
        std::cout << CYAN << userName << ": " << RESET;
        std::getline(std::cin, prompt);

        if (prompt == "exit")
            break;

        std::string response = sendToChatGPT(prompt, apiKey, personality);
        std::cout << GREEN << gptName << ": " << RESET << response << "\n";
    }

    return 0;
}
