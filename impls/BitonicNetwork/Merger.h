#pragma once
#include "Balancer.h"
#include "NetworkSink.h"
#include "Wire.h"
#include "util.h"
#include <cassert>
#include <utility>
#include <span>

/*
    Merger(2k) is defined to take in two input sources, each of width k, and spit out
    to a single output source of width k.

    Feels awkward to have the Merger hold the input sources that it has and then have to poll them.
    That said, we don't want the input sources to have to identify themselves...

    No not at all - we want to hand out perhaps a list of network sinks? But how does each input know
    which sink to call? We want to hand out the sinks that others can connect to (these will be the NetworkSink objects)
    And each of these NetworkSinks will ping US when it has its method called.

    Question: how do we deal with the base case?

    Width == 2

    I guess in exactly the same way
    
*/
template<typename TokenType>
struct Merger
{
    constexpr Merger(size_t width)
    {
        assert(is_power_of_two(width) &&
                "width of merger must be power of two");

        inputStorage.first.reserve(width / 2);
        inputStorage.second.reserve(width / 2);
        inputView.first.reserve(width / 2);
        inputView.second.reserve(width / 2);

        // base case
        if(width == 2)
        {
            sub_mergers = {nullptr, nullptr};
        }
        else
        {

        }
    }

    std::pair<std::span<NetworkSink<TokenType>*>,
              std::span<NetworkSink<TokenType>*>> 
    getInput()
    {
        return inputView;
    }
private:
    size_t width;

    std::pair<std::vector<Wire<TokenType>>, 
              std::vector<Wire<TokenType>>> inputStorage;

    std::pair<std::vector<NetworkSink<TokenType>*>,
              std::vector<NetworkSink<TokenType>*>> inputView;

    std::pair<std::unique_ptr<Merger<TokenType>>, std::unique_ptr<Merger<TokenType>>> 
    sub_mergers;

};
