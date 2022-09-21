#pragma once

#include <cassert>
#include <type_traits>
#include <utility>
#include <iterator>

namespace intrusive {
struct default_tag;

// marker for non-connected node: prev == next == nullptr
struct list_base {
  list_base* prev{nullptr};
  list_base* next{nullptr};

  list_base() = default;

  list_base(list_base&& other) : prev{other.prev}, next{other.next} {
    if (other.prev != nullptr && other.next != nullptr) {
      prev->next = this;
      next->prev = this;
      other.prev = other.next = nullptr;
    }
  }

  list_base(const list_base&) {}  // not default!

  list_base& operator=(list_base&& other) {
    if (this == &other) {
      return *this;
    }
    prev = other.prev;
    next = other.next;
    other.prev = other.next = nullptr;
    if (other.prev != nullptr && other.next != nullptr) {
      prev->next = this;
      next->prev = this;
    }
    return *this;
  }

  list_base& operator=(const list_base& other) {
    return *this;
  }

  void unlink() {
    if (prev == nullptr && next == nullptr) {
      return;
    }
    prev->next = next;
    next->prev = prev;
    prev = next = nullptr;
  }

  /// Insert `other` before this element
  void insert(list_base& other) {
    assert(prev != nullptr && next != nullptr);
    if (this == &other) {
      // We don't want to insert the element before itself
      return;
    }
    other.unlink();

    prev->next = &other;
    other.prev = this->prev;

    other.next = this;
    this->prev = &other;
  }

  ~list_base() {
    unlink();
  }
};

template<typename Tag = default_tag>
struct list_element : public list_base {};

template <typename T, typename Tag = default_tag>
struct list {
  static_assert(std::is_base_of_v<list_element<Tag>, T>, "T should derive from list_element<Tag>");

  list() {
    sentinel.prev = &sentinel;
    sentinel.next = &sentinel;
  }

  list(list&& other) {
    if (other.empty()) {
      sentinel.prev = sentinel.next = &sentinel;
    } else {
      sentinel.prev = other.sentinel.prev;
      sentinel.next = other.sentinel.next;
    }
    other.sentinel.prev = other.sentinel.next = &other.sentinel;
    sentinel.prev->next = sentinel.next->prev = &sentinel;
    assert(other.empty());

    // The moved-from list is now empty and in valid state
  }

  list& operator=(list&& other) {
    // TODO: swap-trick
    if (this == &other) {
      return *this;
    }
    clear();
    if (other.empty()) {
      return *this;
    }
    sentinel.next = other.sentinel.next;
    sentinel.prev = other.sentinel.prev;

    sentinel.next->prev = &sentinel;
    sentinel.prev->next = &sentinel;

    other.sentinel.prev = &other.sentinel;
    other.sentinel.next = &other.sentinel;

    return *this;
  }

  template<bool Const>
  struct generic_iterator;

  using iterator = generic_iterator<false>;
  using const_iterator = generic_iterator<true>;

  template<bool Const>
  struct generic_iterator {
    using value_type = std::conditional_t<Const, const T, T>;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
    using pointer = value_type*;
    using reference = value_type&;

    generic_iterator() : data{nullptr} {}

    generic_iterator(const iterator& iter) : data{iter.data} {}

    template<bool Dummy = Const, typename = std::enable_if_t<Dummy>>
    generic_iterator(const const_iterator& iter) : data{iter.data} {}

    generic_iterator& operator=(const iterator& other) {
      this->data = other.data;
      return *this;
    }

    template<bool Dummy = Const, typename = std::enable_if_t<Dummy>>
    generic_iterator& operator=(const const_iterator& other) {
      this->data = other.data;
      return *this;
    }

    pointer operator->() const {
      return static_cast<value_type*>(static_cast<list_element<Tag>*>(data));
    }

    reference operator*() const {
      return static_cast<value_type&>(static_cast<list_element<Tag>&>(*data));
    }

    // TODO: using operator++; ???
    generic_iterator& operator++() {
      data = data->next;
      return *this;
    }

    generic_iterator operator++(int) {
      generic_iterator result = *this;
      ++*this;
      return result;
    }

    generic_iterator& operator--() {
      data = data->prev;
      return *this;
    }

    generic_iterator operator--(int) {
      generic_iterator result = *this;
      --*this;
      return result;
    }

    template<bool Const2>
    bool operator==(const generic_iterator<Const2>& rhs) const {
      return data == rhs.data;
    }

    template<bool Const2>
    bool operator!=(const generic_iterator<Const2>& rhs) const {
      return data != rhs.data;
    }

  private:
    explicit generic_iterator(list_base *data_) : data{data_} {};
    friend list<T, Tag>;

    list_base* data;
  };

  void push_back(T& val) noexcept {
    sentinel.insert(static_cast<list_element<Tag>&>(val));
  }

  void push_front(T& val) noexcept {
    // works even when the list is empty
    sentinel.next->insert(val);
  }

  void clear() noexcept {
    while (!empty()) {
      pop_back();
    }
  }

  void pop_back() noexcept {
    assert(!empty());
    sentinel.prev->unlink();
  }

  void pop_front() noexcept {
    assert(!empty());
    sentinel.next->unlink();
  }

  const T& back() const noexcept {
    assert(!empty());
    return static_cast<T&>(static_cast<list_element<Tag>&>(*sentinel.prev));
  }

  T& back() noexcept {
    assert(!empty());
    return static_cast<T&>(static_cast<list_element<Tag>&>(*sentinel.prev));
  }

  const T& front() const noexcept {
    assert(!empty());
    return static_cast<T&>(static_cast<list_element<Tag>&>(*sentinel.next));
  }

  T& front() noexcept {
    assert(!empty());
    return static_cast<T&>(static_cast<list_element<Tag>&>(*sentinel.next));
  }

  bool empty() const noexcept {
    return sentinel.prev == &sentinel && sentinel.next == &sentinel;
  }

  iterator begin() noexcept {
    return iterator{sentinel.next};
  }

  const_iterator begin() const noexcept {
    return const_iterator{sentinel.next};
  }

  iterator end() noexcept {
    return iterator{&sentinel};
  }

  const_iterator end() const noexcept {
    // NOTE: this is a hack to counter the constness of sentinel
    return const_iterator{sentinel.prev->next};
  }

  iterator insert(const_iterator it, T& val) noexcept {
    // if we want to insert the element before itself, the list_base::insert
    // will already deal with this
    it.data->insert(val);
    return iterator{static_cast<list_base*>(&val)};
  }

  iterator erase(iterator it) noexcept {
    iterator old = it++;
    old.data->unlink();
    return it;
  }

  void splice(const_iterator pos, list& other, const_iterator first, const_iterator last) {
    if (this == &other) {
      // TODO
    }
    if (first == last) {
      return;
    }
    --last;
    first.data->prev->next = last.data->next;
    last.data->next->prev = first.data->prev;

    first.data->prev = pos.data->prev;
    pos.data->prev->next = first.data;

    last.data->next = pos.data;
    pos.data->prev = last.data;

    // do {
    //   // TODO: this can be done in O(1)
    //   pos.data->insert(*first.data);
    // } while (first != last);
  }

  ~list() {
    clear();
  }

  // What to do if we want to insert an element that already is in the list
  // (ours or the other)?
  // The only special case is when we try to insert a node before itself
  // (+kostil "if (val == iter->...) -> noop")
  list_element<Tag> sentinel;
};

// const_iterator -> don't modify the pointed-to element
// iterator -> we can access and modify the element
// The problem is the copypaste -> use sfinae

} // namespace intrusive
