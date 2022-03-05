#pragma once
#include <cstdint>
#include <iterator>

#define FLUGZEUG_VALIDATE_USE_ITERATORS

namespace flugzeug {

class Value;
class User;

namespace detail {

class Use;

#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
void validate_use(const Value* used_value, const Use* use);
#endif

class Use {
  friend class ValueUses;
  friend class flugzeug::User;

#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  Value* used_value = nullptr;
#endif

  User* user = nullptr;

  Use* next = nullptr;
  Use* previous = nullptr;

  uint32_t operand_index = 0;

  bool heap_allocated = false;

public:
  Use() = default;
  Use(User* user, size_t operand_index) : user(user), operand_index(uint32_t(operand_index)) {}

  size_t get_operand_index() const { return size_t(operand_index); }

  User* get_user() { return user; }
  const User* get_user() const { return user; }

  Use* get_previous() { return previous; }
  const Use* get_previous() const { return previous; }

  Use* get_next() { return next; }
  const Use* get_next() const { return next; }

#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  Value* get_used_value() { return used_value; }
  const Value* get_used_value() const { return used_value; }
#endif
};

class ValueUses {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  Value* value;
#endif

  Use* first = nullptr;
  Use* last = nullptr;
  size_t size = 0;

  template <typename TUse, typename TUser> class UserIteratorInternal {
    TUse* current;
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
    const Value* used_value = nullptr;
#endif

  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = TUser;
    using pointer = value_type*;
    using reference = value_type&;

    explicit UserIteratorInternal(TUse* current) : current(current) {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
      if (current) {
        used_value = current->get_used_value();
      }
#endif
    }

    UserIteratorInternal& operator++() {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
      validate_use(used_value, current);
#endif

      current = current->get_next();
      return *this;
    }

    UserIteratorInternal operator++(int) {
      const auto before = *this;
      ++(*this);
      return before;
    }

    reference operator*() const { return *current->get_user(); }
    pointer operator->() const { return current ? current->get_user() : nullptr; }

    bool operator==(const UserIteratorInternal& rhs) const { return current == rhs.current; }
    bool operator!=(const UserIteratorInternal& rhs) const { return current != rhs.current; }
  };

public:
  explicit ValueUses(Value* value) {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
    this->value = value;
#endif
  }

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

} // namespace detail

} // namespace flugzeug