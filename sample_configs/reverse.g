%token Name
%%
start: Name '::' list { replace($1.first, $3.second, format('{}::{}', acc, $1)); };
list: Name { acc = $1; };
list: list '::' Name { acc = format('{}::{}', $3, acc); };
%%
%%
::              '::'
[A-Z_a-z][-\w]* Name
%%
