#pragma once
#include <type_traits>

template<typename Numeric>
    requires std::is_integral_v<Numeric>
constexpr bool is_power_of_two(Numeric x)
{
    return !(x & (x - 1));
}

static_assert(is_power_of_two(1<<5));
static_assert(!is_power_of_two(9));
