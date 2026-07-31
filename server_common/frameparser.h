#pragma once

#include <cstddef>
#include <deque>
#include <string>

class FrameParser
{
  public:
    explicit FrameParser(std::size_t maxFrameBytes);

    bool append(const char* data, std::size_t size);
    bool popFrame(std::string& frame);
    void reset();

    bool overflowed() const;
    std::size_t pendingBytes() const;

  private:
    const std::size_t maxFrameBytes_;
    std::string currentFrame_;
    std::deque<std::string> completedFrames_;
    bool overflowed_ = false;
};
