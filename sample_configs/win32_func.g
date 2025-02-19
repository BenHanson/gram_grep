/*
This gram_grep configuration file is for scraping Win32 function prototypes from the Windows SDK.
The Windows SDK files can be found at:
C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0 (for example).
Currently we are not interested in the cppwinrt and winrt directories so these can be filtered out by using -x *\cppwinrt\*;*\winrt\*
*/
%captures
%consume PREPROC
%token BLOCK NAME IFDEF PREPROC
%x BLOCK BRACE BODY BRACKETS PARAMS
%%
// Function prototype grammar:
start: (NAME) '(' params ')' opt_qualifiers opt_ifdef terminate;
params: %empty | param_list;
param_list: (param) | param_list ',' (param);
param: item | param item;
item: NAME | '*' | '...' | '(' params ')' '(' params ')';
opt_qualifiers: %empty | opt_qualifiers qualifier;
qualifier: '&'
         | '&&'
         | 'const'
         | 'final'
         | 'noexcept'
         | 'override'
         | 'throw' '(' ')'
         | 'try'
         | 'volatile';
opt_ifdef: %empty | IFDEF;
terminate: ';' | BLOCK;
%%
/* Regex macros: */
id [A-Z_a-z]\w*
%%
 /*Lexer:*/
;        ';'
&        '&'
&&       '&&'
const    'const'
final    'final'
noexcept 'noexcept'
override 'override'
throw    'throw'
try      'try'
volatile 'volatile'

<INITIAL,PARAMS>{id}<.>      NAME

 /* Consume macros used in function parameters */
<PARAMS>{id}_\(<BODY>        skip()
<BODY,BRACKETS>\(<>BRACKETS> skip()
<BRACKETS>\)<<>              skip()
<BODY>\)<PARAMS>             skip()
<BODY,BRACKETS>(?s:.)<.>     skip()

 /* Consume nested braces */
<INITIAL>\{<BLOCK>           skip()
<BLOCK,BRACE>\{<>BRACE>      skip()
<BRACE>\}<<>                 skip()
<BLOCK>\}<INITIAL>           BLOCK
<BLOCK,BRACE>(?s:.)<.>       skip()

<INITIAL>\(<PARAMS>                  '('
<PARAMS>\*<.>                        '*'
<PARAMS>,<.>                         ','
<PARAMS>"..."<.>                     '...'
<PARAMS>\)<INITIAL>                  ')'

 /* Tokens we want to discard */

 /* String */
<INITIAL,PARAMS>\"([^"\\]|\\.)*\"<.> skip()
 /* extern "C" { */
extern\s+\"C\"\s*\{                  skip()
 /* Whitespace */
<INITIAL,PARAMS>\s+<.>               skip()
 /* Multiline comment */
<INITIAL,PARAMS>"/*"(?s:.)*?"*/"<.>  skip()
 /* Single line comment */
<INITIAL,PARAMS>"//".*<.>            skip()

 /* Use a real id so that defines are not included as part of a match */
#define\s+(.|\\\r?\n)*               PREPROC
#if[ ]defined.*\s+noexcept\s*#endif  IFDEF
 /* Use a real id so that directives are not included as part of a match */
#.*                                  PREPROC
%%
