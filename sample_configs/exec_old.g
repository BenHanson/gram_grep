/*
NOTE: in order to succesfully find strings it is necessary to filter out comments and chars.
As a subtlety, comments could contain apostrophes (or even unbalanced double quotes in
an extreme case)!
*/
%token Name RawString String
%%
start: 'exec' '(' string_or_name ',' '&';

string_or_name: string | Name;
string: String { match = substr($1, 1, 1); };
string: RawString { match = substr($1, 3, 2); };
string: string String { match += substr($2, 1, 1); };
string: string RawString { match += substr($2, 3, 2); };
%%
ws [ \t\r\n]+
%%
\(                                     '('
,                                      ','
&                                      '&'
["]([^"\\]|\\.)*["]                    String
R["][(](?s:.)*?[)]["]                  RawString
exec                                   'exec'
[A-Z_a-z]\w*                           Name
'([^'\\]|\\.)*'                        skip()
{ws}|[/][/].*|[/][*].{+}[\r\n]*?[*][/] skip()
%%
