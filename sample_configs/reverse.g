%token Name
%%
start: Name '::' list { replace($1.first, $3.second, format('{}::{}', acc, $1)); };
list: Name { acc = $1; };
list: Name '::' list { acc += format('::{}', $1); };
%%
%%
::              '::'
[A-Z_a-z][-\w]* Name
%%
