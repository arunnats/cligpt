#include <iostream>
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

int main()
{
    std::string apiKey;
    std::cout << "Enter your OpenAI API Key: ";
    std::cin >> apiKey;

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
