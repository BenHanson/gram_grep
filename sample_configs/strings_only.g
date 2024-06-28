%token RawString String
%%
list: String { match = substr($1, 1, 1); };
list: RawString { match = substr($1, 3, 2); };
list: list String { match += substr($2, 1, 1); };
list: list RawString { match += substr($2, 3, 2); };
%%
ws [ \t\r\n]+
%%
\"([^"\\\r\n]|\\.)*\" String
R\"\((?s:.)*?\)\"     RawString
%%
