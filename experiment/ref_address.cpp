#include <cinttypes>
#include <iostream>
void foo(int& x)
{
  uintptr_t y = reinterpret_cast<uintptr_t>(&x);
  std::cout << y;
}

int main()
{
  int y = 8;
  uintptr_t y_addr = reinterpret_cast<uintptr_t>(&y);
  std::cout << y_addr;

  foo(y);
}
