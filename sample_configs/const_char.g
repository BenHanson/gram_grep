%token Name
%%
start: 'const' 'char' '*' '>)'
{
  replace($1.first, $4.first, 'LPCTSTR');
};
start: 'const' 'char' '*' opt_amp Name
{
  replace($1.first, $4.first, 'LPCTSTR ');
};
start: 'const' 'char' '*' 'const'
{
  replace($1.first, $4.first, 'LPCTSTR ');
};
start: 'const' 'char' Name '[' ']'
{
  replace($2, 'TCHAR');
};
opt_amp: %empty | '&';
%%
ws [ \t\r\n]+
%%
\[                           '['
\]                           ']'
[>)]                         '>)'
\*                           '*'
&                            '&'
char                         'char'
const                        'const'
[0-9A-Z_a-z][0-9A-Z_a-z]*    Name
\"([^"\\\r\n]|\\.)*\"        skip()
R\"\((?s:.)*?\)\"            skip()
'([^'\\\r\n]|\\.)*'          skip()
{ws}|"//".*|"/*"(?s:.)*?"*/" skip()
%%
