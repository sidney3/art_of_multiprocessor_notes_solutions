#pragma once
/*
    Note for later: this can be adapted to be a template
*/
template<typename TokenType>
struct NetworkSink
{
    void pushToken(TokenType token) = delete;
};
