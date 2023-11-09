%token STRING
%%
start: STRING+;
%%
%%
[ \t\n\r]+  skip()
\"(\\.|[^"\\\n\r])*\"|'(\\.|[^'\\\n\r])*'	STRING
%%
