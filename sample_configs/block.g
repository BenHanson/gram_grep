// Locate a top level braced block (i.e. function bodies)
// Note that we filter out class, struct and namespace
// in order to match any embeded blocks inside those constructs.
%token Name anything
%x BODY BRACES
%%
start: '{' '}';
%%
any (?s:.)
char '([^'\\]|\\.)+'
name [A-Z_a-z][0-9A-Z_a-z]*
string \"([^"\\]|\\.)*\"|R\"\((?s:.)*?\)\"
ws [ \t\r\n]+|\/\/.*|"/*"(?s:.)*?"*/"
%%
(class|struct|namespace|union)\s+{name}?[^;{]*\{ skip()
extern\s*["]C["]\s*\{     skip()

<INITIAL>\{<BODY>         '{'
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
{name}                    anything
{ws}                      anything
{any}                     anything
%%
