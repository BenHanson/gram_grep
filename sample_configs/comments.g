// Note that this lexer is now targeted at C++ only.
// Previously it also worked for C# code.
%%
%%
%%
\/\/.*|\/\*(?s:.)*?\*\/ 1
\"([^"\\]|\\.)*\"       skip()
R\"\((?s:.)*?\)\"       skip()
%%
