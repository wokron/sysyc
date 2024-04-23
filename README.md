# sysyc

Sysyc is a compiler for the SysY language. It utilizes QBE IR as its intermediate representation.

## Contribution

Contribute code by checking out a new branch, formatted as `<username>--<description>`. For example, `wokron--something-to-do`.

## Todo List
- [ ] Phase One: Compiler Frontend
  - [ ] Syntax tree data structure
  - [ ] Intermediate code data structure
  - [ ] Intermediate code construction interface
  - [ ] Lexical analysis + Syntax analysis
  - [ ] Intermediate code print
  - [ ] Symbol table + Type system
  - [ ] Semantic analysis

- [ ] Phase Two: Compiler Backend
  - [ ] Register allocation algorithm
  - [ ] Target code generation

- [ ] Phase Three: Compiler Optimization
  - [ ] SSA construction and elimination
  - [ ] Other optimization algorithms
     - [ ] Peephole Optimization
     - [ ] Constant Folding and Constant Propagation
     - [ ] Function inlining
     - [ ] GCM and GVN
     - [ ] ...
