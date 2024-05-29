#include <mutex>
#include <optional>

template<typename T>
struct SynchronousQueue
{
    std::optional<T> _item;
    bool enqueuing;
    std::condition_variable cv;
    std::mutex lock;

    void enq(T item)
    {
        std::unique_lock<std::mutex> lk{lock};

        cv.wait(lk, [this](){return !enqueuing;});

        enqueuing = true;
        _item = item;
        cv.notify_all();

        cv.wait(lk, [this]{return !_item.has_value();});
        enqueuing = false;
        cv.notify_all();
    }

    T deq(void)
    {
        std::unique_lock<std::mutex> lk{lock};

        cv.wait(lk, [this](){return _item.has_value();});

        T res = *_item;
        _item = std::nullopt;
        cv.notify_all();
        return res;
    }
};

int main()
{

}
