#include "intrusive_list.h"

namespace intrusive::detail {

bool list_base::is_single() const {
  return prev == this && next == this;
}

list_base::list_base() : prev{this}, next{this} {}

list_base::list_base(list_base&& other) : list_base{} {
  *this = std::move(other);
}

list_base::list_base(const list_base&) : list_base{} {} // not default!

list_base& list_base::operator=(list_base&& other) {
  if (this == &other) {
    return *this;
  }
  assert(is_single()); // otherwise it's illegal to do an assignment
  if (other.is_single()) {
    // noop
    return *this;
  }
  prev = other.prev;
  next = other.next;

  prev->next = this;
  next->prev = this;

  other.prev = &other;
  other.next = &other;
  return *this;
}

void list_base::unlink() {
  // No need to check in case of single node
  prev->next = next;
  next->prev = prev;
  prev = next = this;
}

void list_base::insert(list_base& other) {
  if (this == &other) {
    // We don't want to insert the element before itself
    return;
  }
  other.unlink();
  assert(other.is_single());

  prev->next = &other;
  other.prev = this->prev;

  other.next = this;
  this->prev = &other;
}

list_base::~list_base() {
  unlink();
}

} // namespace intrusive::detail
