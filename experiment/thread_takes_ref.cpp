#include <thread>
#include <iostream>

void thr(int& a)
{
  a++;
}

int main()
{
  std::vector<int> v{};

  for(size_t i = 0; i < 5; i++)
  {
    v.push_back(i);
  }

  std::thread th{std::ref(v[0])};

  th.join();
  std::cout << v[0] << "\n";
}
