%token format anything
%consume anything
%x PREBODY BODY PARENS
%%
start: format '(' ')';
%%
char '([^'\\\r\n]|\\.)+'
name [A-Z_a-z][0-9A-Z_a-z]*
string \"([^"\\\r\n]|\\.)*\"
ws [ \t\r\n]+|"//".*|"/*"(?s:.)*?"*/"
%%
<INITIAL>std::format<PREBODY>     format
<PREBODY>\(<BODY>        '('
<PREBODY>(?s:.)<.>       skip()
<BODY,PARENS>\(<>PARENS> skip()
<PARENS>\)<<>            skip()
<BODY>\)<INITIAL>        ')'
<BODY,PARENS>{char}<.>   skip()
<BODY,PARENS>{name}<.>   skip()
<BODY,PARENS>{string}<.> skip()
<BODY,PARENS>{ws}<.>     skip()
<BODY,PARENS>(?s:.)<.>   skip()
{char}                   anything
{name}                   anything
{string}                 anything
{ws}                     anything
(?s:.)                   anything
%%
