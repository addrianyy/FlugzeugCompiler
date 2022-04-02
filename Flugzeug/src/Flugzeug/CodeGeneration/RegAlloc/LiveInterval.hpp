#pragma once
#include <span>
#include <vector>

namespace flugzeug {

struct Range {
  size_t start;
  size_t end;
};

class LiveInterval {
  std::vector<Range> ranges;

  explicit LiveInterval(std::vector<Range> ranges) : ranges(std::move(ranges)) {}

public:
  LiveInterval() = default;

  std::span<const Range> get_ranges() const { return ranges; }

  size_t first_range_start() const;
  size_t last_range_end() const;

  bool ended_before(const LiveInterval& other) const;

  static bool are_overlapping(const LiveInterval& a, const LiveInterval& b);
  static LiveInterval merge(const LiveInterval& a, const LiveInterval& b);

  void clear();
  void add(Range range);
};

} // namespace flugzeug