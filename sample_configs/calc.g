%token INTEGER
%left '+' '-'
%left '*' '/'
%precedence UMINUS

%% //Grammar rules
start : exp ';';
exp : exp '+' exp;
exp : exp '-' exp;
exp : exp '*' exp;
exp : exp '/' exp;
exp : '(' exp ')';
exp : '-' exp %prec UMINUS;
exp : INTEGER;

//Regex macros
%%

%%

[0-9]+	INTEGER

\+	'+'
-	'-'
\*	'*'
\/	'/'
\(	'('
\)	')'
;   ';'
\s+ skip()

%%