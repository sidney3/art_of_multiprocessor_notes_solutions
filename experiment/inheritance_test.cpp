#include <iostream>

struct Base
{
    void foo()
    {
      std::cout << "from base\n";
    }
};

struct Derived : Base
{
  void foo()
  {
    Base::foo();
    std::cout << "from Derived\n";
  }
};

int main()
{
  Derived d{};
  d.foo();
}
