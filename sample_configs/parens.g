// Locate a top level braced block (i.e. function bodies)
// Note that we filter out class, struct and namespace
// in order to match any embeded blocks inside those constructs.
%token anything
%consume anything
%x PARAMS PARENS
%%
start: '(' ')';
%%
any (?s:.)
char '([^'\\]|\\.)+'
name [A-Z_a-z][0-9A-Z_a-z]*
string \"([^"\\]|\\.)*\"|R\"\((?s:.)*?\)\"
ws [ \t\r\n]+|\/\/.*|"/*"(?s:.)*?"*/"
%%
<INITIAL>\(<PARAMS>         '('
<PARAMS,PARENS>\(<>PARENS>  skip()
<PARENS>\)<<>             skip()
<PARAMS>\)<INITIAL>         ')'
<PARENS,PARAMS>{string}<.>  skip()
<PARENS,PARAMS>{char}<.>    skip()
<PARENS,PARAMS>{ws}<.>      skip()
<PARENS,PARAMS>{name}<.>    skip()
<PARENS,PARAMS>{any}<.>     skip()

{string}                  anything
{char}                    anything
{name}                    anything
{ws}                      anything
{any}                     anything
%%
