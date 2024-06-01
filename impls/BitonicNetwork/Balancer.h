#pragma once
#include "NetworkSink.h"
#include <mutex>

/*
   Make this the simplest possible balancer
*/
template<typename TokenType>
struct Balancer : NetworkSink<TokenType>
{
    constexpr Balancer(NetworkSink<TokenType> upSink, NetworkSink<TokenType> downSink)
        : upSink{upSink}, downSink{downSink}
    {}

    void pushToken(TokenType token)
    {
        std::unique_lock lk{mtx};

        if(state)
        {
            upSink->pushToken(token);
        }
        else
        {
            downSink->pushToken(token);
        }

        state = !state;
    }
private:
    std::mutex mtx{};
    bool state{true};
    NetworkSink<TokenType> *upSink;
    NetworkSink<TokenType> *downSink;
};
