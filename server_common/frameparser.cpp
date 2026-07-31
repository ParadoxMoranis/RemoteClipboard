#include "frameparser.h"

#include <stdexcept>
#include <utility>

FrameParser::FrameParser(std::size_t maxFrameBytes) : maxFrameBytes_(maxFrameBytes)
{
    if (maxFrameBytes_ == 0) {
        throw std::invalid_argument("FrameParser maximum frame size must be positive");
    }
    currentFrame_.reserve(maxFrameBytes_ < 64 * 1024 ? maxFrameBytes_ : 64 * 1024);
}

bool FrameParser::append(const char* data, std::size_t size)
{
    if (overflowed_) {
        return false;
    }

    for (std::size_t index = 0; index < size; ++index) {
        const char value = data[index];
        if (value == '\n') {
            completedFrames_.push_back(std::move(currentFrame_));
            currentFrame_.clear();
            continue;
        }
        if (currentFrame_.size() >= maxFrameBytes_) {
            reset();
            overflowed_ = true;
            return false;
        }
        currentFrame_.push_back(value);
    }
    return true;
}

bool FrameParser::popFrame(std::string& frame)
{
    if (completedFrames_.empty()) {
        return false;
    }
    frame = std::move(completedFrames_.front());
    completedFrames_.pop_front();
    return true;
}

void FrameParser::reset()
{
    currentFrame_.clear();
    completedFrames_.clear();
    overflowed_ = false;
}

bool FrameParser::overflowed() const { return overflowed_; }

std::size_t FrameParser::pendingBytes() const { return currentFrame_.size(); }
