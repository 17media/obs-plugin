#pragma once

#include <cstddef>
#include <deque>
#include <utility>

#include "../websocket/WsMessage.hpp"

class WsMessageQueue {
   public:
    explicit WsMessageQueue(size_t maxSize) : maxSize_(maxSize) {}

    void setMaxSize(size_t maxSize) {
        maxSize_ = maxSize;
        trim();
    }

    size_t maxSize() const {
        return maxSize_;
    }

    size_t size() const {
        return q_.size();
    }

    bool empty() const {
        return q_.empty();
    }

    void clear() {
        q_.clear();
    }

    void enqueue(WsMessage m) {
        q_.push_back(std::move(m));
        trim();
    }

    WsMessage front() const {
        return q_.front();
    }

    void popFront() {
        q_.pop_front();
    }

   private:
    void trim() {
        while (q_.size() > maxSize_) {
            q_.pop_front();
        }
    }

    std::deque<WsMessage> q_;
    size_t maxSize_ = 0;
};

