#include <type_traits>
#include <any>
#include <array>

struct not_default_constructible
{
    not_default_constructible(int){}
};

static_assert(!std::is_default_constructible_v<not_default_constructible>);

template<typename T, long N>
struct ArrayWrapper 
{
    std::array<T, N> arr;

    template<typename ... Args>
    ArrayWrapper(Args&&... args) 
    {
        // initialize each element of arr with T{args...}
    }
};

int main()
{
    ArrayWrapper<not_default_constructible, 5> aw{5};
}
