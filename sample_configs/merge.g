%token AS Integer INTO MERGE Name PERCENT TOP USING
%%
merge: MERGE opt_top opt_into name opt_alias USING;
opt_top: %empty | TOP '(' Integer ')' opt_percent;
opt_percent: %empty | PERCENT;
opt_into: %empty | INTO;
name: Name | Name '.' Name | Name '.' Name '.' Name;
opt_alias: %empty | opt_as Name;
opt_as: %empty | AS;
%%
%%
(?i:AS)                                               AS
(?i:INTO)                                             INTO
(?i:MERGE)                                            MERGE
(?i:PERCENT)                                          PERCENT
(?i:TOP)                                              TOP
(?i:USING)                                            USING
[.]                                                   '.'
[(]                                                   '('
[)]                                                   ')'
\d+                                                   Integer
(?i:[a-z_][a-z0-9@$#_]*|\[[a-z_][a-z0-9@$#_]*[ ]*\])  Name
\s+                                                   skip()
%%
