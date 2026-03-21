%%
%%
name [A-Z_a-z]\w*
%%
<\/{name}>      1
<{name}\/>      2
<.*?>           3
\s+             skip()
<!--(?s:.)*?--> skip()
[^<]+           4
%%
