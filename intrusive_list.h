#pragma once

#include <cassert>
#include <iterator>
#include <type_traits>
#include <utility>

namespace intrusive {
struct default_tag;

struct list_base {
  // NOTE: marker for non-connected node: prev == next == nullptr
  list_base* prev{nullptr};
  list_base* next{nullptr};

  list_base();
  list_base(list_base&& other);
  list_base(const list_base&);
  list_base& operator=(list_base&& other);
  list_base& operator=(const list_base& other);

  void unlink();
  void insert(list_base& other);
  ~list_base();
};

template <typename Tag = default_tag>
struct list_element : public list_base {};

template <typename T, typename Tag = default_tag>
struct list {
  static_assert(std::is_base_of_v<list_element<Tag>, T>,
                "T should derive from list_element<Tag>");

  list() {
    sentinel.prev = &sentinel;
    sentinel.next = &sentinel;
  }

  list(list&& other) : list{} {
    *this = std::move(other);
  }

  list& operator=(list&& other) {
    if (this == &other) {
      return *this;
    }
    clear();
    if (other.empty()) {
      return *this;
    }
    sentinel.prev = other.sentinel.prev;
    sentinel.next = other.sentinel.next;

    other.sentinel.prev = &other.sentinel;
    other.sentinel.next = &other.sentinel;

    sentinel.next->prev = &sentinel;
    sentinel.prev->next = &sentinel;

    assert(other.empty());

    return *this;
  }

  template <bool Const>
  struct generic_iterator;

  using iterator = generic_iterator<false>;
  using const_iterator = generic_iterator<true>;

  template <bool Const>
  struct generic_iterator {
    using value_type = std::conditional_t<Const, const T, T>;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
    using pointer = value_type*;
    using reference = value_type&;

    generic_iterator() = default;

    generic_iterator(const iterator& iter) : data{iter.data} {}

    template <bool Dummy = Const, typename = std::enable_if_t<Dummy>>
    generic_iterator(const const_iterator& iter) : data{iter.data} {}

    generic_iterator& operator=(const iterator& other) {
      this->data = other.data;
      return *this;
    }

    template <bool Dummy = Const, typename = std::enable_if_t<Dummy>>
    generic_iterator& operator=(const const_iterator& other) {
      this->data = other.data;
      return *this;
    }

    pointer operator->() const {
      return static_cast<pointer>(static_cast<list_element<Tag>*>(data));
    }

    reference operator*() const {
      return static_cast<reference>(static_cast<list_element<Tag>&>(*data));
    }

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

    template <bool ConstRhs>
    bool operator==(const generic_iterator<ConstRhs>& rhs) const {
      return data == rhs.data;
    }

    template <bool ConstRhs>
    bool operator!=(const generic_iterator<ConstRhs>& rhs) const {
      return data != rhs.data;
    }

  private:
    explicit generic_iterator(list_base* data_) : data{data_} {};
    friend list<T, Tag>;

    list_base* data{nullptr};
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
    return const_iterator{const_cast<list_element<Tag>*>(&sentinel)};
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

  void splice(const_iterator pos, list& other, const_iterator first,
              const_iterator last) {
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
  }

  ~list() {
    clear();
  }

  list_element<Tag> sentinel;
};

} // namespace intrusive
