%token String VString
%%
list: String { match = substr($1, 1, 1); };
list: VString { match = substr($1, 2, 1); };
list: list '+' String { match += substr($3, 1, 1); };
list: list '+' VString { match += substr($3, 2, 1); };
%%
ws [ \t\r\n]+
%%
[+]                                    '+'
["]([^"\\]|\\.)*["]                    String
@["]([^"]|["]["])*["]                  VString
'([^'\\]|\\.)*'                        skip()
{ws}|[/][/].*|[/][*].{+}[\r\n]*?[*][/] skip()
%%
