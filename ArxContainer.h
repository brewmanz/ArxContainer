#pragma once

#ifndef ARX_RINGBUFFER_H
#define ARX_RINGBUFFER_H

#include "ArxContainer/has_include.h"
#include "ArxContainer/has_libstdcplusplus.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

// Make sure std namespace exists
namespace std { }

// Import everything from the std namespace into arx::std, so that
// anything we import rather than define is also available through
// arx::stdx.
// This includes everything yet to be defined, so we can do this early
// (and must do so, to allow e.g. the C++14 additions in the arx::std
// namespace to reference the C++11 stuff from the system headers.
namespace arx {
    namespace stdx {
        using namespace ::std;
    }
}

// Import everything from arx::std back into the normal std namespace.
// This ensures that you can just use `std::foo` everywhere and you get
// the standard library version if it is available, falling back to arx
// versions for things not supplied by the standard library. Only when
// you really need the arx version (e.g. for constexpr numeric_limits
// when also using ArduinoSTL), you need to qualify with arx::stdx::
namespace std {
    using namespace ::arx::stdx;
}

#include "ArxContainer/replace_minmax_macros.h"
#include "ArxContainer/initializer_list.h"

#if ARX_HAVE_LIBSTDCPLUSPLUS >= 201103L  // Have libstdc++11

#include <vector>
#include <array>
#include <deque>
#include <map>

#endif

#include <limits.h>

#ifndef ARX_VECTOR_DEFAULT_SIZE
#define ARX_VECTOR_DEFAULT_SIZE 16
#endif  // ARX_VECTOR_DEFAULT_SIZE

#ifndef ARX_DEQUE_DEFAULT_SIZE
#define ARX_DEQUE_DEFAULT_SIZE 16
#endif  // ARX_DEQUE_DEFAULT_SIZE

#ifndef ARX_MAP_DEFAULT_SIZE
#define ARX_MAP_DEFAULT_SIZE 16
#endif  // ARX_MAP_DEFAULT_SIZE

namespace arx {

namespace container {
    namespace detail {
        template <class T>
        inline T&& move(T& t) { return static_cast<T&&>(t); }
    }  // namespace detail
}  // namespace container

template <typename T, size_t N>
class RingBuffer;

template <typename T>
class RingBuffer_base {
  public:
    RingBuffer_base(size_t n)
    : m_N(n) {}
    inline size_t capacity() const { return m_N; }
  private:
    size_t m_N;
  public:
    class Iterator;
    class ConstIterator {
        friend class RingBuffer_base<T>;
        template <typename TT, size_t N>
        friend class RingBuffer;

        const T* ptr {nullptr};  // pointer to the first element
        int pos {0};
        size_t N {1};

        ConstIterator(const T* ptr, int pos, size_t n)
        : ptr(ptr), pos(pos), N(n) {}

    public:
        ConstIterator() {}
        ConstIterator(const ConstIterator& it) {
            this->ptr = it.ptr;
            this->pos = it.pos;
            this->N = it.N;
        }

        ConstIterator(ConstIterator&& it) {
            this->ptr = container::detail::move(it.ptr);
            this->pos = container::detail::move(it.pos);
            this->N = container::detail::move(it.N);
        }

        ConstIterator& operator=(const ConstIterator& rhs) {
            this->ptr = rhs.ptr;
            this->pos = rhs.pos;
            this->N = rhs.N;
            return *this;
        }
        ConstIterator& operator=(ConstIterator&& rhs) {
            this->ptr = container::detail::move(rhs.ptr);
            this->pos = container::detail::move(rhs.pos);
            this->N = container::detail::move(rhs.N);
            return *this;
        }

        // const-like conversion ConstIterator => Iterator
        Iterator to_iterator() const {
            return Iterator(this->ptr, this->pos, this->N);
        }

    protected:
        int pos_wrap_around(const int pos) const{
            if (pos >= 0)
                return pos % N;
            // // else
            return (N - 1) - (abs(pos + 1) % N);
        }

    public:
        int index() const {
            return pos_wrap_around(pos);
        }
        int index_with_offset(const int i) const {
            return pos_wrap_around(pos + i);
        }

        const T& operator*() const {
            return *(ptr + index());
        }
        const T* operator->() const {
            return ptr + index();
        }

        ConstIterator operator+(const int n) const {
            return ConstIterator(this->ptr, this->pos + n);
        }
        int operator-(const ConstIterator& rhs) const {
            return this->pos - rhs.pos;
        }
        ConstIterator operator-(const int n) const {
            return ConstIterator(this->ptr, this->pos - n);
        }
        ConstIterator& operator+=(const int n) {
            this->pos += n;
            return *this;
        }
        ConstIterator& operator-=(const int n) {
            this->pos -= n;
            return *this;
        }

        // prefix increment/decrement
        ConstIterator& operator++() {
            ++pos;
            return *this;
        }
        ConstIterator& operator--() {
            --pos;
            return *this;
        }
        // postfix increment/decrement
        ConstIterator operator++(int) {
            ConstIterator it = *this;
            ++pos;
            return it;
        }
        ConstIterator operator--(int) {
            ConstIterator it = *this;
            --pos;
            return it;
        }

        bool operator==(const ConstIterator& rhs) const {
            return (rhs.ptr == ptr) && (rhs.pos == pos) && (rhs.N == N);
        }
        bool operator!=(const ConstIterator& rhs) const {
            return !(*this == rhs);
        }
        bool operator<(const ConstIterator& rhs) const {
            return pos < rhs.pos;
        }
        bool operator<=(const ConstIterator& rhs) const {
            return pos <= rhs.pos;
        }
        bool operator>(const ConstIterator& rhs) const {
            return pos > rhs.pos;
        }
        bool operator>=(const ConstIterator& rhs) const {
            return pos >= rhs.pos;
        }

    private:
        int raw_pos() const {
            return pos;
        }

        void set(const int i) {
            pos = i;
        }

        void reset() {
            pos = 0;
        }
    };

    class Iterator : public ConstIterator {
        friend class RingBuffer_base<T>;
        template <typename TT, size_t N>
        friend class RingBuffer;

        Iterator(const T* ptr, int pos, size_t n) {
            this->ptr = ptr;
            this->pos = pos;
            this->N = n;
        }

    public:
        Iterator() = default;
        Iterator(const Iterator&) = default;
        Iterator(Iterator&&) = default;
        Iterator& operator=(const Iterator&) = default;
        Iterator& operator=(Iterator&&) = default;

        T& operator*() {
            return *(const_cast<T*>(this->ptr) + this->index());
        }
        T* operator->() {
            return const_cast<T*>(this->ptr) + this->index();
        }

        // all inherited methods that return ConstIterator must be reimplemented
        Iterator operator+(const int n) const {
            return Iterator(this->ptr, this->pos + n, this->N);
        }
        Iterator operator-(const int n) const {
            return Iterator(this->ptr, this->pos - n, this->N);
        }
        Iterator& operator+=(const int n) {
            this->pos += n;
            return *this;
        }
        Iterator& operator-=(const int n) {
            this->pos -= n;
            return *this;
        }

        // prefix increment/decrement
        Iterator& operator++() {
            ++(this->pos);
            return *this;
        }
        Iterator& operator--() {
            --(this->pos);
            return *this;
        }
        // postfix increment/decrement
        Iterator operator++(int) {
            Iterator it = *this;
            ++(this->pos);
            return it;
        }
        Iterator operator--(int) {
            Iterator it = *this;
            --(this->pos);
            return it;
        }
    };
    using iterator = Iterator;
    using const_iterator = ConstIterator;
    //using iterator = typename RingBuffer_base<T>::Iterator;
    //using const_iterator = typename RingBuffer_base<T>::ConstIterator;
};
template <typename T, size_t N>
class RingBuffer : public RingBuffer_base<T>{
protected:
    friend class RingBuffer_base<T>::Iterator;
    friend class RingBuffer_base<T>::ConstIterator;

    T queue_[N];
    int head_;
    int tail_;

public:
    //using Iterator = typename RingBuffer_base<T>::Iterator;
    //using ConstIterator = typename RingBuffer_base<T>::ConstIterator;
    //using iterator = typename RingBuffer_base<T>::Iterator;
    //using const_iterator = typename RingBuffer_base<T>::ConstIterator;

    RingBuffer()
    : RingBuffer_base<T>(N)
    , queue_()
    , head_(0)
    , tail_(0) {
    }

    RingBuffer(std::initializer_list<T> lst)
    : RingBuffer_base<T>(N)
    , queue_()
    , head_(0)
    , tail_(0) {
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            push_back(*it);
        }
    }

    // copy
    explicit RingBuffer(const RingBuffer& r)
    : RingBuffer_base<T>(N)
    , queue_()
    , head_(r.head_)
    , tail_(r.tail_) {
        typename RingBuffer_base<T>::const_iterator it = r.begin();
        for (size_t i = 0; i < r.size(); ++i) {
            int pos = it.index_with_offset(i);
            queue_[pos] = r.queue_[pos];
        }
    }
    RingBuffer& operator=(const RingBuffer& r) {
        head_ = r.head_;
        tail_ = r.tail_;
        typename RingBuffer_base<T>::const_iterator it = r.begin();
        for (size_t i = 0; i < r.size(); ++i) {
            int pos = it.index_with_offset(i);
            queue_[pos] = r.queue_[pos];
        }
        return *this;
    }

    // move
    RingBuffer(RingBuffer&& r)
    : RingBuffer_base<T>(N) {
        head_ = container::detail::move(r.head_);
        tail_ = container::detail::move(r.tail_);
        typename RingBuffer_base<T>::const_iterator it = r.begin();
        for (size_t i = 0; i < r.size(); ++i) {
            int pos = it.index_with_offset(i);
            queue_[pos] = container::detail::move(r.queue_[pos]);
        }
    }

    RingBuffer& operator=(RingBuffer&& r) {
        head_ = container::detail::move(r.head_);
        tail_ = container::detail::move(r.tail_);
        typename RingBuffer_base<T>::const_iterator it = r.begin();
        for (size_t i = 0; i < r.size(); ++i) {
            int pos = it.index_with_offset(i);
            queue_[pos] = container::detail::move(r.queue_[pos]);
        }
        return *this;
    }

    //override size_t capacity() const { return N; };
    size_t size() const { return tail_ - head_; }
    // data() method better not to use :-(
    // it should point to the 1st item and have enough space for size() readings of items
    // impossible with ringbuffer - either points to the 1st item or has enough space
    // only exception when it works is when head_ pos == 0
    const T* data() const { return reinterpret_cast<const T*>(&(queue_)); }
    T* data() { return reinterpret_cast<T*>(&(queue_)); }
    bool empty() const { return tail_ == head_; }
    void clear() { head_ = tail_ = 0; }

    void pop() {
        pop_front();
    }
    void pop_front() {
        if (size() == 0) return;
        if (size() == 1)
            clear();
        else
            increment_head();
    }
    void pop_back() {
        if (size() == 0) return;
        if (size() == 1)
            clear();
        else
            decrement_tail();
    }

    void push(const T& data) {
        push_back(data);
    }
    void push(T&& data) {
        push_back(data);
    }
    void push_back(const T& data) {
        get(size()) = data;
        increment_tail();
    }
    void push_back(T&& data) {
        get(size()) = data;
        increment_tail();
    }
    void push_front(const T& data) {
        decrement_head();
        get(0) = data;
    }
    void push_front(T&& data) {
        decrement_head();
        get(0) = data;
    }
    void emplace(const T& data) { push(data); }
    void emplace(T&& data) { push(data); }
    void emplace_back(const T& data) { push_back(data); }
    void emplace_back(T&& data) { push_back(data); }

    const T& front() const { return get(0); }
    T& front() { return get(0); }

    const T& back() const { return get(static_cast<int>(size()) - 1); }
    T& back() { return get(static_cast<int>(size()) - 1); }

    const T& operator[](size_t index) const { return get(static_cast<int>(index)); }
    T& operator[](size_t index) { return get(static_cast<int>(index)); }

    typename RingBuffer_base<T>::iterator begin() { return empty() ? typename RingBuffer_base<T>::Iterator() : typename RingBuffer_base<T>::Iterator(queue_, head_, N); }
    typename RingBuffer_base<T>::iterator end() { return empty() ? typename RingBuffer_base<T>::Iterator() : typename RingBuffer_base<T>::Iterator(queue_, tail_, N); }
    typename RingBuffer_base<T>::const_iterator begin() const { return empty() ? typename RingBuffer_base<T>::ConstIterator() : typename RingBuffer_base<T>::ConstIterator(queue_, head_, N); }
    typename RingBuffer_base<T>::const_iterator end() const { return empty() ? typename RingBuffer_base<T>::ConstIterator() : typename RingBuffer_base<T>::ConstIterator(queue_, tail_, N); }

    // https://en.cppreference.com/w/cpp/container/vector/erase
    typename RingBuffer_base<T>::iterator erase(const typename RingBuffer_base<T>::const_iterator& p) {
        if (!is_valid(p)) return end();

        typename RingBuffer_base<T>::iterator it_last = end() - 1;
        for (typename RingBuffer_base<T>::iterator it = p.to_iterator(); it != it_last; ++it)
            *it = *(it + 1);
        *it_last = T();
        decrement_tail();
        return empty() ? end() : p.to_iterator();
    }

    void resize(size_t sz) {
        size_t s = size();
        if (sz > s) {
            for (size_t i = 0; i < sz - s; ++i) push(T());
        } else if (sz < s) {
            for (size_t i = 0; i < s - sz; ++i) pop();
        }
    }

    void assign(typename RingBuffer_base<T>::const_iterator first, typename RingBuffer_base<T>::const_iterator end) {
        clear();
        while (first != end) push(*(first++));
    }

    void assign(const T* first, const T* end) {
        clear();
        while (first != end) push(*(first++));
    }

    void shrink_to_fit() {
        // dummy
    }

    void reserve(size_t n) {
        (void)n;
        // dummy
    }

    void fill(const T& v) {
        for (typename RingBuffer_base<T>::iterator it = begin(); it != end(); ++it)
            *it = v;
    }

    // https://en.cppreference.com/w/cpp/container/vector/insert
    void insert(const typename RingBuffer_base<T>::const_iterator& pos, const typename RingBuffer_base<T>::const_iterator& first, const typename RingBuffer_base<T>::const_iterator& last) {
        if (!is_valid(pos) && pos != end())
            return;

        size_t sz = last - first;

        size_t new_sz = size() + sz;
        if (new_sz > RingBuffer_base<T>::capacity())
            new_sz = RingBuffer_base<T>::capacity();

        typename RingBuffer_base<T>::iterator it = begin() + new_sz - 1;
        while (it != pos) {
            *it = *(it - sz);
            --it;
        }
        for (size_t i = 0; i < sz; ++i) {
            *(it + i) = *(first + i);
            if (size() < RingBuffer_base<T>::capacity() || (it + i) == end())
                increment_tail();
        }
    }

    void insert(const typename RingBuffer_base<T>::const_iterator& pos, const T* first, const T* last) {
        if (!is_valid(pos) && pos != end())
            return;

        size_t sz = last - first;

        size_t new_sz = size() + sz;
        if (new_sz > RingBuffer_base<T>::capacity())
            new_sz = RingBuffer_base<T>::capacity();

        typename RingBuffer_base<T>::iterator it = begin() + new_sz - 1;
        while (it != pos) {
            *it = *(it - sz);
            --it;
        }
        for (size_t i = 0; i < sz; ++i) {
            *(it + i) = *(first + i);
            if (size() < RingBuffer_base<T>::capacity() || (it + i) == end())
                increment_tail();
        }
    }

    void insert(const typename RingBuffer_base<T>::const_iterator& pos, const T& val) {
        const T* ptr = &val;
        insert(pos, ptr, ptr + 1);
    }

private:
    T& get(const typename RingBuffer_base<T>::iterator& it) {
        return queue_[it.index()];
    }
    const T& get(const typename RingBuffer_base<T>::const_iterator& it) const {
        return queue_[it.index()];
    }
    T& get(const int index) {
        return queue_[begin().index_with_offset(index)];
    }
    const T& get(const int index) const {
        return queue_[begin().index_with_offset(index)];
    }

    T* ptr(const typename RingBuffer_base<T>::iterator& it) {
        return queue_ + it.index();
    }
    const T* ptr(const typename RingBuffer_base<T>::const_iterator& it) const {
        return queue_ + it.index();
    }
    T* ptr(const int index) {
        return queue_ + begin().index_with_offset(index);
    }
    const T* ptr(const int index) const {
        return queue_ + begin().index_with_offset(index);
    }

    void increment_head() {
        ++head_;
        resolve_overflow();
    }
    void increment_tail() {
        ++tail_;
        resolve_overflow();
        if (size() > N)
            increment_head();
    }
    void decrement_head() {
        --head_;
        resolve_overflow();
        if (size() > N)
            decrement_tail();
    }
    void decrement_tail() {
        --tail_;
        resolve_overflow();
    }

    void resolve_overflow() {
        if (empty())
            clear();
        else if (head_ <= (static_cast<int>(INT_MIN) + static_cast<int>(RingBuffer_base<T>::capacity())) \
                || tail_ >= (static_cast<int>(INT_MAX) - static_cast<int>(RingBuffer_base<T>::capacity()))) {
            // +/- capacity(): reserve some space for pointer/iterator arithmetics
            // head_/tail_ pointers are re-set N+1 steps before the overflow occurs
            int len = size();
            head_ = begin().index();
            tail_ = head_ + len;
        }
    }

    bool is_valid(const typename RingBuffer_base<T>::const_iterator& it) const {
        if (it.ptr != queue_)
            return false; // iterator to a different object
        return (it.raw_pos() >= head_) && (it.raw_pos() < tail_);
    }
};
} // namespace arx

template <typename T, size_t N>
inline bool operator==(const arx::RingBuffer<T, N>& x, const arx::RingBuffer<T, N>& y) {
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i)
        if (x[i] != y[i]) return false;
    return true;
}

template <typename T, size_t N>
inline bool operator!=(const arx::RingBuffer<T, N>& x, const arx::RingBuffer<T, N>& y) {
    return !(x == y);
}

namespace arx {
namespace stdx {

template <typename T, size_t N = ARX_VECTOR_DEFAULT_SIZE>
struct vector : public RingBuffer<T, N> {
    //using iterator = typename RingBuffer_base<T>::iterator;
    //using const_iterator = typename RingBuffer_base<T>::const_iterator;

    vector()
    : RingBuffer<T, N>() {}
    vector(std::initializer_list<T> lst)
    : RingBuffer<T, N>(lst) {}

    // copy
    vector(const vector& r)
    : RingBuffer<T, N>(r) {}

    vector& operator=(const vector& r) {
        RingBuffer<T, N>::operator=(r);
        return *this;
    }

    // move
    vector(vector&& r)
    : RingBuffer<T, N>(r) {}

    vector& operator=(vector&& r) {
        RingBuffer<T, N>::operator=(r);
        return *this;
    }

private:
    using RingBuffer<T, N>::pop;
    using RingBuffer<T, N>::pop_front;
    using RingBuffer<T, N>::push;
    using RingBuffer<T, N>::push_front;
    using RingBuffer<T, N>::emplace;
    using RingBuffer<T, N>::fill;
};

} // namespace arx
} // namespace stdx

namespace arx {
namespace stdx {

template <typename T, size_t N>
struct array : public RingBuffer<T, N> {
    //using iterator = typename RingBuffer_base<T>::iterator;
    //using const_iterator = typename RingBuffer_base<T>::const_iterator;

    array()
    : RingBuffer<T, N>() {}
    array(std::initializer_list<T> lst)
    : RingBuffer<T, N>(lst) {}

    // copy
    array(const array& r)
    : RingBuffer<T, N>(r) {}

    array& operator=(const array& r) {
        RingBuffer<T, N>::operator=(r);
        return *this;
    }

    // move
    array(array&& r)
    : RingBuffer<T, N>(r) {}

    array& operator=(array&& r) {
        RingBuffer<T, N>::operator=(r);
        return *this;
    }

private:
    using RingBuffer<T, N>::pop;
    using RingBuffer<T, N>::pop_front;
    using RingBuffer<T, N>::push;
    using RingBuffer<T, N>::push_front;
    using RingBuffer<T, N>::emplace;
};

} // namespace arx
} // namespace stdx

namespace arx {
namespace stdx {

template <typename T, size_t N = ARX_DEQUE_DEFAULT_SIZE>
struct deque : public RingBuffer<T, N> {
    //using iterator = typename RingBuffer_base<T>::iterator;
    //using const_iterator = typename RingBuffer_base<T>::const_iterator;

    deque()
    : RingBuffer<T, N>() {}
    deque(std::initializer_list<T> lst)
    : RingBuffer<T, N>(lst) {}

    // copy
    deque(const deque& r)
    : RingBuffer<T, N>(r) {}

    deque& operator=(const deque& r) {
        RingBuffer<T, N>::operator=(r);
        return *this;
    }

    // move
    deque(deque&& r)
    : RingBuffer<T, N>(r) {}

    deque& operator=(deque&& r) {
        RingBuffer<T, N>::operator=(r);
        return *this;
    }

private:
    using RingBuffer<T, N>::capacity;
    using RingBuffer<T, N>::pop;
    using RingBuffer<T, N>::push;
    using RingBuffer<T, N>::fill;
};
} // namespace arx
} // namespace stdx

namespace arx {
namespace stdx {

template <typename T1, typename T2>
struct pair {
    T1 first;
    T2 second;
};

template <typename T1, typename T2>
inline pair<T1, T2> make_pair(const T1& t1, const T2& t2) {
    return {t1, t2};
};

} // namespace arx
} // namespace stdx

template <typename T1, typename T2>
inline bool operator==(const arx::stdx::pair<T1, T2>& x, const arx::stdx::pair<T1, T2>& y) {
    return (x.first == y.first) && (x.second == y.second);
}

template <typename T1, typename T2>
inline bool operator!=(const arx::stdx::pair<T1, T2>& x, const arx::stdx::pair<T1, T2>& y) {
    return !(x == y);
}

namespace arx {
namespace stdx {

template <typename Key, class T, size_t N = ARX_MAP_DEFAULT_SIZE>
struct map : public RingBuffer<pair<Key, T>, N> {
    using base = RingBuffer<pair<Key, T>, N>;
    //using iterator = typename RingBuffer_base<T>::iterator;
    //using const_iterator = typename RingBuffer_base<T>::const_iterator;

    map()
    : base() {}
    map(std::initializer_list<pair<Key, T> > lst)
    : base(lst) {}

    map(typename RingBuffer_base<pair<Key, T> >::iterator iBegin, typename RingBuffer_base<pair<Key, T> >::iterator iEnd)
    : base() {
        for (auto it = iBegin; it != iEnd; ++it) {
            push_back(*it);
        }
    }


    // copy
    map(const map& r)
    : base(r) {}

    map& operator=(const map& r) {
        base::operator=(r);
        return *this;
    }

    // move
    map(map&& r)
    : base(r) {}

    map& operator=(map&& r) {
        base::operator=(r);
        return *this;
    }

    typename RingBuffer_base<pair<Key, T> >::const_iterator find(const Key& key) const {
        for (typename RingBuffer_base<pair<Key, T> >::const_iterator it = this->begin(); it != this->end(); ++it) {
            if (key == it->first)
                return it;
        }
        return this->end();
    }

    typename RingBuffer_base<pair<Key, T> >::iterator find(const Key& key) {
        for (typename RingBuffer_base<pair<Key, T> >::iterator it = this->begin(); it != this->end(); ++it) {
            if (key == it->first)
                return it;
        }
        return this->end();
    }

    pair<typename RingBuffer_base<pair<Key, T> >::iterator, bool> insert(const Key& key, const T& t) {
        return insert(::arx::stdx::make_pair(key, t));
    }

    pair<typename RingBuffer_base<pair<Key, T> >::iterator, bool> insert(const pair<Key, T>& p) {
        bool b {false};
        typename RingBuffer_base<T>::iterator it = find(p.first);
        if (it == this->end()) {
            this->push(p);
            b = true;
            it = this->begin() + this->size() - 1;
        }
        return {it, b};
    }

    pair<typename RingBuffer_base<pair<Key, T> >::iterator, bool> emplace(const Key& key, const T& t) {
        return insert(key, t);
    }

    pair<typename RingBuffer_base<pair<Key, T> >::iterator, bool> emplace(const pair<Key, T>& p) {
        return insert(p);
    }

private:
    T& empty_value() const {
        static T val;
        val = T(); // fresh empty value every time
        return val;
    }
public:
    const T& at(const Key& key) const {
        typename RingBuffer_base<pair<Key, T> >::const_iterator it = find(key);
        if (it != this->end()) return it->second;
        return empty_value();
        //return find(key)->second;
    }

    T& at(const Key& key) {
        typename RingBuffer_base<pair<Key, T> >::iterator it = find(key);
        if (it != this->end()) return it->second;
        return empty_value();
        //return find(key)->second;
    }

    typename RingBuffer_base<T>::iterator erase(const typename RingBuffer_base<T>::const_iterator& it) {
        typename RingBuffer_base<pair<Key, T> >::iterator i = find(it->first);
        return base::erase(i);
    }

    typename RingBuffer_base<T>::iterator erase(const Key& key) {
        typename RingBuffer_base<pair<Key, T> >::iterator i = find(key);
        return base::erase(i);
    }

    // erase() will cause compile error if map's Key is 'unsigned int'
    // => collision of this method with erase(const Key&)
    typename RingBuffer_base<pair<Key, T> >::iterator erase(const size_t index) {
        if (index < this->size()) {
            typename RingBuffer_base<T>::iterator it = this->begin() + index;
            return erase(it);
        }
        return this->end();
    }

    T& operator[](const Key& key) {
        typename RingBuffer_base<T>::iterator it = find(key);
        if (it != this->end()) return it->second;

        insert(::arx::stdx::make_pair(key, T()));
        return this->back().second;
    }

private:
    using base::assign;
    using base::back;
    using base::capacity;
    using base::data;
    using base::emplace_back;
    using base::front;
    using base::pop;
    using base::pop_back;
    using base::pop_front;
    using base::push;
    using base::push_back;
    using base::push_front;
    using base::resize;
    using base::shrink_to_fit;
    using base::fill;
};

} //  namespace stdx
} // namespace arx

template <typename T, size_t N>
using ArxRingBuffer = arx::RingBuffer<T, N>;

#endif  // ARX_RINGBUFFER_H
