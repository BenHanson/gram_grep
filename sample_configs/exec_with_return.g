%token exec_with_return anything
%x PREBODY BODY PARENS
%%
start: exec_with_return '(' ')';
%%
any (?s:.)
char '([^'\\]|\\.)+'
name [A-Z_a-z]\w*
string \"([^"\\]|\\.)*\"|R\"\((?s:.)*?\)\"
skip_string R\"_\((?s:.)*?\)_\"
ws [ \t\r\n]+|\/\/.*|\/\*(?s:.)*?\*\/
%%
<INITIAL>exec_with_return<PREBODY>  exec_with_return
<PREBODY>\(<BODY>                   '('
<PREBODY>(?s:.)<.>                  skip()
<BODY,PARENS>\(<>PARENS>            skip()
<PARENS>\)<<>                       skip()
<BODY>\)<INITIAL>                   ')'
<BODY,PARENS>{string}<.>            skip()
<BODY,PARENS>{skip_string}<.>       anything
<BODY,PARENS>{char}<.>              skip()
<BODY,PARENS,PREBODY>{ws}<.>        skip()
<BODY,PARENS>{name}<.>              skip()
<BODY,PARENS>{any}<.>               skip()
{string}                            anything
{skip_string}                       anything
{char}                              anything
{ws}                                anything
{name}                              anything
{any}                               anything
%%
