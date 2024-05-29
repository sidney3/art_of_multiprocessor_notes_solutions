#include <array>
#include <functional>
#include <utility>
struct myStruct
{
    std::array<std::array<int,5>,5> iF;
    std::array<std::array<int,5>,5> jF;
    std::array<std::array<int,5>,5> someOtherArray;
};

/* __attribute__((always_inline)) */
/* void applyCallable(Array& arr, Callable&& fn) */
/* { */
/*     for(int i = 0; i < Rows; i++) */
/*     { */
/*         for(int j = 0; j < Cols; j++) */
/*         { */
/*             arr[i][j] = fn(i,j); */
/*         } */
/*     } */
/* } */

template<long ... Dimensions>
void applyCallable(auto& arr, auto&& fn)
{
  
}

template<typename ... Ts>
struct type_list{};

/*
Given two lists of tuples, returns the cartesian product of both
*/
template<typename List1, typename List2>
struct BinaryCartesian;

template<typename ... Tuples1, typename ... Tuples2>
struct BinaryCartesian<type_list<Tuples1...>, type_list<Tuples2...>>
{

};

template<typename ... Lists>
struct CartesianProduct;

template<typename Head, typename ... Rest>
struct CartesianProduct<Head, Rest...>
{

};


void fn(myStruct &b)
{
    applyCallable<10,9>(b.iF, [&b](int row, int col)
    {
        return b.someOtherArray[row-1][col] * 2;
    });
    applyCallable<9, 10>(b.jF, [&b](int row, int col)
    {
        return b.someOtherArray[row][col-1] * 2;
    });
}

int main()
{
  myStruct S;
  fn(S);
}
