%token INSERT INTO Name String VALUES
%consume String
%%
start: insert;
insert: INSERT into name VALUES;
into: INTO | %empty;
name: Name | Name '.' Name | Name '.' Name '.' Name;
%%
%%
(?i:INSERT)                                           INSERT
(?i:INTO)                                             INTO
(?i:VALUES)                                           VALUES
\.                                                    '.'
(?i:[a-z_][a-z0-9@$#_]*|\[[a-z_][a-z0-9@$#_]*[ ]*\])  Name
'([^']|'')*'                                          String
\s+|--.*|"/*"(?s:.)*?"*/"                             skip()
%%
