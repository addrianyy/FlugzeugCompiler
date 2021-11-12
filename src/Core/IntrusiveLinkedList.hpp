#pragma once
#include "ClassTraits.hpp"
#include "Error.hpp"
#include <memory>

template <typename T, typename Owner> class IntrusiveNode;
template <typename T, typename Owner> class IntrusiveLinkedList;
template <typename T, typename Node, typename Owner> class IntrusiveLinkedListIterator;

template <typename T, typename Owner> class IntrusiveNode {
  friend class IntrusiveLinkedList<T, Owner>;
  friend class IntrusiveLinkedListIterator<T, IntrusiveNode<T, Owner>, Owner>;
  friend class IntrusiveLinkedListIterator<const T, const IntrusiveNode<T, Owner>, Owner>;

  using List = IntrusiveLinkedList<T, Owner>;

  Owner* owner = nullptr;
  IntrusiveNode* next = nullptr;
  IntrusiveNode* previous = nullptr;

  List& get_list() {
    verify(owner, "Cannot get containing list for unlinked node.");
    return owner->get_list();
  }

  T* to_underlying() { return static_cast<T*>(this); }

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(IntrusiveNode);

  IntrusiveNode() = default;
  virtual ~IntrusiveNode() { verify(!owner, "Tried to destroy linked node."); }

  Owner* get_owner() { return owner; }
  T* get_previous() { return static_cast<T*>(previous); }
  T* get_next() { return static_cast<T*>(next); }

  const Owner* get_owner() const { return owner; }
  const T* get_previous() const { return static_cast<const T*>(previous); }
  const T* get_next() const { return static_cast<const T*>(next); }

  void insert_before(T* before) { before->get_list().insert_before(to_underlying(), before); }
  void insert_after(T* after) { after->get_list().insert_after(to_underlying(), after); }

  void move_before(T* before) {
    verify(before->owner == owner, "Tried to move between different lists.");

    get_list().unlink(to_underlying());
    insert_before(before);
  }

  void move_after(T* after) {
    verify(after->owner == owner, "Tried to move between different lists.");

    get_list().unlink(to_underlying());
    insert_after(after);
  }

  void unlink() { get_list().unlink(to_underlying()); }

  void destroy() {
    if (owner) {
      get_list().unlink(to_underlying());
    }

    delete this;
  }
};

template <typename T, typename Owner> class IntrusiveLinkedList {
  using Node = IntrusiveNode<T, Owner>;

  Owner* owner = nullptr;
  Node* first = nullptr;
  Node* last = nullptr;

  size_t size = 0;

  Node* to_node(T* node) { return static_cast<Node*>(node); }

  void own_node(Node* node) {
    verify(node, "Cannot own null node.");
    verify(!node->owner, "Node is already owned.");

    node->owner = owner;

    size++;

    owner->on_added_node(static_cast<T*>(node));
  }

public:
  CLASS_NON_MOVABLE_NON_COPYABLE(IntrusiveLinkedList);

  explicit IntrusiveLinkedList(Owner* owner) : owner(owner) {}

  T* get_first() { return static_cast<T*>(first); }
  T* get_last() { return static_cast<T*>(last); }

  const T* get_first() const { return static_cast<const T*>(first); }
  const T* get_last() const { return static_cast<const T*>(last); }

  size_t get_size() const { return size; }

  ~IntrusiveLinkedList() {
    auto to_delete = first;

    while (to_delete) {
      const auto node = to_delete;

      to_delete = node->next;

      node->destroy();
    }
  }

  void insert_before(T* node, T* before) {
    const auto list_node = to_node(node);
    const auto before_node = to_node(before);

    own_node(list_node);

    if (before == nullptr) {
      const auto previous_first = first;

      first = list_node;

      list_node->previous = nullptr;
      list_node->next = previous_first;

      if (previous_first) {
        verify(!previous_first->previous, "Invalid previous link.");

        previous_first->previous = list_node;
      } else {
        verify(!last, "Invalid last node.");

        last = list_node;
      }
    } else {
      verify(before_node->owner == owner, "Before node is not owned by this list.");

      list_node->next = before_node;
      list_node->previous = before_node->previous;

      if (before_node->previous) {
        before_node->previous->next = list_node;
      } else {
        verify(before_node == first, "List corruption.");

        first = list_node;
      }

      before_node->previous = list_node;
    }
  }

  void insert_after(T* node, T* after) {
    const auto list_node = to_node(node);
    const auto after_node = to_node(after);

    own_node(list_node);

    if (after == nullptr) {
      const auto previous_last = last;

      last = list_node;

      list_node->previous = previous_last;
      list_node->next = nullptr;

      if (previous_last) {
        verify(!previous_last->next, "Invalid next link.");
        previous_last->next = list_node;
      } else {
        verify(!first, "Invalid first node.");

        first = list_node;
      }
    } else {
      verify(after_node->owner == owner, "After node is not owned by this list.");

      list_node->next = after_node->next;
      list_node->previous = after_node;

      if (after_node->next) {
        after_node->next->previous = list_node;
      } else {
        verify(after_node == last, "List corruption.");

        last = list_node;
      }

      after_node->next = list_node;
    }
  }

  void unlink(T* node) {
    const auto unlink_node = to_node(node);

    verify(unlink_node->owner == owner, "Cannot unlink this node, it's not owned by us.");

    if (unlink_node->previous) {
      unlink_node->previous->next = unlink_node->next;
    } else {
      verify(unlink_node == first, "List corruption.");

      first = unlink_node->next;
    }

    if (unlink_node->next) {
      unlink_node->next->previous = unlink_node->previous;
    } else {
      verify(unlink_node == last, "List corruption.");

      last = unlink_node->previous;
    }

    unlink_node->next = nullptr;
    unlink_node->previous = nullptr;
    unlink_node->owner = nullptr;

    size--;

    owner->on_removed_node(node);
  }

  T* push_front(T* insert_node) {
    insert_before(insert_node, nullptr);
    return insert_node;
  }

  T* push_back(T* insert_node) {
    insert_after(insert_node, nullptr);
    return insert_node;
  }

  bool is_empty() const { return first == nullptr; }

  using iterator = IntrusiveLinkedListIterator<T, IntrusiveNode<T, Owner>, Owner>;
  using const_iterator = IntrusiveLinkedListIterator<const T, const IntrusiveNode<T, Owner>, Owner>;

  iterator begin() { return iterator(first); }
  iterator end() { return iterator(nullptr); }

  const_iterator begin() const { return const_iterator(first); }
  const_iterator end() const { return const_iterator(nullptr); }
};

template <typename T, typename Node, typename Owner> class IntrusiveLinkedListIterator {
  Node* current = nullptr;

public:
  using Item = T;

  IntrusiveLinkedListIterator() = default;
  explicit IntrusiveLinkedListIterator(Node* node) : current(node) {}

  IntrusiveLinkedListIterator& operator++() {
    current = current->next;
    return *this;
  }

  T& operator*() const { return static_cast<T&>(*current); }
  T* operator->() const { return static_cast<T*>(current); }

  IntrusiveLinkedListIterator get_next_iterator() const {
    return IntrusiveLinkedListIterator(current->next);
  }

  IntrusiveLinkedListIterator& operator=(const IntrusiveLinkedListIterator& other) {
    if (&other != this) {
      current = other.current;
    }

    return *this;
  }

  bool operator==(const IntrusiveLinkedListIterator& rhs) const { return current == rhs.current; }
  bool operator!=(const IntrusiveLinkedListIterator& rhs) const { return current != rhs.current; }
};