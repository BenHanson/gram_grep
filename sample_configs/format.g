%token Integer Name RawString String
%%
start: '(' format list ')' '.' 'str' '(' ')'
    /* Erase the first "(" and the trailing ".str()" */
    { erase($1);
    erase($5, $8); };
start: 'str' '(' format list ')'
    /* Erase "str(" */
    { erase($1, $2); };
format: 'boost' '::' 'format' '(' string ')'
    /* Replace "boost" with "std" */
    /* Replace the format specifiers within the strings */
    { replace($1, 'std');
    replace_all($5, '%(\d+[Xdsx])', '{:$1}');
    replace_all($5, '%((?:\d+)?\.\d+f)', '{:$1}');
    replace_all($5, '%x', '{:x}');
    replace_all($5, '%[ds]', '{}');
    replace_all($1, '%%', '%');
    erase($6); };
string: String;
string: RawString;
string: string String;
string: string RawString;
list: %empty;
list: list '%' param
    /* Replace "%" with ", " */
    { replace($2, ', '); };
param: Integer;
param: name
    /* Replace any trailing ".c_str()" calls with "" */
    { replace_all($1, '\.c_str\(\)$', ''); };
name: Name opt_func
    | name deref Name opt_func;
opt_func: %empty | '(' opt_param ')';
deref: '.' | '->' | '::';
opt_param: %empty | Integer | name;
%%
%%
\(                              '('
\)                              ')'
\.                              '.'
%                               '%'
::                              '::'
->                              '->'
boost                           'boost'
format                          'format'
str                             'str'
-?\d+                           Integer
\"([^"\\]|\\.)*\"               String
R\"\((?s:.)*?\)\"               RawString
'([^'\\]|\\.)*'                 skip()
[_a-zA-Z][_0-9a-zA-Z]*          Name
\s+|\/\/.*|\/\*.{+}[\r\n]*?\*\/ skip()
%%
