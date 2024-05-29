#include <unordered_map>
#include "ThreadLocalDynamic.h"
#include "../StampedAllocator/StampedAllocator.h"


namespace sidney3
{
template<
    typename BackupAllocator, 
    typename DefaultAllocator>
struct FailureAllocator : DefaultAllocator
{
    using pointer = std::allocator_traits<BackupAllocator>::pointer;
    static_assert(std::is_same_v<
            pointer, 
            typename std::allocator_traits<DefaultAllocator>::pointer>);

    FailureAllocator(DefaultAllocator *failureAlloc)
        : failureAlloc_{failureAlloc}
    {}

    pointer allocate(const size_t count)
    {
        pointer _ptr = DefaultAllocator::allocate(count);

        if(!_ptr)
        {
            _ptr = failureAlloc_->allocate(count);
            assert(_ptr 
                    && "Backup allocator must succeed at allocation");
        }

        return _ptr;
    }

    void deallocate(pointer ptr, size_t count)
    {
        if(!DefaultAllocator::deallocate(ptr, count))
        {
            bool backupSuccess = failureAlloc_->deallocate(ptr, count);
            assert(backupSuccess 
                    && "backup allocator must succeed at deallocation");
        }
    }


private:
    BackupAllocator *failureAlloc_;
};


template<
    typename T,
    typename PrincipleAlloc,
    typename ThreadLocalAlloc
    >
struct MultithreadAlloc
{
private:
    PrincipleAlloc mainAllocator;
    ThreadLocal<
        MultithreadAlloc, FailureAllocator<PrincipleAlloc, ThreadLocalAlloc>
        > threadLocalAllocator;
};

} // namespace sidney3
