#pragma once
#include <span>
#include <vector>

namespace flugzeug {

struct Range {
  size_t start;
  size_t end;
};

class LiveInterval {
  std::vector<Range> ranges_;

  explicit LiveInterval(std::vector<Range> ranges) : ranges_(std::move(ranges)) {}

 public:
  LiveInterval() = default;

  std::span<const Range> ranges() const { return ranges_; }

  size_t first_range_start() const;
  size_t last_range_end() const;

  bool ends_before(const LiveInterval& other) const;
  bool overlaps_with(size_t other) const;

  static bool are_overlapping(const LiveInterval& a, const LiveInterval& b);
  static LiveInterval merge(const LiveInterval& a, const LiveInterval& b);

  void clear();
  void add(Range range);
};

}  // namespace flugzeug