#include "LiveInterval.hpp"

#include <Flugzeug/Core/Error.hpp>
#include <Flugzeug/Core/Iterator.hpp>

using namespace flugzeug;

size_t LiveInterval::first_range_start() const {
  return ranges_.front().start;
}
size_t LiveInterval::last_range_end() const {
  return ranges_.back().end;
}

bool LiveInterval::ends_before(const LiveInterval& other) const {
  return last_range_end() <= other.first_range_start();
}

bool LiveInterval::overlaps_with(size_t other) const {
  return any_of(ranges_,
                [other](const Range& range) { return other >= range.start && other < range.end; });
}

void LiveInterval::add(Range range) {
  if (ranges_.empty()) {
    ranges_.push_back(range);
  } else {
    const auto last = ranges_.back();
    verify(last.end <= range.start, "Unordered insertion to live interval");

    // Try to merge the last range.
    if (last.end == range.start) {
      ranges_.back().end = range.end;
    } else {
      ranges_.push_back(range);
    }
  }
}

bool LiveInterval::are_overlapping(const LiveInterval& a, const LiveInterval& b) {
  std::span<const Range> a_ranges = a.ranges();
  std::span<const Range> b_ranges = b.ranges();

  size_t previous_end = 0;

  const auto process_range = [&](std::span<const Range>& span) {
    const auto range = span[0];
    span = span.subspan(1);

    const bool overlap = range.start < previous_end;

    previous_end = range.end;

    return overlap;
  };

  while (!a_ranges.empty() || !b_ranges.empty()) {
    // If one of the ranges is empty then we can immediately return the result.
    if (a_ranges.empty()) {
      return process_range(b_ranges);
    }
    if (b_ranges.empty()) {
      return process_range(a_ranges);
    }

    const auto ar = a_ranges[0];
    const auto br = b_ranges[0];

    bool result = false;
    if (ar.start < br.start) {
      result = process_range(a_ranges);
    } else if (ar.start > br.start) {
      result = process_range(b_ranges);
    } else {
      if (ar.end < br.end) {
        result = process_range(a_ranges);
      } else {
        result = process_range(b_ranges);
      }
    }

    if (result) {
      return true;
    }
  }

  return false;
}

LiveInterval LiveInterval::merge(const LiveInterval& a, const LiveInterval& b) {
  std::span<const Range> a_ranges = a.ranges();
  std::span<const Range> b_ranges = b.ranges();

  std::vector<Range> result;

  const auto process_range = [&](std::span<const Range>& span) {
    const auto range = span[0];
    span = span.subspan(1);

    if (result.empty()) {
      result.push_back(range);
    } else {
      if (range.start <= result.back().end) {
        result.back().end = std::max(result.back().end, range.end);
      } else {
        result.push_back(range);
      }
    }
  };

  while (!a_ranges.empty() || !b_ranges.empty()) {
    if (a_ranges.empty()) {
      while (!b_ranges.empty()) {
        process_range(b_ranges);
      }
      break;
    }

    if (b_ranges.empty()) {
      while (!a_ranges.empty()) {
        process_range(a_ranges);
      }
      break;
    }

    const auto ar = a_ranges[0];
    const auto br = b_ranges[0];

    if (ar.start < br.start) {
      process_range(a_ranges);
    } else if (ar.start > br.start) {
      process_range(b_ranges);
    } else {
      if (ar.end < br.end) {
        process_range(a_ranges);
      } else {
        process_range(b_ranges);
      }
    }
  }

  return LiveInterval(std::move(result));
}

void LiveInterval::clear() {
  ranges_.clear();
}
