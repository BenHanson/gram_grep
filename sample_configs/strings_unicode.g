%token RawString String
%%
str: '_T' '(' String ')'
{
  erase($1);
  replace($2, 'L');
  erase($4);
};
str: 'L' String;
str: 'L' 'R' String;
str: String { insert($1, 'L'); };
str: 'R' RawString { insert($1, 'L'); };
str: 'TRACE' '(' String;
str: 'TRACE0' '(' String;
%%
ws [ \t\r\n]+
%%
\(                           '('
\)                           ')'
_T                           '_T'
L                            'L'
R                            'R'
TRACE                        'TRACE'
TRACE0                       'TRACE0'
#include.*                   skip()
#pragma\s+push_macro.*       skip()
#pragma\s+pop_macro.*        skip()
extern\s+\"C\".*             skip()
\"([^"\\\r\n]|\\.)*\"        String
\"\((?s:.)*?\)\"             RawString
'([^'\\\r\n]|\\.)*'          skip()
{ws}|"//".*|"/*"(?s:.)*?"*/" skip()
%%
