// Locate a function name, param list and body
// Note that we filter out class, struct and namespace
// in order to match any embeded blocks inside those constructs.
%captures
%token anything Name '<<'
%consume anything '<<'
%x BODY BRACES CHEVRONS PARAMS PARENS TPARAMS
%%
start: (name) opt_template_params ('(' ')') opt_qualifiers opt_param_list ('{' '}');
opt_template_params: %empty | '<' '>';
opt_qualifiers: %empty | opt_qualifiers qualifier;
qualifier: '&' | '&&' | 'const' | 'final' | 'override' | 'volatile' | 'throw' '(' ')';
opt_param_list: %empty | ':' param_list;
param_list: param | param_list ',' param;
param: name '(' ')' | name '{' '}';
name: Name | name '::' Name;
%%
any (?s:.)
char '([^'\\]|\\.)+'
name [A-Z_a-z][0-9A-Z_a-z]*
string \"([^"\\]|\\.)*\"|R\"\((?s:.)*?\)\"
ws [ \t\r\n]+|\/\/.*|"/*"(?s:.)*?"*/"
%%
(class|struct|namespace|union)\s+{name}?[^;,>{]*\{? skip()
extern\s*["]C["]\s*\{     skip()

<INITIAL>\<<TPARAMS>            '<'
<TPARAMS,CHEVRONS>\(<>CHEVRONS> skip()
<CHEVRONS>\><<>                 skip()
<TPARAMS>\><INITIAL>            '>'
<CHEVRONS,TPARAMS>{string}<.>   skip()
<CHEVRONS,TPARAMS>{char}<.>     skip()
<CHEVRONS,TPARAMS>{ws}<.>       skip()
<CHEVRONS,TPARAMS>{name}<.>     skip()
<CHEVRONS,TPARAMS>{any}<.>      skip()

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

,                         ','
&                         '&'
&&                        '&&'
:                         ':'
::                        '::'
<<                        '<<'
#.*                       skip()
const                     'const'
final                     'final'
override                  'override'
throw                     'throw'
volatile                  'volatile'
{string}                  anything
{char}                    anything
{name}                    Name
{ws}                      skip()
{any}                     anything
%%
