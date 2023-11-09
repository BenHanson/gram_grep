/*
NOTE: in order to succesfully find strings it is necessary to filter out comments and chars.
As a subtlety, comments could contain apostrophes (or even unbalanced double quotes in
an extreme case)!
*/
%%
%%
ws [ \t\r\n]+
%%
\"([^"\\\r\n]|\\.)*\"        1
R\"\((?s:.)*?\)\"            1
R\"_\((?s:.)*?\)_\"          skip()
'([^'\\\r\n]|\\.)*'          skip()
{ws}|"//".*|"/*"(?s:.)*?"*/" skip()
%%
