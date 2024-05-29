#include <mutex> 

struct S
{
    std::mutex m1;
    std::mutex m2;

    void foo();
};

void S::foo()
{
    std::unique_lock<std::mutex> lk(m1);

    for(int i = 0; i < 1; i++) 
    {
        std::unique_lock<std::mutex> lk2(m2);
        lk.swap(lk2);
    }

}

int main()
{
    S s{};
}
