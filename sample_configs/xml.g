// Based on http://www.goldparser.org/grammars/files/xml.zip
%token AttributeValue Name Text
%x DATA
%%
XmlDoc: OptHeader Objects;

OptHeader: %empty | '<?' Name Attributes '?>';

Objects: Object | Objects Object;
Object: StartTag Content EndTag | UnaryTag;

StartTag: '<' Name Attributes '>';
Content: %empty | Text | Objects;
EndTag: '</' Name '>';
UnaryTag: '<' Name Attributes '/>';

Attributes: %empty | Attributes Attribute;
Attribute: Name '=' AttributeValue;
%%
Printable    [\n -~]
Letter       [A-Za-z]
Whitespace   [\t-\r \xA0]
Digit        \d
Alphanumeric \w{-}_
Name         {Letter}{Alphanumeric}*
%%
<\?                 '<?'
\?>                 '?>'
<                   '<'
<INITIAL>><DATA>    '>'
<DATA>\s+<.>        skip()
<DATA>[^<]+<.>      Text
<DATA>\<<INITIAL>   '<'
<DATA>\<\/<INITIAL> '</'
<\/                 '</'
\/>                 '/>'
=                   '='
<!--(?s:.)*?-->     skip()
\".*?\"             AttributeValue
'.*?'               AttributeValue
{Name}(:{Name})*    Name
\s+                 skip()
%%
