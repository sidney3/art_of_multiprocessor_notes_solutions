#include <any>
#include <cassert>

template<typename T>
struct stamped_value
{
    int stamp;
    T value;

    stamped_value(T value)
        : stamp(0), value(value)
    {}
    stamped_value(T value, int index)
        : stamp(index), value(value)
    {}
    static stamped_value<T> max(const auto& lhs, const auto& rhs)
    {
        return (lhs.stamp >= rhs.stamp) ? lhs : rhs;
    }

};

template<typename T>
static const stamped_value<T> min_stamped_value()
{
    return stamped_value(T{}, -1);
}

int main()
{
    stamped_value<int> sv{5};
    assert(stamped_value<int>::max(sv, min_stamped_value<int>()).value == sv.value);
}
