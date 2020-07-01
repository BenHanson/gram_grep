%token Int
%%
list: Int;
list: list ',' Int;
%%
ws \s+
%%
[0-9]+ Int
, ','
{ws} skip()
%%
