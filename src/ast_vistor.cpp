#include "ast_visitor.h"

// void ASTVisitor::visit(const BinaryExp& exp) {
//     // Visit the left expression
//     visit(*exp.left);
//     // Visit the right expression
//     visit(*exp.right);
//     // Additional logic for binary operations, such as type checking, could be added here
// }

// void ASTVisitor::visit(const BoolExp& exp) {
//     visit(*exp.left);
//     visit(*exp.right);
//     // Handle boolean expression logic
// }

// void ASTVisitor::visit(const CallExp& exp) {
//     // Check if the function is defined in the symbol table
//     if (!currentScope->getSymbol(exp.ident)) {
//         std::cerr << "Error: Function " << exp.ident << " not defined." << std::endl;
//     }
//     // Visit all actual parameters
//     for (auto& arg : *exp.func_rparams) {
//         visit(*arg);
//     }
// }

// void ASTVisitor::visit(const UnaryExp& exp) {
//     visit(*exp.exp);
//     // Handle unary expression logic
// }

// void ASTVisitor::visit(const CompareExp& exp) {
//     visit(*exp.left);
//     visit(*exp.right);
//     // Handle comparison expression logic
// }

// void ASTVisitor::visit(const AssignStmt& stmt) {
//     // Confirm that the lvalue is assignable
//     std::shared_ptr<Symbol> sym = currentScope->getSymbol(std::get<std::string>(*stmt.lval));
//     if (sym && sym->constant) {
//         std::cerr << "Error: Cannot assign to constant " << sym->name << "." << std::endl;
//     }
//     visit(*stmt.exp);
// }

// void ASTVisitor::visit(const ExpStmt& stmt) {
//     if (stmt.exp) {
//         visit(*stmt.exp);
//     }
//     // Handle expression statement logic
// }

// void ASTVisitor::visit(const BlockStmt& stmt) {
//     // New scope starts
//     auto newScope = currentScope->pushScope();
//     currentScope = newScope;
//     for (const auto& item : *stmt.block) {
//         visit(*item);
//     }
//     // Exit scope
//     currentScope = currentScope->popScope();
// }

// void ASTVisitor::visit(const IfStmt& stmt) {
//     visit(*stmt.cond);
//     visit(*stmt.if_stmt);
//     if (stmt.else_stmt) {
//         visit(*stmt.else_stmt);
//     }
//     // Handle if statement logic
// }

// void ASTVisitor::visit(const WhileStmt& stmt) {
//     loop_count ++;
//     visit(*stmt.cond);
//     visit(*stmt.stmt);
//     loop_count --;
//     // Handle while loop logic
// }

// void ASTVisitor::visit(const ControlStmt& stmt) {
//     // Handle break or continue statements
//     if (stmt.type == ControlStmt::BREAK && loop_count == 0) {
//         std::cerr << "Error: 'break' not within loop or switch." << std::endl;
//     }
//     if (stmt.type == ControlStmt::CONTINUE && loop_count == 0) {
//         std::cerr << "Error: 'continue' not within loop." << std::endl;
//     }
// }

// void ASTVisitor::visit(const ReturnStmt& stmt) {
//     if (stmt.exp) {
//         visit(*stmt.exp);
//     }
//     // Handle return statement logic
// }

// // Assuming visitBlock is to handle a new scope
// void ASTVisitor::visitBlock(const BlockStmt& block, bool newScope) {
//     if (newScope) {
//         currentScope = currentScope->pushScope();
//     }
//     for (const auto& item : *block.block) {
//         visit(*item);
//     }
//     if (newScope) {
//         currentScope = currentScope->popScope();
//     }
// }

// void ASTVisitor::visit(const VarDef& var_def) {
//     // Handle variable definition, potentially with dimensions and initialization
//     std::shared_ptr<Symbol> symbol(new Symbol(var_def.ident, nullptr, currentScope->type == Decl::CONST));
//     currentScope->addSymbol(var_def.ident, symbol);
//     if (var_def.init_val) {
//         visit(*var_def.init_val);
//     }
// }

// void ASTVisitor::visit(const Decl& decl) {
//     // Handle declaration which may include multiple variable definitions
//     for (const auto& var_def : *decl.var_defs) {
//         visit(*var_def);
//     }
// }

// void ASTVisitor::visit(const FuncFParam& param) {
//     // Handle function formal parameter
//     std::shared_ptr<Symbol> symbol(new Symbol(param.ident, nullptr, false)); // Type should be set appropriately
//     currentScope->addSymbol(param.ident, symbol);
// }

// void ASTVisitor::visit(const FuncDef& func_def) {
//     // Create a new function scope
//     auto funcScope = currentScope->pushScope();
//     currentScope = funcScope;
//     // Register the function in the symbol table
//     std::shared_ptr<Symbol> symbol(new Symbol(func_def.ident, nullptr, false)); // Set appropriate type for function
//     currentScope->addSymbol(func_def.ident, symbol);
//     // Visit all function parameters
//     for (const auto& param : *func_def.func_fparams) {
//         visit(*param);
//     }
//     // Visit the function body
//     for (const auto& item : *func_def.block) {
//         visit(*item);
//     }
//     // Exit function scope
//     currentScope = currentScope->popScope();
// }
