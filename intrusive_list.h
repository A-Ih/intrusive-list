#pragma once

#include <cassert>
#include <iterator>
#include <type_traits>
#include <utility>

namespace intrusive {
struct default_tag;

template <typename T, typename Tag>
struct list;

namespace detail {

struct list_base {
  // NOTE: marker for non-connected/sentinel node: prev == next == this
  list_base();
  list_base(list_base&& other);
  list_base(const list_base&);
  list_base& operator=(list_base&& other);
  list_base& operator=(const list_base&) = delete;
  ~list_base();

private:
  /// Returns true if node is not contained in any list or is a sentinel
  bool is_single() const;
  /// Remove node from a list (has no effect if a node is single);
  void unlink();
  /// Insert `other` before this element
  void insert(list_base& other);

  template <typename T, typename Tag>
  friend struct ::intrusive::list;

  list_base* prev;
  list_base* next;
};

} // namespace detail

template <typename Tag = default_tag>
struct list_element : public detail::list_base {};

template <typename T, typename Tag = default_tag>
struct list {
  static_assert(std::is_base_of_v<list_element<Tag>, T>,
                "T should derive from list_element<Tag>");

  list() = default;
  list(list&& other) = default;

  list& operator=(list&& other) {
    if (this == &other) {
      return *this;
    }
    clear();
    sentinel = std::move(other.sentinel);
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
    generic_iterator(const generic_iterator& iter) = default;

    template <bool Dummy = Const, typename = std::enable_if_t<Dummy>>
    generic_iterator(const iterator& iter) : data{iter.data} {}

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
    explicit generic_iterator(detail::list_base* data_) : data{data_} {};
    friend list<T, Tag>;

    detail::list_base* data{nullptr};
  };

  void push_back(T& val) noexcept {
    insert(end(), val);
  }

  void push_front(T& val) noexcept {
    insert(begin(), val);
  }

  void clear() noexcept {
    while (!empty()) {
      pop_back();
    }
  }

  void pop_back() noexcept {
    erase(std::prev(end()));
  }

  void pop_front() noexcept {
    erase(begin());
  }

  const T& back() const noexcept {
    return *std::prev(end());
  }

  T& back() noexcept {
    return *std::prev(end());
  }

  const T& front() const noexcept {
    return *begin();
  }

  T& front() noexcept {
    return *begin();
  }

  bool empty() const noexcept {
    return sentinel.prev == &sentinel && sentinel.next == &sentinel;
  }

  iterator begin() noexcept {
    // NOTE: actually begin() in empty list is equivalent to end() -
    // implementation detail
    return iterator{sentinel.next};
  }

  const_iterator begin() const noexcept {
    // see the note in non-constant implementation
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
    auto ptr = static_cast<list_element<Tag>*>(&val);
    it.data->insert(*ptr);
    return iterator{ptr};
  }

  iterator erase(iterator it) noexcept {
    assert(!empty());
    iterator old = it++;
    old.data->unlink();
    return it;
  }

  void splice(const_iterator pos, list& other, const_iterator first,
              const_iterator last) noexcept {
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
