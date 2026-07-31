#pragma once

#include <iostream>
#include <string>

class TestContext
{
  public:
    void expect(bool condition, const std::string& description, const char* file, int line)
    {
        if (condition) {
            return;
        }
        ++failures_;
        std::cerr << file << ':' << line << ": expectation failed: " << description << '\n';
    }

    int result() const
    {
        if (failures_ == 0) {
            std::cout << "All expectations passed\n";
        }
        return failures_ == 0 ? 0 : 1;
    }

  private:
    int failures_ = 0;
};

#define RC_EXPECT(context, condition)                                                              \
    (context).expect(static_cast<bool>(condition), #condition, __FILE__, __LINE__)

#define RC_EXPECT_EQ(context, actual, expected)                                                    \
    (context).expect((actual) == (expected), #actual " == " #expected, __FILE__, __LINE__)
