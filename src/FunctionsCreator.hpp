#pragma once

namespace flugzeug {

class Function;
class Context;

} // namespace flugzeug

std::vector<flugzeug::Function*> create_functions(flugzeug::Context* context);