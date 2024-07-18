# sysyc

Sysyc is a compiler for the SysY language. It utilizes QBE IR as its intermediate representation.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Todo List
- [X] Phase One: Compiler Frontend
  - [x] Syntax tree data structure
  - [X] Intermediate code data structure
  - [X] Intermediate code construction interface
  - [x] Lexical analysis + Syntax analysis
  - [X] Intermediate code print
  - [x] Symbol table + Type system
  - [X] Semantic analysis

- [ ] Phase Two: Compiler Backend
  - [X] Register allocation algorithm
  - [ ] Target code generation

- [ ] Phase Three: Compiler Optimization
  - [X] SSA construction and elimination
  - [ ] Other optimization algorithms
     - [ ] Peephole Optimization
     - [X] Constant Folding and Constant Propagation
     - [X] Function Inlining
     - [ ] GCM
     = [X] GVN
     - [X] Loop Rotation
     - [X] CFG Simplifying
     - [X] Deadcode Elimination
     - [ ] Strength Reduction
     - [ ] Tail Recursion Optimization
