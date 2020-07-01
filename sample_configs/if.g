%token if anything
%x PREBODY BODY PARENS
%%
start: if '(' ')';
%%
any .{+}[\r\n]
char '([^'\\]|\\.)+'
name [A-Z_a-z][0-9A-Z_a-z]*
string ["]([^"\\]|\\.)*["]|R["][(](?s:.)*?[)]["]
ws [ \t\r\n]+|[/][/].*|[/][*].{+}[\r\n]*?[*][/]
%%
<INITIAL>if<PREBODY>                if
<PREBODY>[(]<BODY>                  '('
<PREBODY>.{+}[\r\n]<.>              skip()
<BODY,PARENS>[(]<>PARENS>           skip()
<PARENS>[)]<<>                      skip()
<BODY>[)]<INITIAL>                  ')'
<BODY,PARENS>{string}<.>            skip()
<BODY,PARENS>{char}<.>              skip()
<BODY,PARENS>{ws}<.>                skip()
<BODY,PARENS>{name}<.>              skip()
<BODY,PARENS>{any}<.>               skip()
{string}                            anything
{char}                              anything
{ws}                                anything
{name}                              anything
{any}                               anything
%%
