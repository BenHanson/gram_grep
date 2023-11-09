%token Char Name String
%x BODY BRACES
%%
start: type_name '(' opt_params ')' trailing '{' '}';
opt_params: %empty | params;
params: param | params ',' param;
param: type_name;
type_name: item | type_name item;
item: 'const'| name | amp_star | '<' params '>';
name: Name | name '::' Name;
amp_star: '&' | '*';
trailing: opt_cv opt_ref;
opt_cv: %empty | 'const' | 'volatile';
opt_ref: %empty | '&'| '&&';
%%
char '([^'\\\r\n]|\\.)*'
string \"([^"\\\r\n]|\\.)*\"|R\"\((?s:.)*?\)\"
ws \s+|"//".*|"/*"(?s:.)*?"*/"
%%
\(                         '('
\)                         ')'
::                         '::'
,                          ','
<                          '<'
>                          '>'
&                          '&'
&&                         '&&'
\*                         '*'
const                      'const'
volatile                   'volatile'
~?[A-Z_a-z][0-9_A-Za-z]*   Name
{char}                     Char
{string}                   String
#[a-z]+([ \t]+[A-Z_a-z]+)* skip() /* directives mess up return types */
{ws}                       skip()
<INITIAL>\{<BODY>         '{'
<BODY,BRACES>\{<>BRACES>  skip()
<BRACES>\}<<>             skip()
<BODY>\}<INITIAL>         '}'
<BODY,BRACES>{char}<.>    skip()
<BODY,BRACES>{string}<.>  skip()
<BODY,BRACES>{ws}<.>      skip()
<BODY,BRACES>(?s:.)<.>    skip()
%%
