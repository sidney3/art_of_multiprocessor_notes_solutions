#include <unordered_map>
#include <iostream>
#include <algorithm>

int main()
{
  std::unordered_map<int,int> freq;
  for(int i = 0; i < 5; i++)
  {
    freq[i]++;
  }

  freq[2]++;
  std::cout << freq[2] << "\n";

  auto max_freq = std::max_element(freq.begin(),
                                  freq.end(),
                                  [](auto p1, auto p2){return p1.second <= p2.second;});

  std::cout << max_freq->first << "," << max_freq->second << "\n";
}
