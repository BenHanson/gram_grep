%captures
%token Name Token
%%
start: (Name) '(' params ')' '{';
params: %empty
      | parms_oom;
parms_oom: param
         | parms_oom ',' param;
param: Name
      | param Name;
%%
whitespace \s+|\/\/.*|\/\*(?s:.)*?\*\/
%%
\(                          '('
\)                          ')'
\{                          '{'
,                           ','
if|for|foreach|switch|while Token
[A-Z_a-z]\w*                Name
["]([^"\\]|\\.)*["]         Token
@["]([^"]|["]["])*["]       Token
'([^'\\]|\\.)*'             Token
{whitespace}                skip()
(?s:.)                      Token
%%
