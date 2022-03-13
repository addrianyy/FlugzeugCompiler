#pragma once

namespace flugzeug {

class Block;
class Function;

namespace analysis {
class Loop;
}

namespace utils {

Block* get_or_create_loop_preheader(Function* function, const analysis::Loop* loop,
                                    bool allow_conditional = false);
Block* get_or_create_loop_dedicated_exit(Function* function, const analysis::Loop* loop);
Block* get_or_create_loop_single_back_edge_block(Function* function, const analysis::Loop* loop);

} // namespace utils

} // namespace flugzeug