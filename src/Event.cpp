
#include "Event.hpp"


template <size_t Capacity>
bool EventQueue<Capacity>::push(const Event &event)
{
    if (isFull()) {
        return false;
    }

    buffer_[tail_] = event;
    tail_ = nextIndex(tail_);
    count_++;

    return true;
}

template <size_t Capacity>
bool EventQueue<Capacity>::pop(Event &event)
{
    if (isEmpty()) {
        return false;
    }

    event = buffer_[head_];
    head_ = nextIndex(head_);
    count_--;

    return true;
}

template <size_t Capacity>
bool EventQueue<Capacity>::peek(Event &event) const
{
    if (isEmpty()) {
        return false;
    }

    event = buffer_[head_];
    return true;
}

template <size_t Capacity>
bool EventQueue<Capacity>::isEmpty() const
{
    return count_ == 0;
}

template <size_t Capacity>
bool EventQueue<Capacity>::isFull() const
{
    return count_ == Capacity;
}

template <size_t Capacity>
size_t EventQueue<Capacity>::size() const
{
    return count_;
}

template <size_t Capacity>
constexpr size_t EventQueue<Capacity>::capacity() const
{
    return Capacity;
}

template <size_t Capacity>
void EventQueue<Capacity>::clear()
{
    head_ = 0;
    tail_ = 0;
    count_ = 0;
}

template <size_t Capacity>
constexpr size_t EventQueue<Capacity>::nextIndex(size_t index)
{
    return (index + 1) % Capacity;
}
