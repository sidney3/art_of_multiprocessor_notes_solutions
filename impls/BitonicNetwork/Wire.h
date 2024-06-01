#include "NetworkSink.h"

template<typename TokenType>
struct Wire : NetworkSink<TokenType>
{
    Wire(NetworkSink<TokenType>* sink)
        : sink(sink)
    {}

    void pushToken(TokenType token)
    {
        sink->pushToken(token);
    }
private:
    NetworkSink<TokenType>* sink;
};

