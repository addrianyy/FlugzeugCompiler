#pragma once
#include <cstdint>
#include <xutility>

namespace flugzeug {

class User;
class Use;

namespace detail {
void validate_use(const Use* use);
}

class Use {
  friend class ValueUses;
  friend class User;

  User* user = nullptr;
  size_t operand_index = 0;

  Use* next = nullptr;
  Use* previous = nullptr;

  bool valid = false;

public:
  Use(User* user, size_t operand_index) : user(user), operand_index(operand_index) {}

  bool is_valid() const { return valid; }

  size_t get_operand_index() const { return operand_index; }

  User* get_user() { return user; }
  const User* get_user() const { return user; }

  Use* get_previous() { return previous; }
  const Use* get_previous() const { return previous; }

  Use* get_next() { return next; }
  const Use* get_next() const { return next; }
};

class ValueUses {
  Use* first = nullptr;
  Use* last = nullptr;
  size_t size = 0;

  template <typename TUse, typename TUser> class UserIteratorInternal {
    TUse* current;

  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = TUser;
    using pointer = value_type*;
    using reference = value_type&;

    explicit UserIteratorInternal(TUse* current) : current(current) {}

    UserIteratorInternal& operator++() {
      detail::validate_use(current);
      current = current->get_next();
      detail::validate_use(current);
      return *this;
    }

    UserIteratorInternal operator++(int) {
      const auto before = *this;
      ++(*this);
      return before;
    }

    reference operator*() const { return *current->get_user(); }
    pointer operator->() const { return current->get_user(); }

    bool operator==(const UserIteratorInternal& rhs) const { return current == rhs.current; }
    bool operator!=(const UserIteratorInternal& rhs) const { return current != rhs.current; }
  };

public:
  void add_use(Use* use);
  void remove_use(Use* use);

  Use* get_first() { return first; }
  Use* get_last() { return last; }

  using iterator = UserIteratorInternal<Use, User>;
  using const_iterator = UserIteratorInternal<const Use, const User>;

  size_t get_size() const { return size; }
  bool is_empty() const { return size == 0; }

  iterator begin() { return iterator(first); }
  iterator end() { return iterator(nullptr); }

  const_iterator begin() const { return const_iterator(first); }
  const_iterator end() const { return const_iterator(nullptr); }
};

} // namespace flugzeug