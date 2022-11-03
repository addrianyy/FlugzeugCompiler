#pragma once

#define CLASS_NON_COPYABLE(class_name)    \
  class_name(const class_name&) = delete; \
  class_name& operator=(const class_name&) = delete;

#define CLASS_NON_MOVABLE(class_name) \
  class_name(class_name&&) = delete;  \
  class_name& operator=(class_name&&) = delete;

#define CLASS_NON_MOVABLE_NON_COPYABLE(class_name) \
  CLASS_NON_COPYABLE(class_name)                   \
  CLASS_NON_MOVABLE(class_name)
