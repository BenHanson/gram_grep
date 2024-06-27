%token CREATE Integer Name NVARCHAR Param PROCEDURE
%%
proc: CREATE PROCEDURE Name '.' Name param_list;
proc: CREATE PROCEDURE Name '.' Name '(' param_list ')';
param_list: param | param_list ',' param;
param: Param type;
type: Name;
type: Name Name;
type: Name '(' Integer ')';
type: NVARCHAR '(' Integer ')' {}
%%
%%
\.                        '.'
,                         ','
\(                        '('
\)                        ')'
(?i:CREATE)               CREATE
(?i:PROCEDURE)            PROCEDURE
(?i:NVARCHAR)             NVARCHAR
\d+                       Integer
[A-Z_a-z][0-9A-Z_a-z]*    Name
@[A-Z_a-z][0-9A-Z_a-z]*   Param
'([^']|'')*'              skip()
\s+|--.*|"/*"(?s:.)*?"*/" skip()
%%
