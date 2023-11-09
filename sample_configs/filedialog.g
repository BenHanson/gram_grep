%token CFileDialog Char Name Number String
%%
start: CFileDialog Name '(' params ')';
/*params: param ',' param ',' param ',' param ',' param ',' param;*/
params: param | params ',' param;
param: func | Number | String | name_list;
name_list: Name | name_list '|' Name;
func: Name '.' Name '(' ')';
%%
ws [ \t\r\n]+
%%
\(										'('
\)										')'
\.										'.'
,										','
\|										'|'
CFileDialog								CFileDialog
[_A-Za-z][_0-9A-Za-z]*					Name
\d+(UL)?								Number
'([^'\\\r\n]|\\.)*'						Char
\"([^"\\\r\n]|\\.)*\"					String
{ws}|"//".*|"/*"(?s:.)*?"*/"			skip()
%%
