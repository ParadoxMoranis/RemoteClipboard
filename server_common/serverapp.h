#pragma once

#include <cstddef>
#include <string>

int runServerApplication(int argc, char* argv[], const std::string& configName,
                         const std::string& displayName, std::size_t maxMessageBytes);
