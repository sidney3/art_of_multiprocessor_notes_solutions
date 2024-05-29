#include <memory>

int main()
{
  std::unique_ptr<int> p = std::make_unique<int>(5);
  std::unique_ptr<int>& p_ref = p;
}
