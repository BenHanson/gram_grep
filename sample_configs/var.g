%captures
%token Name Keyword String Whitespace
%%
start: Name opt_template Whitespace (Name) opt_ws ';';
opt_template: %empty | '<' name '>';
name: Name | name '::' Name;
opt_ws: %empty | Whitespace;
%%
name [A-Z_a-z]\w*
%%
;                     ';'
<                     '<'
>                     '>'
::                    '::'
#{name}               Keyword
break                 Keyword
CExtDllState          Keyword
CShellManager         Keyword
CWaitCursor           Keyword
continue              Keyword
delete                Keyword
enum                  Keyword
false                 Keyword
goto                  Keyword
namespace             Keyword
new                   Keyword
return                Keyword
throw                 Keyword
VTS_[0-9A-Z_]*        Keyword
{name}                Name
\"([^"\\\r\n]|\\.)*\" String
R\"\((?s:.)*?\)\"     String
\s+                   Whitespace
\/\/.*                skip()
"/*"(?s:.)*?"*/"      skip()
%%
