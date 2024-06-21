#include <doctest.h>

#include "opt/pass/base.h"

static std::vector<std::string> calls_record;

TEST_CASE("testing pass base") {

    class TestBasicBlockPass : public BasicBlockPass {
    public:
        bool run_on_basic_block(ir::Block &bb) override {
            calls_record.push_back("run_on_basic_block");
            return false;
        }
    };

    class TestFunctionPass : public FunctionPass {
    public:
        bool run_on_function(ir::Function &func) override {
            calls_record.push_back("run_on_function");
            return false;
        }
    };

    class TestModulePass : public ModulePass {
    public:
        bool run_on_module(ir::Module &module) override {
            calls_record.push_back("run_on_module");
            return false;
        }
    };

    // build a simple module
    ir::Module module;
    auto func = std::make_shared<ir::Function>();
    module.functions.push_back(func);
    func->start = std::make_shared<ir::Block>();
    func->start->next = std::make_shared<ir::Block>();

    // run the basic block pass
    TestBasicBlockPass bb_pass;
    bb_pass.run(module);
    CHECK_EQ(calls_record.size(), 2);
    CHECK_EQ(calls_record, std::vector<std::string>{"run_on_basic_block",
                                                    "run_on_basic_block"});
    calls_record.clear();

    // run the function pass
    TestFunctionPass func_pass;
    func_pass.run(module);
    CHECK_EQ(calls_record.size(), 1);
    CHECK_EQ(calls_record, std::vector<std::string>{"run_on_function"});
    calls_record.clear();

    TestModulePass module_pass;
    module_pass.run(module);
    CHECK_EQ(calls_record.size(), 1);
    CHECK_EQ(calls_record, std::vector<std::string>{"run_on_module"});
    calls_record.clear();

    // an example of using PassPipeline
    using CompletePass =
        PassPipeline<TestBasicBlockPass, TestFunctionPass, TestModulePass>;
    CompletePass pass;
    pass.run(module);
    CHECK_EQ(calls_record.size(), 4);
    CHECK_EQ(calls_record, std::vector<std::string>{
                               "run_on_basic_block", "run_on_basic_block",
                               "run_on_function", "run_on_module"});
    calls_record.clear();
}