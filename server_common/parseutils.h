#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

inline int parseIntArg(const std::string& value, const char* optionName)
{
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0') {
        throw std::invalid_argument(std::string("Invalid value for ") + optionName + ": " + value);
    }
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        throw std::out_of_range(std::string("Value out of range for ") + optionName + ": " + value);
    }
    return static_cast<int>(parsed);
}

inline uint16_t parsePortArg(const std::string& value, const char* optionName)
{
    const int parsed = parseIntArg(value, optionName);
    if (parsed < 1 || parsed > std::numeric_limits<uint16_t>::max()) {
        throw std::out_of_range(std::string("Port out of range for ") + optionName + ": " + value);
    }
    return static_cast<uint16_t>(parsed);
}
