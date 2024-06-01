#include "mrmw_queue.h"
#include "../../exercises/chapter10/test_pool.h"

void single_threaded_test()
{
    MRMWQueue<int> q(1024);

    q.force_enq(5);
    q.force_enq(6);

    assert(q.force_deq() == 5);
    assert(q.force_deq() == 6);
    assert(!q.deq().has_value());
}

int main()
{
    MRMWQueue<int> q(1024);
    test_pool(q);

    single_threaded_test();
}
