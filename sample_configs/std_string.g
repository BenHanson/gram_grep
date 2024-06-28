%token Name RawString String
%%
start: 'std' '::' 'string' '(' string ')'
{
  erase($1, $4);
  replace($6, 's');
};
string: String { match = substr($1, 1, 1); };
string: RawString { match = substr($1, 3, 2); };
string: string String { match += substr($2, 1, 1); };
string: string RawString { match += substr($2, 3, 2); };
%%
ws [ \t\r\n]+
%%
\(                           '('
\)                           ')'
::                           '::'
std                          'std'
string                       'string'
[0-9A-Z_a-z][0-9A-Z_a-z]*    Name
\"([^"\\\r\n]|\\.)*\"        String
R\"\((?s:.)*?\)\"            RawString
'([^'\\\r\n]|\\.)*'          skip()
{ws}|"//".*|"/*"(?s:.)*?"*/" skip()
%%
