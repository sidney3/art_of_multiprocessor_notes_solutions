#include <span>
#include <vector>
#include <iostream>


void do_stuff(std::span<const int> vec)
{
  // do stuff with vec
  std::cout << vec[0] << "\n";
}

int main()
{
  std::vector<int> v = {1,2,3};
  do_stuff(v);
}
