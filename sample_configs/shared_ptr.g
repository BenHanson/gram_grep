%token Integer Name String RawString
%%
start: '=' 'std' '::' 'shared_ptr' '<' name '>'
	'(' 'new' name ')'
	{
		replace($4, 'make_shared');
		erase($9, $10);
	};
name: Name | name '::' Name;
%%
%%
\(	'('
\)	')'
<	'<'
>	'>'
=	'='
::	'::'
new                         'new'
std                         'std'
shared_ptr                  'shared_ptr'
-?\d+                       Integer
\"([^"\\\r\n]|\\.)*\"       String
R\"\((?s:.)*?\)\"           RawString
'([^'\\\r\n]|\\.)*'         skip()
[_a-zA-Z][_0-9a-zA-Z]*      Name
\s+|"//".*|"/*"(?s:.)*?"*/" skip()
%%
