#include <thread>
#include <iostream>

struct LoudObject
{
    LoudObject() 
    {
        std::cout << "LoudObject() " << std::this_thread::get_id() << "\n";
    }

    void foo()
    {}

    ~LoudObject()
    {
        std::cout << "~LoudObject() " << std::this_thread::get_id() << "\n";
    }
};


struct S
{
    static thread_local LoudObject x;
};

thread_local LoudObject S::x{};


int main()
{
    S s;
    s.x.foo();

    std::thread th{[&s](){
        std::cout << "from thread: " << std::this_thread::get_id() << "\n";
        s.x.foo();
    }};

    th.join();
}
