// Lexer for counting non-blank lines
// that are outside of C style comments
// for C++ files.
// Example usage:
// gram_grep -r --include='*.hpp' --config=line_count.g .
%%
%%
%%
^\r?\n                   skip()
"//".*                   skip()
"/*"(?s:.)*?"*/"         skip()
[ \t]+                   skip()
[ \t]*[^/\r\n]+          1
%%
