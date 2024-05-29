#include <array>
#include <type_traits>
#include <atomic>
#include <iostream>


constexpr std::array<bool, 8> get_bits(char c)
{
  std::array<bool, 8> res;
  for(size_t i = 0; i < 8; i++)
  {
    res[7-i] = (static_cast<uint8_t>(c) & (1<<i));
  }
  return res;
}

template<typename T>
constexpr decltype(auto) get_bytes(T&& obj) 
{
  using U = std::decay_t<T>; 
  union u_t
  {
    u_t(){}
    U obj;
    char data[sizeof(U)];
  } u;

  memcpy(&u.obj, &obj, sizeof(obj));

  std::array<char, sizeof(U)> res;
  for(size_t i = 0; i < sizeof(U); i++)
  {
    res[sizeof(U) - i - 1] = u.data[i];
  }

  return res;
}

template<typename T>
constexpr decltype(auto) get_bits(T&& obj) 
{
  using U = std::decay_t<T>;

  std::array<char, sizeof(U)> bytes = get_bytes(obj);

  std::array<bool, sizeof(U) * 8> res;

  std::array<bool, 8> tmp;

  for(size_t i = 0; i < sizeof(U); i++)
  {
    tmp = get_bits(bytes[i]);
    for(size_t j = 0; j < 8; j++)
    {
      res[i * 8 + j] = tmp[j];
    }
  }

  return res;
};

int main()
{
  std::atomic<bool> i = false;
  std::array<bool, sizeof(i) * 8> bits = get_bits(i);
  for(size_t i = 0; i < bits.size(); i++)
  {
    std::cout << bits[i];
  }
  std::cout << "\n";
}
