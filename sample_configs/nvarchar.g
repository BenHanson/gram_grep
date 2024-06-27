%token DECLARE Error NVARCHAR
%%
nvarchar: DECLARE Error NVARCHAR;
nvarchar: NVARCHAR {}
%%
%%
(?i:DECLARE)              DECLARE
@(?i:Error[a-z_0-9]*)     Error
(?i:NVARCHAR)             NVARCHAR
'([^']|'')*'              skip()
\s+|--.*|"/*"(?s:.)*?"*/" skip()
%%
