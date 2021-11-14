#include <Flugzeug/IR/Block.hpp>
#include <Flugzeug/IR/Function.hpp>
#include <Flugzeug/IR/InstructionInserter.hpp>
#include <Flugzeug/IR/Instructions.hpp>

using namespace flugzeug;

std::vector<Function*> create_functions(Context* context) {
  InstructionInserter inserter;
  Function* f = nullptr;

  Function* function_1 =
    context->create_function(context->get_void_ty(), "print_num", {context->get_i32_ty()});
  Function* function_0 =
    context->create_function(context->get_void_ty(), "print", {context->get_i8_ty()->ref(1)});
  Function* function_3 = context->create_function(context->get_i32_ty(), "test123",
                                                  {context->get_i32_ty(), context->get_i32_ty()});
  Function* function_2 = context->create_function(context->get_i32_ty(), "main", {});
  {
    f = function_1;

    Parameter* v0 = f->get_parameter(0);

    Constant* v21 = context->get_constant(context->get_i64_ty(), 1ull);
    Constant* v22 = context->get_constant(context->get_i8_ty(), 48ull);
    Constant* v19 = context->get_constant(context->get_i32_ty(), 10ull);
    Constant* v29 = context->get_constant(context->get_i64_ty(), 254ull);
    Constant* v30 = context->get_constant(context->get_i32_ty(), 0ull);
    Constant* v25 = context->get_constant(context->get_i64_ty(), 255ull);
    Constant* v24 = context->get_constant(context->get_i8_ty(), 0ull);
    Constant* v27 = context->get_constant(context->get_i64_ty(), 253ull);
    Constant* v26 = context->get_constant(context->get_i32_ty(), 48ull);

    Block* entry = f->create_block();
    Block* block_2 = f->create_block();
    Block* block_4 = f->create_block();
    Block* block_5 = f->create_block();
    Block* block_6 = f->create_block();

    inserter.set_insertion_block(entry);
    auto v1 = inserter.stack_alloc(context->get_i8_ty(), 256);
    auto v2 = inserter.offset(v1, v25);
    inserter.store(v2, v24);
    auto v3 = inserter.compare_eq(v0, v30);
    inserter.cond_branch(v3, block_2, block_4);

    inserter.set_insertion_block(block_2);
    auto v4 = inserter.offset(v1, v29);
    inserter.store(v4, v22);
    inserter.branch(block_4);

    inserter.set_insertion_block(block_4);
    auto v5 = inserter.phi(context->get_i64_ty());
    auto v6 = inserter.phi(context->get_i32_ty());
    auto v7 = inserter.compare_ugt(v6, v30);
    inserter.cond_branch(v7, block_5, block_6);

    inserter.set_insertion_block(block_5);
    auto v8 = inserter.udiv(v6, v19);
    auto v9 = inserter.umod(v6, v19);
    auto v10 = inserter.add(v9, v26);
    auto v11 = inserter.trunc(v10, context->get_i8_ty());
    auto v12 = inserter.offset(v1, v5);
    inserter.store(v12, v11);
    auto v13 = inserter.sub(v5, v21);
    inserter.branch(block_4);

    inserter.set_insertion_block(block_6);
    auto v14 = inserter.add(v5, v21);
    auto v15 = inserter.offset(v1, v14);
    inserter.call(function_0, {v15});
    inserter.ret();

    v5->add_incoming(entry, v29);
    v5->add_incoming(block_5, v13);
    v5->add_incoming(block_2, v27);
    v6->add_incoming(entry, v0);
    v6->add_incoming(block_5, v8);
    v6->add_incoming(block_2, v0);
  }
  {
    f = function_3;

    Constant* v6 = context->get_constant(context->get_i32_ty(), 12ull);

    Block* entry = f->create_block();

    inserter.set_insertion_block(entry);
    inserter.ret(v6);
  }
  {
    f = function_2;

    Constant* v7 = context->get_constant(context->get_i32_ty(), 1ull);
    Constant* v9 = context->get_constant(context->get_i32_ty(), 8ull);
    Constant* v10 = context->get_constant(context->get_i32_ty(), 0ull);

    Block* entry = f->create_block();
    Block* block_1 = f->create_block();
    Block* block_2 = f->create_block();
    Block* block_4 = f->create_block();

    inserter.set_insertion_block(entry);
    inserter.branch(block_1);

    inserter.set_insertion_block(block_1);
    auto v0 = inserter.phi(context->get_i32_ty());
    auto v1 = inserter.phi(context->get_i32_ty());
    auto v2 = inserter.phi(context->get_i32_ty());
    auto v3 = inserter.compare_ult(v0, v9);
    inserter.cond_branch(v3, block_2, block_4);

    inserter.set_insertion_block(block_2);
    inserter.call(function_1, {v2});
    auto v4 = inserter.add(v2, v1);
    auto v5 = inserter.add(v0, v7);
    inserter.branch(block_1);

    inserter.set_insertion_block(block_4);
    inserter.ret(v10);

    v0->add_incoming(entry, v10);
    v0->add_incoming(block_2, v5);
    v1->add_incoming(entry, v7);
    v1->add_incoming(block_2, v4);
    v2->add_incoming(entry, v10);
    v2->add_incoming(block_2, v1);
  }
  return {function_1, function_0, function_3, function_2};
}