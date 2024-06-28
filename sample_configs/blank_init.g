%token Name
%%
start: string Name '=' '""' ';'
{
  erase($2.second, $5.first);
};
start: string Name '(' '""' ')' ';'
{
  erase($2.second, $6.first);
};
string: 'string' | 'CString';
%%
ws [ \t\r\n]+
%%
;                                      ';'
\(                                     '('
\)                                     ')'
=                                      '='
\"\"                                   '""'
CString                                'CString'
string                                 'string'
[0-9A-Z_a-z][0-9A-Z_a-z]*              Name
'([^'\\\r\n]|\\.)*'                    skip()
{ws}|"//".*|"/*"(?s:.)*?"*/"           skip()
%%
