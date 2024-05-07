rm -f src/scanner.cpp src/parser.cpp include/parser.h
flex -o src/scanner.cpp scanner.l
bison parser.y -o src/parser.cpp --header=include/parser.h
