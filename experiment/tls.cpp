#include <iostream>
#include <thread>

thread_local struct S
{
  S()
  {
    std::cout << "constructor from" << std::this_thread::get_id() << "\n";
  }
  ~S()
  {
    std::cout << "destructor from: " << std::this_thread::get_id() << "\n";
  }
  int x = 5;
} S_t;

int main()
{
  S_t.x = 9;

  std::thread th{[](){
    std::cout << S_t.x << "\n";
    S_t.x = 1;

  }};

  th.join();
  std::cout << S_t.x << "\n";

}
