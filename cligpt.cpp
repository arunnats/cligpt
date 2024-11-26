#include <iostream>
#include <curl/curl.h>
#include <deque>
#include <string>
#include "nlohmann/json.hpp"
#include <fstream>

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

// Get the stored API Key
std::string getAPIKey()
{
    const std::string envFile = ".env";
    std::ifstream file(envFile);
    std::string apiKey;

    if (file.is_open())
    {
        std::getline(file, apiKey);
        file.close();
    }

    if (apiKey.empty())
    {
        std::cout << "No API Key found. Please enter your OpenAI API Key: ";
        std::cin >> apiKey;
        std::ofstream outFile(envFile);
        outFile << apiKey;
        outFile.close();
    }

    return apiKey;
}

// Menu to manage the key
void manageAPIKey()
{
    const std::string envFile = ".env";
    std::ifstream file(envFile);
    std::string apiKey;

    if (file.is_open())
    {
        std::getline(file, apiKey);
        file.close();
    }

    int choice;
    std::cout << "API Key Management:\n";
    std::cout << "1. View Key\n";
    std::cout << "2. Update Key\n";
    std::cout << "3. Remove Key\n";
    std::cout << "Enter your choice: ";
    std::cin >> choice;

    if (choice == 1)
    {
        if (apiKey.empty())
        {
            std::cout << "No API Key found.\n";
        }
        else
        {
            std::cout << "API Key: " << apiKey.substr(0, 5) << "*****" << apiKey.substr(apiKey.length() - 3) << std::endl;
        }
    }
    else if (choice == 2)
    {
        std::cout << "Enter new API Key: ";
        std::cin >> apiKey;
        std::ofstream outFile(envFile);
        outFile << apiKey;
        outFile.close();
    }
    else if (choice == 3)
    {
        std::ofstream outFile(envFile);
        outFile.close();
        std::cout << "API Key removed.\n";
    }
}

// Api call to openAI
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
            std::cerr << "Curl request failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return responseString;
}

// Add responses to message history
void processResponse(const std::string &response, const std::string &prompt)
{
    try
    {
        auto jsonResponse = json::parse(response);
        std::string content = jsonResponse["choices"][0]["message"]["content"];
        std::cout << "\nChatGPT: " << content << std::endl;

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
        std::cerr << "Error parsing response: " << e.what() << std::endl;
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
            return 0;
        }
        else if (flag == "-key")
        {
            manageAPIKey();
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

    std::cin.ignore(); // Clear input buffer

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
