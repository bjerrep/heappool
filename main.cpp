#include "heappool.h"
#include <vector>


template<int N>
struct testClassA
{
    testClassA()
    {
        for(int i=0; i<N; i++)
        {
            data[i] = i;
        }
    }
    char data[N];
};


void test_1()
{
    #define N 12
    #define ALLOCS 12

    std::vector<testClassA<N>*> v;
    v.reserve(ALLOCS);
    for (int i=0; i<ALLOCS; i++)
    {
        auto* scp = new testClassA<N>();
        v.push_back(scp);
    }
    hp->print_allocation_table();

    for (auto scp : v)
    {
        delete scp;
    }

    hp->print_allocation_table();
}


int main()
{
    log("miniheap demo\n");

    const int NOF_POOLS = 2;

    int my_partitions[NOF_POOLS][2] = {
        {16, 10},
        {32, 5}
    };

    hp = new HeapPool(NOF_POOLS, *my_partitions);

    test_1();

    return 0;
}
