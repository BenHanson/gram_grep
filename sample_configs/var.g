%captures
%token Float Hex Integer Keyword Name String Using
%consume Keyword Using
%%
start: type opt_template (Name) opt_assign ';';
opt_template: %empty | '<' name_list '>';
name_list: name | name_list ',' name;
type: name opt_ptr_ref;
opt_ptr_ref: %empty | opt_ptr_ref '*'| opt_ptr_ref '&';
name: Name | name '::' Name;
opt_assign: %empty | '=' value;
value: 'false'
     | Float
     | Hex
     | Integer
     | name
     | 'nullptr'
     | String
     | 'true';
%%
name [A-Z_a-z]\w*
%%
;                     ';'
,                     ','
&                     '&'
\*                    '*'
<                     '<'
>                     '>'
=                     '='
::                    '::'
#{name}               Keyword
break                 Keyword
class                 Keyword
continue              Keyword
delete                Keyword
else                  Keyword
enum                  Keyword
false                 'false'
goto                  Keyword
namespace             Keyword
new                   Keyword
nullptr               'nullptr'
return                Keyword
struct                Keyword
throw                 Keyword
true                  'true'
using                 Using
VTS_[0-9A-Z_]*        Keyword
{name}                Name
0x[\dA-Fa-f]+         Hex
-?\d+                 Integer
[-+]?([0-9]+(\.[0-9]*)?|\.[0-9]+) Float
\"([^"\\\r\n]|\\.)*\" String
R\"\((?s:.)*?\)\"     String
\s+                   skip()
\/\/.*                skip()
"/*"(?s:.)*?"*/"      skip()
%%
