/* tiny C parser */
%token NUMBER
%token SYMBOL
%token STRING
%token VAR
%token IF
%token RETURN
%token WHILE
%token FOR
%token PRINTLN

%right '='
%left '<' '>'
%left '+' '-'
%left '*'
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%start program
%%
program: /* empty */
    | external_definitions
    ;

external_definitions:
      external_definition
    | external_definitions external_definition
    ;

external_definition:
      SYMBOL parameter_list block  /* fucntion definition */
    | VAR SYMBOL ';'
    | VAR SYMBOL '=' expr ';'
    | VAR SYMBOL '[' expr ']' ';';

parameter_list: '(' ')' | '(' symbol_list ')';

block: '{' local_vars statements '}';

local_vars: /* NULL */
    | VAR symbol_list ';';

symbol_list: SYMBOL
    | symbol_list ',' SYMBOL;

statements: statement
    | statements statement;

statement: expr ';'
    | block
    | IF '(' expr ')' statement %prec LOWER_THAN_ELSE
    | IF '(' expr ')' statement ELSE statement
    | RETURN expr ';'
    | RETURN ';'
    | WHILE '(' expr ')' statement
    | FOR '(' expr ';' expr ';' expr ')' statement;

expr: primary_expr
    | SYMBOL '=' expr
    | SYMBOL '[' expr ']' '=' expr
    | expr '+' expr
    | expr '-' expr
    | expr '*' expr
    | expr '<' expr
    | expr '>' expr;

primary_expr:
      SYMBOL
    | NUMBER
    | STRING
    | SYMBOL '[' expr ']'
    | SYMBOL '(' arg_list ')'
    | SYMBOL '(' ')'
    | '(' expr ')'
    | PRINTLN  '(' arg_list ')';

arg_list: expr
    | arg_list ',' expr;
%%
%%
=               '='
<               '<'
>               '>'
\+              '+'
-               '-'
\*              '*'
;               ';'
\[              '['
\]              ']'
\(              '('
\)              ')'
\{              '{'
\}              '}'
,               ','
else            ELSE
for             FOR
if              IF
println         PRINTLN
return          RETURN
var             VAR
while           WHILE
\d+             NUMBER
\"([^"]|\\.)*\" STRING
[A-Za-z]+       SYMBOL
%%
