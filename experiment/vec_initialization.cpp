#include <vector>
#include <iostream>
#include <span>

struct S
{
    S(int member) : _member(member)
    {}
    int _member;
};

template<long M>
struct S_vec
{
    S_vec() : v(M, S{1})
    {}
    std::vector<S> v;
};

int main()
{
    S_vec<10> s;

    std::cout << s.v[0]._member << "\n";
}
