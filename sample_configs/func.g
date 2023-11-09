%token ',' FuncName Integer Name String
%%
start: func '(' params ')';
func: FuncName;
params: %empty |
        param |
        params ',' param;
param: Name | Integer | String | Name '!=' Integer;
%%
%%
\(                                 '('
\)                                 ')'
,                                  ','
\d+                                Integer
!=                                 '!='
\"([^"\\\r\n]|\\.)*\"              String
[ \t\r\n]+|"/*"(?s:.)*?"*/"|"//".* skip()
Special                            FuncName
[_A-Za-z][_0-9A-Za-z]*             Name
%%
