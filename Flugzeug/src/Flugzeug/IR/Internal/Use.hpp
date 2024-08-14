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
  Value* used_value_ = nullptr;
#endif

  User* user_ = nullptr;

  Use* next_ = nullptr;
  Use* previous_ = nullptr;

  uint32_t operand_index_ = 0;

  bool heap_allocated_ = false;

 public:
  Use() = default;
  Use(User* user, size_t operand_index) : user_(user), operand_index_(uint32_t(operand_index)) {}

  size_t operand_index() const { return size_t(operand_index_); }

  User* user() { return user_; }
  const User* user() const { return user_; }

  Use* previous() { return previous_; }
  const Use* previous() const { return previous_; }

  Use* next() { return next_; }
  const Use* next() const { return next_; }

#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  Value* used_value() { return used_value_; }
  const Value* used_value() const { return used_value_; }
#endif
};

class ValueUses {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
  Value* value_;
#endif

  Use* first_ = nullptr;
  Use* last_ = nullptr;
  size_t size_ = 0;

  template <typename TUse, typename TUser>
  class UserIteratorInternal {
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
        used_value = current->used_value();
      }
#endif
    }

    UserIteratorInternal& operator++() {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
      validate_use(used_value, current);
#endif

      current = current->next();
      return *this;
    }

    UserIteratorInternal operator++(int) {
      const auto before = *this;
      ++(*this);
      return before;
    }

    reference operator*() const { return *current->user(); }
    pointer operator->() const { return current ? current->user() : nullptr; }

    bool operator==(const UserIteratorInternal& rhs) const { return current == rhs.current; }
    bool operator!=(const UserIteratorInternal& rhs) const { return current != rhs.current; }
  };

 public:
  explicit ValueUses(Value* value) {
#ifdef FLUGZEUG_VALIDATE_USE_ITERATORS
    value_ = value;
#endif
  }

  void add_use(Use* use);
  void remove_use(Use* use);

  Use* first() { return first_; }
  Use* last() { return last_; }

  using iterator = UserIteratorInternal<Use, User>;
  using const_iterator = UserIteratorInternal<const Use, const User>;

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  iterator begin() { return iterator(first_); }
  iterator end() { return iterator(nullptr); }

  const_iterator begin() const { return const_iterator(first_); }
  const_iterator end() const { return const_iterator(nullptr); }
};

}  // namespace detail

}  // namespace flugzeug