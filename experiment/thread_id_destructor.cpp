#include <thread>
#include <iostream>

struct A
{
    A()
    {
        std::cout << "A: " << std::this_thread::get_id() << "\n";
    }
    ~A()
    {
        std::cout << "~A: " << std::this_thread::get_id() << "\n";
    }
};

void thr()
{
    A a{};
}

int main()
{
    std::cout << "main thread id: " << std::this_thread::get_id() << "\n";
    A a{};

    std::thread t{thr};

    t.join();
}
