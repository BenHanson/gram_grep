%token FLAG
%%
start: 'AfxMessageBox' '(' 'e.what' opt_flags ')'
{
    replace($1, 'tfb::exceptionWait');
	insert($3, '"", ');
	erase($4);
};
opt_flags: %empty | ',' flags;
flags: FLAG
     | flags '|' FLAG;
%%
ws [ \t\r\n]+
%%
\(                           '('
\)                           ')'
\|                           '|'
,                            ','
AfxMessageBox                'AfxMessageBox'
MB_[0-9A-Z_]+                FLAG
\"([^"\\\r\n]|\\.)*\"        'e.what'
R\"\((?s:.)*?\)\"            skip()
R\"_\((?s:.)*?\)_\"          skip()
'([^'\\\r\n]|\\.)*'          skip()
{ws}|"//".*|"/*"(?s:.)*?"*/" skip()
%%
