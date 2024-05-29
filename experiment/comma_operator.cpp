#include <iostream>
int main()
{
  int i = (static_cast<void>(6), 5);
  std::cout << i << "\n";
}
