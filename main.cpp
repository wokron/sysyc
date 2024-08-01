#include "ast.h"
#include "error.h"
#include "ir/ir.h"
#include "opt/pass/pass.h"
#include "parser.h"
#include "target/target.h"
#include "visitor.h"
#include <fstream>
#include <getopt.h>

using Passes = opt::PassPipeline<
    opt::FillPredsPass, opt::SimplifyCFGPass, opt::FillPredsPass,
    opt::FillReversePostOrderPass, opt::FillUsesPass,
    opt::CooperFillDominatorsPass, opt::FillDominanceFrontierPass,
    opt::SSAConstructPass, opt::FillUsesPass, opt::GVNPass, opt::FillUsesPass,
    opt::SimpleDeadCodeEliminationPass, opt::FillPredsPass,
    opt::SSADestructPass, opt::LocalConstAndCopyPropagationPass,
    opt::FillUsesPass, opt::SimpleDeadCodeEliminationPass, opt::FillPredsPass,
    opt::SimplifyCFGPass, opt::LocalConstAndCopyPropagationPass,
    opt::FillUsesPass, opt::SimpleDeadCodeEliminationPass, opt::FillPredsPass,
    opt::SimplifyCFGPass>;

using RegisterPasses =
    opt::PassPipeline<opt::FillUsesPass, opt::FillReversePostOrderPass,
                      opt::LivenessAnalysisPass, opt::FillIntervalPass>;

struct Options {
    bool optimize = false;
    bool emit_ast = false;
    bool emit_ir = false;
    bool emit_asm = false;
    std::string output;
};

void usage(const char *name) {
    std::cerr << "Usage: " << name << " [options] [file]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -h, --help: Show this help message" << std::endl;
    std::cerr << "  -O1: Enable optimization" << std::endl;
    std::cerr << "  --emit-ast: Emit AST as JSON" << std::endl;
    std::cerr << "  --emit-ir: Emit IR as JSON" << std::endl;
    std::cerr << "  -S, --emit-asm: Emit assembly" << std::endl;
    std::cerr << "  -o, --output: Output file" << std::endl;
}

void cmd_error(const char *name, const std::string &msg, int exitcode = 1) {
    std::cerr << name << ": " << msg << std::endl;
    exit(exitcode);
}

extern FILE *yyin;

void compile(const char *name, const Options &options,
             const std::string &input) {
    std::ofstream outfile;
    auto output = options.output;

    yyin = fopen(input.c_str(), "r");

    if (yyin == nullptr) {
        cmd_error(name, "failed to open file: " + input, 4);
    }

    auto root = std::make_shared<CompUnits>();
    yyparse(root);

    if (options.emit_ast) {
        if (output.length() == 0) {
            output = "out.json";
        }
        outfile.open(output, std::ios::out);
        print_ast(outfile, *root);
        return;
    }

    ir::Module module;
    Visitor visitor(module, options.optimize);
    visitor.visit(*root);

    if (has_error()) {
        cmd_error(name, "compilation failed", 5);
    }

    if (options.optimize) {
        Passes pass;
        pass.run(module);
    }

    if (options.emit_ir) {
        if (output.length() == 0) {
            output = "out.ssa";
        }
        outfile.open(output, std::ios::out);
        module.emit(outfile);
        return;
    }

    RegisterPasses reg_pass;
    reg_pass.run(module);

    // std::cerr << "Register allocation:" << std::endl;
    // for (auto &func : module.functions) {
    //     std::cerr << "Function: " << func->name << std::endl;
    //     target::LinearScanAllocator regalloc;
    //     regalloc.allocate_registers(*func);
    //     for (auto [temp, reg] : regalloc.get_register_map()) {
    //         temp->emit(std::cerr);
    //         std::cerr << " -> " << target::regno2string(reg) << std::endl;
    //     }
    // }

    // std::cerr << "Stack layout:" << std::endl;
    // for (auto &func : module.functions) {
    //     std::cerr << "Function: " << func->name << std::endl;
    //     target::StackManager stack_manager;
    //     stack_manager.run(*func);

    //     for (auto [reg, offset] :
    //          stack_manager.get_callee_saved_regs_offset()) {
    //         std::cerr << target::regno2string(reg) << " -> sp(" << offset <<
    //         ")"
    //                   << std::endl;
    //     }
    //     for (auto [temp, offset] : stack_manager.get_local_var_offset()) {
    //         temp->emit(std::cerr);
    //         std::cerr << " -> sp(" << offset << ")" << std::endl;
    //     }
    //     for (auto [temp, offset] : stack_manager.get_spilled_temps_offset())
    //     {
    //         temp->emit(std::cerr);
    //         std::cerr << " -> sp(" << offset << ")" << std::endl;
    //     }
    // }

    if (options.emit_asm) {
        if (output.length() == 0) {
            output = "out.s";
        }
        outfile.open(output, std::ios::out);
        target::Generator generator(outfile, options.optimize);

        generator.generate(module);
        return;
    }

    cmd_error(name, "nothing to do", 6);
}

int main(int argc, char *argv[]) {
    enum {
        HELP = 256,
        O1,
        EMIT_IR,
        EMIT_AST,
        EMIT_ASM,
        OUTPUT,
    };
    const struct option long_options[] = {
        {"help", no_argument, 0, HELP},
        {"O1", no_argument, 0, O1},
        {"emit-ast", no_argument, 0, EMIT_AST},
        {"emit-ir", no_argument, 0, EMIT_IR},
        {"emit-asm", no_argument, 0, EMIT_ASM},
        {"output", required_argument, 0, OUTPUT},
        {0, 0, 0, 0}};

    Options options;
    int opt;

    while ((opt = getopt_long_only(argc, argv, "hSo:", long_options, NULL)) !=
           -1) {
        switch (opt) {
        case 'h':
        case HELP:
            usage(argv[0]);
            return 0;
        case O1:
            options.optimize = true;
            break;
        case EMIT_AST:
            options.emit_ast = true;
            break;
        case EMIT_IR:
            options.emit_ir = true;
            break;
        case 'S':
        case EMIT_ASM:
            options.emit_asm = true;
            break;
        case 'o':
        case OUTPUT:
            options.output = optarg;
            break;
        case '?':
            cmd_error(argv[0], "unknown option", 2);
            return 1;
        }
    }

    if (argv[optind] == nullptr) {
        cmd_error(argv[0], "no input file", 3);
        return 1;
    }

    compile(argv[0], options, argv[optind]);

    return 0;
}
