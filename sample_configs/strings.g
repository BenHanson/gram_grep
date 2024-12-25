/*
NOTE: in order to successfully find strings it is necessary to filter out comments and chars.
As a subtlety, comments could contain apostrophes (or even unbalanced double quotes in
an extreme case)!
*/
%token RawString String
%%
list: String { match = substr($1, 1, 1); };
list: RawString { match = substr($1, 3, 2); };
list: list String { match += substr($2, 1, 1); };
list: list RawString { match += substr($2, 3, 2); };
%%
%%
\"([^"\\\r\n]|\\.)*\"       String
R\"\((?s:.)*?\)\"           RawString
'([^'\\\r\n]|\\.)*'         skip()
\s+|"//".*|"/*"(?s:.)*?"*/" skip()
%%
