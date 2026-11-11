#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

class OneSevenLiveWebsocketMessageAssembler {
   public:
    enum class Result { Incomplete, Complete, TooLarge };

    explicit OneSevenLiveWebsocketMessageAssembler(std::size_t maxBytes) : maxBytes_(maxBytes) {}

    Result append(std::string_view chunk, bool final, std::string& message) {
        if (chunk.size() > maxBytes_ - pending_.size()) {
            pending_.clear();
            fragmented_ = false;
            return Result::TooLarge;
        }
        if (!chunk.empty())
            pending_.append(chunk.data(), chunk.size());
        if (!final) {
            fragmented_ = true;
            return Result::Incomplete;
        }
        message = std::move(pending_);
        pending_.clear();
        fragmented_ = false;
        return Result::Complete;
    }

    bool hasIncompleteMessage() const {
        return fragmented_;
    }

   private:
    std::size_t maxBytes_;
    std::string pending_;
    bool fragmented_ = false;
};
