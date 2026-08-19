# java2c

Como compilar:
```
flex lexer/lexer.l
bison -d parser/parser.y

gcc -o compilador lex.yy.c parser.tab.c -lfl

./a.out
``'