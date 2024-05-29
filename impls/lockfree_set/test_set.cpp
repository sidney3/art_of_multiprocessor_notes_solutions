#include "lockfree_set.h"
#include <future>
#include <cassert>

template<class Set>
void test_set(Set&& S)
{
    S.insert(5);
    assert(S.remove(5));
    assert(!S.contains(5));
    S.insert(9);
    S.insert(10);
    S.insert(11);
    S.insert(2);
    assert(S.contains(9));
    assert(S.remove(9));
    auto fut1 = std::async(std::launch::async, [&S](){
            for(int i = 0; i < 1000; i++)
            {
                S.insert(5);
                assert(!S.insert(5));
                S.remove(5);
                assert(!S.contains(5));
                S.insert(5);
                assert(S.contains(5));
            }
            });
    auto fut2 = std::async(std::launch::async, [&S](){
            for(int i = 0; i < 1000; i++)
            {
                S.insert(6);
                assert(S.contains(6));
                S.insert(7);
                assert(S.contains(7));
                assert(S.remove(7));
                assert(!S.contains(7));
            }
            });
    auto fut3 = std::async(std::launch::async, [&S](){
            for(int i = 0; i < 1000; i++)
            {

                S.insert(9);
                assert(S.remove(9));
                S.insert(13);
                assert(S.remove(13));
                S.remove(15);
                S.insert(15);
                S.insert(17);
                assert(S.contains(17));
                assert(!S.contains(14));
            }
            }
        );
    auto fut4 = std::async(std::launch::async, [&S](){
            for(int i = 0; i < 1000; i++)
            {

                S.insert(1);
            }
            }
        );
    auto fut5 = std::async(std::launch::async, [&S](){
            for(int i = 0; i < 1000; i++)
            {
                S.remove(1);
            }
            }
        );

    
    fut1.get();
    fut2.get();
    fut3.get();
    fut4.get();
    fut5.get();
}


int main()
{
    test_set(Set<int>{});
}
