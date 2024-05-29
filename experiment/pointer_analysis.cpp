#include <memory>
#include <string>

__attribute__((noinline))
void foo(int& a)
{
}

std::string print_ptr(uintptr_t ptr)
{
    std::string res;
}

int main()
{
  auto j = std::make_unique<int>(5);
  foo(*j);
}
