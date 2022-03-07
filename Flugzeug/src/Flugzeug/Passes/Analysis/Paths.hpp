#pragma once
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <Flugzeug/IR/Block.hpp>

namespace flugzeug::analysis {

void get_blocks_inbetween(Block* from, Block* to, Block* barrier,
                          std::unordered_set<Block*>& blocks_inbetween);
std::unordered_set<Block*> get_blocks_inbetween(Block* from, Block* to, Block* barrier);

void get_blocks_from_dominator_to_target(Block* dominator, Block* target,
                                         std::unordered_set<Block*>& blocks_inbetween);
std::unordered_set<Block*> get_blocks_from_dominator_to_target(Block* dominator, Block* target);

} // namespace flugzeug::analysis