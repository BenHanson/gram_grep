%token AS CREATE Integer Name NULL NVARCHAR TABLE TYPE
%%
start: CREATE TYPE Name '.' Name AS TABLE '(' param_list ')';
param_list: param | param_list ',' param;
param: Name type opt_null;
type: Name;
type: Name Name;
type: Name '(' Integer ')';
type: Name '(' Integer ',' Integer ')';
type: NVARCHAR '(' Integer ')' {}
opt_null: %empty | NULL;
%%
%%
\.                        '.'
,                         ','
\(                        '('
\)                        ')'
(?i:AS)                   AS
(?i:CREATE)               CREATE
(?i:TABLE)                TABLE
(?i:TYPE)                 TYPE
(?i:NULL)                 NULL
(?i:NVARCHAR)             NVARCHAR
\d+                       Integer
[A-Z_a-z][0-9A-Z_a-z]*    Name
'([^']|'')*'              skip()
\s+|--.*|"/*"(?s:.)*?"*/" skip()
%%
