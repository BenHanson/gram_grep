%token Bool CHAR CONST EXTERN INT LONG Name NULLPTR Number
%token SimpleType SIZE_T STATIC STD TYPEDEF UNSIGNED WindowsType
%%
start: decl;
decl: type list ';';
decl: prefix opt_const type list2 ';';
prefix: EXTERN | STATIC | TYPEDEF;
opt_const: CONST | %empty;
type: WindowsType
  | SimpleType
  | SIZE_T
  | STD '::' SIZE_T
  | CHAR
  | UNSIGNED CHAR
  | INT
  | UNSIGNED INT
  | LONG
  | UNSIGNED LONG
  | name_list stars;
name_list: Name
  | name_list '::' Name;
stars: '*'
  | stars '*';
list: item
  | list ',' item;
item: Name {};
item: Name '=' value;
list2: item2
  | list2 ',' item2;
item2: Name;
item2: Name '=' value;
value: Bool
  | Number
  | NULLPTR;
%%
NAME  [_A-Za-z][_0-9A-Za-z]*
%%
=                                               '='
,                                               ','
;                                               ';'
[*]                                             '*'
::                                              '::'
true|TRUE|false|FALSE                           Bool
char                                            CHAR
const                                           CONST
extern                                          EXTERN
int((32|64)_t)?                                 INT
interface\s+{NAME}\s+[{][^}]*[}];               skip()
long                                            LONG
nullptr                                         NULLPTR
size_t                                          SIZE_T
static                                          STATIC
std                                             STD
typedef                                         TYPEDEF
BOOL|BSTR|BYTE|COLORREF|D?WORD|DWORD_PTR        WindowsType
DROPEFFECT|HACCEL|HANDLE|HBITMAP|HBRUSH         WindowsType
HCRYPTHASH|HCRYPTKEY|HCRYPTPROV|HCURSOR|HDBC    WindowsType
HICON|HINSTANCE|HMENU|HMODULE|HSTMT|HTREEITEM   WindowsType
HWND|LPARAM|LPCTSTR|LPDEVMODE|POSITION|SDWORD   WindowsType
SQLHANDLE|SQLINTEGER|SQLSMALLINT|UINT|U?INT_PTR WindowsType
UWORD|WPARAM                                    WindowsType
bool|double|float                               SimpleType
unsigned                                        UNSIGNED
{NAME}                                          Name
-?\d+([.]\d+)?                                  Number
[ \t\r\n]+|[/][/].*|[/][*].{+}[\r\n]*?[*][/]    skip()
%%
