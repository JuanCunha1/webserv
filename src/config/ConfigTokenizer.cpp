#include "config/ConfigTokenizer.hpp"

#include <cctype>
#include <stdexcept>

std::vector<std::string> ConfigTokenizer::tokenize(
    const std::string &content)
{
    std::vector<std::string> tokens;
    std::string token;
    bool inQuote = false;
    bool quoteClosed = false;

    for (std::size_t i = 0; i < content.size(); ++i)
    {
        char c = content[i];

        if (inQuote)
        {
            if (c == '\\' && i + 1 < content.size()
                && (content[i + 1] == '"' || content[i + 1] == '\\'))
            {
                token += content[++i];
            }
            else if (c == '"')
            {
                inQuote = false;
                quoteClosed = true;
                tokens.push_back(token);
                token.clear();
            }
            else
                token += c;
            continue;
        }

        if (quoteClosed)
        {
            if (!std::isspace(static_cast<unsigned char>(c))
                && c != '{' && c != '}' && c != ';' && c != '#')
            {
                throw std::runtime_error(
                    "ConfigTokenizer: unexpected character after quoted value");
            }
            quoteClosed = false;
        }

        if (c == '#')
        {
            while (i < content.size() && content[i] != '\n')
                ++i;
        }
        else if (c == '"')
        {
            if (!token.empty())
                throw std::runtime_error(
                    "ConfigTokenizer: quote must begin a token");
            inQuote = true;
        }
        else if (std::isspace(static_cast<unsigned char>(c)))
        {
            if (!token.empty())
            {
                tokens.push_back(token);
                token.clear();
            }
        }
        else if (c == '{' || c == '}' || c == ';')
        {
            if (!token.empty())
            {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back(std::string(1, c));
        }
        else
            token += c;
    }

    if (inQuote)
        throw std::runtime_error(
            "ConfigTokenizer: unterminated quoted value");

    if (!token.empty())
        tokens.push_back(token);

    return tokens;
}
