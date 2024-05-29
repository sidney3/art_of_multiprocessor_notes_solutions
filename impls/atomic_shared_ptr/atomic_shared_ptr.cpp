#include <atomic>
#include <memory>

template<typename T>
struct atomic_shared_ptr
{
    atomic_shared_ptr(T* ptr)
        : _ptr{ptr}, 
        ref_count{ new std::atomic<long>{1}}
    {

    }

    bool compare_exchange_strong(
            std::shared_ptr<T>& e,
            std::shared_ptr<T> to_become,
            std::memory_order order = std::memory_order_acq_rel)
    {
        T* expected_ptr = e.get();
        T* to_become_ptr = to_become.get();
        bool res = _ptr.compare_exchange_strong(expected_ptr, to_become_ptr);
        e.reset(expected_ptr);

    }



    
    std::atomic<T*> _ptr;
    std::atomic<long>* ref_count;

};

int main()
{
    atomic_shared_ptr<int> ptr{new int{5}};
}
