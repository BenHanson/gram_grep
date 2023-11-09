%token catch anything
%x PREPARAMS PARAMS PARENS PREBODY BODY BRACES
%%
start: catch '(' ')' '{' '}';
%%
any (?s:.)
char '([^'\\]|\\.)+'
name [A-Z_a-z][0-9A-Z_a-z]*
string \"([^"\\]|\\.)*\"|R\"\((?s:.)*?\)\"
ws [ \t\r\n]+|\/\/.*|"/*"(?s:.)*?"*/"
%%
<INITIAL>catch<PREPARAMS>   catch

<PREPARAMS>\(<PARAMS>       '('
<PREPARAMS>(?s:.)<.>        skip()
<PARAMS,PARENS>\(<>PARENS>  skip()
<PARENS>\)<<>               skip()
<PARAMS>\)<PREBODY>         ')'
<PARAMS,PARENS>{string}<.>  skip()
<PARAMS,PARENS>{char}<.>    skip()
<PARAMS,PARENS>{ws}<.>      skip()
<PARAMS,PARENS>{name}<.>    skip()
<PARAMS,PARENS>{any}<.>     skip()

<PREBODY>\{<BODY>         '{'
<PREBODY>(?s:.)<.>        skip()
<BODY,BRACES>\{<>BRACES>  skip()
<BRACES>\}<<>             skip()
<BODY>\}<INITIAL>         '}'
<BRACES,BODY>{string}<.>  skip()
<BRACES,BODY>{char}<.>    skip()
<BRACES,BODY>{ws}<.>      skip()
<BRACES,BODY>{name}<.>    skip()
<BRACES,BODY>{any}<.>     skip()

{string}                  anything
{char}                    anything
{ws}                      anything
{name}                    anything
{any}                     anything
%%
