#include <string>
#include <string_view>
#include <iostream>

int main()
{
    std::string S = "abcd";
    std::string_view s_view(S.begin() + 1, S.end());
    std::cout << s_view << "\n";

}

