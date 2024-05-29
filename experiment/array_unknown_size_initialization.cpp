#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <iostream>

struct not_default_constructible {
  not_default_constructible(int) {}
};

static_assert(!std::is_default_constructible_v<not_default_constructible>);

template <typename T, long N>
struct array_of_ndc {
public:
  std::array<T, N> arr;

  array_of_ndc(T init) 
      : array_of_ndc(init, std::make_index_sequence<N>())
  {
  }
private:
  template<size_t ... I>
  array_of_ndc(T init, std::index_sequence<I...>)
    : arr{ (static_cast<void>(I), init)... }
  // NOTE: the effect of the comma operator is that the left hand side gets ignored!
  // we just cast it to void to avoid getting a compiler warning
  {}
};

int main() {
  array_of_ndc<not_default_constructible, 1> a{not_default_constructible{5}};
}
