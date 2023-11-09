/*
NOTE: in order to succesfully find strings it is necessary to filter out comments and chars.
As a subtlety, comments could contain apostrophes (or even unbalanced double quotes in
an extreme case)!
*/
%token RawString String
%%
start: 'format' '(' list ')';
list: String;
list: RawString;
list: list String;
list: list RawString;
%%
ws [ \t\r\n]+
%%
\(                                     '('
\)                                     ')'
std::format                            'format'
["]([^"\\]|\\.)*["]                    String
R["][(](?s:.)*?[)]["]                  RawString
'([^'\\]|\\.)*'                        skip()
{ws}|[/][/].*|[/][*].{+}[\r\n]*?[*][/] skip()
%%
