// NOTE: does not recognise functions inside structs or classes currently.
// Could recognise those separately and search inside.
%captures
%token Any ATTR Name
%x BODY BRACES FBODY NAME PARAMS PARENS
%%
start: (Name) '(' ')' trailing attr_list init_list semi_body;
trailing: %empty
        | trailing trail_list;
trail_list: '&'
          | '&&'
          | 'const'
          | 'noexcept'
          | 'volatile';
attr_list: %empty
         | attr_list ATTR;
init_list: %empty
         | ':' param_list;
param_list: Name '(' ')'
          | param_list ',' Name '(' ')';
semi_body: ';' | '{' '}';
%%
any (?s:.)
char '([^'\\]|\\.)+'
name [A-Z_a-z][0-9A-Z_a-z]*
string ["]([^"\\]|\\.)*["]|R["][(](?s:.)*?[)]["]
ws [ \t\r\n]+|[/][/].*|[/][*](?s:.)*?[*][/]
%%
<FBODY>;<INITIAL>                      ';'
<FBODY>:<.>                            ':'
<FBODY>,<.>                            ','
<FBODY>&<.>                            '&'
<FBODY>&&<.>                           '&&'
<FBODY>\[\[.*?\]\]<.>                  ATTR
<FBODY>const<.>                        'const'
<FBODY>noexcept<.>                     'noexcept'
<FBODY>volatile<.>                     'volatile'
<INITIAL,NAME,FBODY>[A-Z_a-z]\w*<NAME> Name

<INITIAL,NAME>[(]<PARAMS>              '('
<PARAMS,PARENS>[(]<>PARENS>            skip()
<PARENS>[)]<<>                         skip()
<PARAMS>[)]<FBODY>                     ')'
<PARAMS,PARENS>{string}<.>             skip()
<PARAMS,PARENS>{char}<.>               skip()
<PARAMS,PARENS>{name}<.>               skip()
<PARAMS,PARENS>{any}<.>                skip()

<FBODY>[{]<BODY>                       '{'
<BODY,BRACES>[{]<>BRACES>              skip()
<BRACES>[}]<<>                         skip()
<BODY>[}]<INITIAL>                     '}'
<BODY,BRACES>{string}<.>               skip()
<BODY,BRACES>{char}<.>                 skip()
<BODY,BRACES>{name}<.>                 skip()
<BODY,BRACES>{any}<.>                  skip()

<NAME,FBODY>{string}<INITIAL>          skip()
<NAME,FBODY>{char}<INITIAL>            skip()
<NAME,FBODY>{any}<INITIAL>             Any
<*>{ws}<.>                             skip()
%%
