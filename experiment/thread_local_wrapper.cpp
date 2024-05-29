#include <thread>
#include <functional>
#include <unordered_map>
#include <type_traits>
#include <thread>
#include <cassert>

template<typename T, typename ... Args>
concept Callable = requires(T t, Args... args)
{
    {t(args...)};
};

template<typename IdType, typename T>
struct tl 
{
    std::optional<T> load(IdType id)
    {
        auto it = _ptr_to_value.find(id);
        if(it == _ptr_to_value.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    void set(IdType id, T value)
    {
        _ptr_to_value.insert({id, value});
    }

private:
    std::unordered_map<IdType, T> _ptr_to_value;
};


struct not_default_constructible
{
    int x;
    not_default_constructible(int x)
    {}
};

static_assert(!std::is_default_constructible_v<not_default_constructible>);
// use
struct A
{
    int i;
    A() : i(0)
    {}

    int get()
    {
        return value.get(this);
    }
    void set(int x)
    {
        value.set(this, x);
    }
    static thread_local tl<A*, int> value;

};

thread_local tl<A*, int> A::value{};

void setter_thread(A& a, int x)
{
    a.set(x);
}

int main()
{
    A a;
    a.set(5);
    std::thread th{setter_thread, std::ref(a), 6};

    th.join();
    assert(a.get() == 5);

}
