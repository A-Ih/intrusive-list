#include "intrusive_list.h"

namespace intrusive {

list_base::list_base() = default;

list_base::list_base(list_base&& other) : prev{other.prev}, next{other.next} {
  if (other.prev != nullptr && other.next != nullptr) {
    prev->next = this;
    next->prev = this;
    other.prev = other.next = nullptr;
  }
}

list_base::list_base(const list_base&) {} // not default!

list_base& list_base::operator=(list_base&& other) {
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

list_base& list_base::operator=(const list_base& other) {
  return *this;
}

void list_base::unlink() {
  if (prev == nullptr && next == nullptr) {
    return;
  }
  prev->next = next;
  next->prev = prev;
  prev = next = nullptr;
}

/// Insert `other` before this element
void list_base::insert(list_base& other) {
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

list_base::~list_base() {
  unlink();
}

} // namespace intrusive
