// Locate a function name, param list and body
// Note that we filter out class, struct and namespace
// in order to match any embeded blocks inside those constructs.
%captures
%token anything Name
%consume anything
%x BODY BRACES PARAMS PARENS
%%
start: (name_or_op) opt_template_params ('(' ')') opt_qualifiers opt_param_list ('{' '}');
opt_template_params: %empty | '<' name '>';
opt_qualifiers: %empty | opt_qualifiers qualifier;
qualifier: '&' | '&&' | 'const' | 'final' | 'noexcept' | 'override' | 'throw' '(' ')' | 'volatile';
opt_param_list: %empty | ':' param_list;
param_list: param | param_list ',' param;
param: name '(' ')' | name '{' '}';
name_or_op: Name '::' opt_tilde Name | operator;
opt_tilde: %empty | '~';
name: Name | name '::' Name;
operator: 'operator' op;
op: name | '+' | '-' | '*'| '/' | '%' | '^' | '&' | '|' | '~' | '!' | '='
  | '<' | '>' | '+=' | '-=' | '*=' | '/=' | '%=' | '^=' | '&=' | '|='
  | '<<' | '>>' | '>>=' | '<<=' | '==' | '!=' | '<=' | '>=' | '<=>'
  | '&&' | '||' | '++' | '--' | ',' | '->*' | '->' | '(' ')' | '[' ']';
%%
any (?s:.)
char '([^'\\]|\\.)+'
name [A-Z_a-z][0-9A-Z_a-z]*
string \"([^"\\]|\\.)*\"|R\"\((?s:.)*?\)\"
ws [ \t\r\n]+|\/\/.*|"/*"(?s:.)*?"*/"
%%
(class|struct|namespace|union)\s+{name}?[^;,>{]*\{? skip()
extern\s*["]C["]\s*\{     skip()

<INITIAL>\(<PARAMS>         '('
<PARAMS,PARENS>\(<>PARENS>  skip()
<PARENS>\)<<>               skip()
<PARAMS>\)<INITIAL>         ')'
<PARENS,PARAMS>{string}<.>  skip()
<PARENS,PARAMS>{char}<.>    skip()
<PARENS,PARAMS>{ws}<.>      skip()
<PARENS,PARAMS>{name}<.>    skip()
<PARENS,PARAMS>{any}<.>     skip()

<INITIAL>\{<BODY>         '{'
<BODY,BRACES>\{<>BRACES>  skip()
<BRACES>\}<<>             skip()
<BODY>\}<INITIAL>         '}'
<BRACES,BODY>{string}<.>  skip()
<BRACES,BODY>{char}<.>    skip()
<BRACES,BODY>{ws}<.>      skip()
<BRACES,BODY>{name}<.>    skip()
<BRACES,BODY>{any}<.>     skip()

\+                        '+'
-                         '-'
\*                        '*'
\/                        '/'
%                         '%'
\^                        '^'
&                         '&'
\|                        '|'
~                         '~'
!                         '!'
=                         '='
<                         '<'
>                         '>'
\+=                       '+='
-=                        '-='
\*=                       '*='
\/=                       '/='
%=                        '%='
\^=                       '^='
&=                        '&='
\|=                       '|='
<<                        '<<'
>>                        '>>'
>>=                       '>>='
<<=                       '<<='
==                        '=='
!=                        '!='
<=                        '<='
>=                        '>='
<=>                       '<=>'
&&                        '&&'
\|\|                      '||'
\+\+                      '++'
--                        '--'
,                         ','
->\*                      '->*'
->                        '->'
\[                        '['
\]                        ']'
:                         ':'
::                        '::'
#.*                       skip()
const                     'const'
final                     'final'
noexcept                  'noexcept'
operator                  'operator'
override                  'override'
throw                     'throw'
volatile                  'volatile'
{string}                  anything
{char}                    anything
{name}                    Name
{ws}                      skip()
{any}                     anything
%%
