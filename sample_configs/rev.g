%token Name
%%
// Make sure there are at least 2 items
start: Name '::' list { print(format('{}::{}\n', acc, $1)); };
list: Name { acc = $1; };
list: list '::' Name { acc = format('{}::{}', $3, acc); };
%%
%%
::              '::'
[A-Z_a-z][-\w]* Name
%%
