#include <atomic>
#include <cassert>

int main()
{
  int j = 5;
  int* j_p = &j;

  int r = 5;
  int* r_p = &r;
  std::atomic<int*> i = j_p;

  assert(i.compare_exchange_strong(r_p, nullptr));

}
