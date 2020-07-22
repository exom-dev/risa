# About <a href="https://cmake.org/cmake/help/v3.15"><img align="right" src="https://img.shields.io/badge/CMake-3.15-BA1F28?logo=CMake" alt="CMake 3.15" /></a><a href="https://en.wikipedia.org/wiki/C_(programming_language)"><img align="right" src="https://img.shields.io/badge/C-99-A5B4C6?logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHZpZXdCb3g9IjAgMCAyNCAyNCIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48dGl0bGU+QysrIGljb248L3RpdGxlPjxwYXRoIGZpbGw9IiNmZmZmZmYiIGQ9Ik0yMi4zOTMgNmMtLjE2Ny0uMjktLjM5OC0uNTQzLS42NTItLjY5TDEyLjkyNS4yMmMtLjUwOC0uMjkzLTEuMzM5LS4yOTMtMS44NDcgMEwyLjI2IDUuMzFjLS41MDguMjkzLS45MjMgMS4wMTMtLjkyMyAxLjZ2MTAuMThjMCAuMjk0LjEwNC42Mi4yNzEuOTEuMTY3LjI5LjM5OC41NDMuNjUyLjY4OWw4LjgxNiA1LjA5MWMuNTA4LjI5MyAxLjMzOS4yOTMgMS44NDcgMGw4LjgxNi01LjA5MWMuMjU0LS4xNDYuNDg1LS4zOTkuNjUyLS42ODlzLjI3MS0uNjE2LjI3MS0uOTFWNi45MWMuMDAyLS4yOTQtLjEwMi0uNjItLjI2OS0uOTF6TTEyIDE5LjEwOWMtMy45MiAwLTcuMTA5LTMuMTg5LTcuMTA5LTcuMTA5UzguMDggNC44OTEgMTIgNC44OTFhNy4xMzMgNy4xMzMgMCAwIDEgNi4xNTYgMy41NTJsLTMuMDc2IDEuNzgxQTMuNTY3IDMuNTY3IDAgMCAwIDEyIDguNDQ1Yy0xLjk2IDAtMy41NTQgMS41OTUtMy41NTQgMy41NTVTMTAuMDQgMTUuNTU1IDEyIDE1LjU1NWEzLjU3IDMuNTcgMCAwIDAgMy4wOC0xLjc3OGwzLjA3NyAxLjc4QTcuMTM1IDcuMTM1IDAgMCAxIDEyIDE5LjEwOXoiLz48L3N2Zz4=" alt="C 99" /></a>

**Risa** is an embeddable and modular scripting language.

**This implementation is extremely early, work-in-progress and, therefore, not yet ready for an alpha release.**

# Extended BNF (EBNF) notation

This is the EBNF notation for Risa's grammar.

```abnf
script ::= ( space declaration space )* EOF ;

declaration ::= fnDecl
              | varDecl
              | stmt ;

fnDecl  ::= "function" space fn ;
varDecl ::= "var" space IDENTIFIER ( space "=" expr)? ";" ;

fn     ::= IDENTIFIER space "(" space params? space ")" ( block | "=>" expr );
params ::= IDENTIFIER ( "," space IDENTIFIER )* ;

stmt ::= exprStmt
       | ifStmt
       | whileStmt
       | forStmt
       | returnStmt
       | continueStmt
       | breakStmt
       | block ;

exprStmt     ::= expr ";" space ;
ifStmt       ::= "if" space "(" expr ")" space stmt space ( space "else" space stmt space )? ;
whileStmt    ::= "while" space "(" expr ")" space stmt ;
forStmt      ::= "for" "(" ( varDec | exprStmt | ";" ) expr? ";" expr? ")" space stmt ;
returnStmt   ::= space "return" expr? ";" ;
continueStmt ::= space "continue" INTEGER? ";" ;
breakStmt    ::= space "break" INTEGER? ";" ;
block        ::= space "{" declaration* "}" ;

expr ::= space comma space ;

comma ::= assignment space "," space assignment ;

assignment ::= ( call space "." space )? IDENTIFIER space "=" space assignment
             | ternary ;

ternary ::= or space "?" expr ":" expr ;

or  ::= and ( "||" and)* ;
and ::= bitwiseOr ( "&&" bitwiseOr )* ;

bitwiseOr  ::= bitwiseXor ( "|" bitwiseXor )* ;
bitwiseXor ::= bitwiseAnd ( "^" bitwiseAnd )* ;
bitwiseAnd ::= equality ( "&" equality )* ;

equality       ::= comparison ( ( "==" | "!=" ) comparison )* ;
comparison     ::= shift ( ( "<" | "<=" | ">" | ">=" ) shift )* ;
shift          ::= addition ( ( "<<" | ">>" ) addition )* ;
addition       ::= multiplication ( ( "+" | "-" ) multiplication )* ;
multiplication ::= unary ( ( "*" | "/" | "%" ) unary )* ;

unary ::= space ( ( "!" | "-" | "~" ) unary | call ) space ;
call  ::= primary ( "(" args? ")" | "." IDENTIFIER )* ;
args  ::= expr ( "," expr )* ;

primary ::= "null"
          | "true"
          | "false"
          | NUMBER
          | STRING
          | IDENTIFIER
          | "(" expr ")" ;

space ::= ""
        | " "
        | "\t"
        | "\r"
        | "\n"
        | comment ;

comment ::= "//" ( COMMENTCHAR )*
          | "/*" ( ANYCHAR )* "*/" ;

COMMENTCHAR ::= [#x00-#x09#x0B-#xFF] ;
ANYCHAR     ::= [#x00-#xFF] ;

NUMBER ::= BYTE
         | INTEGER
         | FLOAT ;

STRING     ::= "\"" STRCHAR "\"" ;
IDENTIFIER ::= ALPHA ( ALPHA | DIGIT )* ;

BYTE    ::= DIGIT+ "b" ;
INTEGER ::= DIGIT+ ;
FLOAT   ::= DIGIT+ "." DIGIT+ ;

STRCHAR ::= [#x00-#x09#x0B-#x21#x23-#xFF] | "\\\"" ;

ALPHA ::= [a-zA-Z] | "_" ;
DIGIT ::= [0-9] ;
```

# TODO

- ~~functions~~
- ~~closures~~
- ~~garbage collector~~
- ~~break statement~~
- ~~continue statement~~
- objects
- arrays
- ~~ternary operator~~
- ~~function arrow body~~
- lambda expressions
- ~~string escape sequences~~
- ~~comma operator~~
- compiler optimizations
- assembler + inline assembly

# License <a href="https://github.com/exom-dev/risa/blob/master/LICENSE"><img align="right" src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT"></a>

This project was created by [The Exom Developers](https://github.com/exom-dev).

The Risa project is licensed under the [MIT](https://github.com/exom-dev/risa/blob/master/LICENSE) license.

## References

[This](https://github.com/munificent/craftinginterpreters) and [this paper](https://www.lua.org/doc/jucs05.pdf).