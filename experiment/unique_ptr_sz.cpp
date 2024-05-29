#include <memory>

int main()
{
  static_assert(sizeof(std::shared_ptr<int>) == 16);
}
