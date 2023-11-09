%token FORMAT STRING String VString
%%
start: format list;
format: STRING '.' FORMAT '(';
list: String { match = substr($1, 1, 1); };
list: VString { match = substr($1, 2, 1); };
list: list '+' String { match += substr($3, 1, 1); };
list: list '+' VString { match += substr($3, 2, 1); };
%%
ws [ \t\r\n]+
%%
\+                           '+'
\.                           '.'
\(                           '('
Format                       FORMAT
String                       STRING
\"([^"\\\r\n]|\\.)*\"        String
@\"([^"]|\"\")*\"            VString
'([^'\\\r\n]|\\.)*'          skip()
{ws}|"//".*|"/*"(?s:.)*?"*/" skip()
%%
