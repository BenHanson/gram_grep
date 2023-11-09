%token Code EntityRef GUID Name Param
%%
param: Code '=' Param;
param: GUID '=' Param;
param: EntityRef '=' Param;
param: Name '=' Param {};
%%
ws [ \t\r\n]+
%%
=                            '='
(?:Code)                     Code
(?i:EntityRef)               EntityRef
(?:GUID)                     GUID
'\{\d+\}'                    Param
[A-Z_a-z]\w*                 Name
{ws}|"//".*|"/*"(?s:.)*?"*/" skip()
%%
