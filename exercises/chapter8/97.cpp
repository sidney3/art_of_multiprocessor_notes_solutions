#include <functional>
#include <iostream>
#include <thread>
#include <cassert>

struct IRooms
{
    void enter(size_t i) = delete;
    bool exit() = delete;
};

/*
   Just really beautiful code.
*/
struct Rooms : IRooms
{
    Rooms()
        : curr_state{ {NO_ACTIVE_ROOM, 0} }
    {}

    void enter(int i)
    {
        while(true)
        {
            room_state local_state = curr_state.load(std::memory_order_acquire);

            if(local_state.active_room == NO_ACTIVE_ROOM)
            {
                room_state proposed_state{i, 1};

                if(curr_state.compare_exchange_strong(local_state,
                            proposed_state))
                {
                    return;
                }
            }
            else if(local_state.active_room == i)
            {
                room_state proposed_state{local_state.active_room, 
                    local_state.room_count + 1};

                if(curr_state.compare_exchange_strong(
                            local_state, proposed_state
                            ))
                {
                    return;
                }

            }
        }
    }

    template<typename F>
        requires std::invocable<F>
    bool exit(F&& f)
    {
        while(true)
        {
            room_state local_state = curr_state.load(std::memory_order_acquire);

            if(local_state.room_count == 1)
            {
                assert(local_state.active_room != TRANSITIONING && local_state.active_room != NO_ACTIVE_ROOM);

                room_state proposed_state{TRANSITIONING, 0};

                // concern : somebody else enters the room while in this function
                if(curr_state.compare_exchange_strong(local_state, proposed_state))
                {
                    f();
                    curr_state.store({NO_ACTIVE_ROOM, 0});
                    return true;
                }
            }
            else
            {
                room_state proposed_state{local_state.active_room, local_state.room_count - 1};
                if(curr_state.compare_exchange_strong(local_state, proposed_state))
                {
                    return false;
                }
            }
        }

    }
private:
    static constexpr int TRANSITIONING = -2;
    static constexpr int NO_ACTIVE_ROOM = -1;
    struct room_state
    {
        int active_room = NO_ACTIVE_ROOM;
        int room_count = 0;
    };
    std::atomic<room_state> curr_state;
};

