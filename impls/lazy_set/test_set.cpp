#include "lazy_set.h"

template<template <typename T> class Set>
void test_set()
{
    Set<int> S;
    S.insert(5);
    assert(S.remove(5));
    assert(!S.contains(5));
    S.insert(9);
    S.insert(10);
    S.insert(11);
    S.insert(2);
    assert(S.contains(9));
    assert(S.remove(9));
    assert(!S.contains(9));
    auto fut1 = std::async(std::launch::async, [&S](){
            S.insert(5);
            assert(!S.insert(5));
            S.remove(5);
            assert(!S.contains(5));
            S.insert(5);
            assert(S.contains(5));
            });
    auto fut2 = std::async(std::launch::async, [&S](){
            S.insert(6);
            assert(S.contains(6));
            S.insert(7);
            assert(S.contains(7));
            });
    auto fut3 = std::async(std::launch::async, [&S](){
            S.insert(7);
            S.insert(9);
            assert(S.remove(9));
            S.insert(13);
            S.insert(15);
            S.insert(17);
            assert(S.contains(17));
            assert(!S.contains(14));
            });
    auto fut4 = std::async(std::launch::async, [&S](){
            S.insert(8);
            });
    
    fut1.get();
    fut2.get();
    fut3.get();
    fut4.get();
}
int main()
{
    test_set<set>();
}
