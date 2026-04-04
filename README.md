# gram_grep
Search text using a grammar, lexer, or straight regex. Chain searches for greater refinement.

See [http://benhanson.net/gram_grep.html](http://benhanson.net/gram_grep.html) for examples.

### Table of Contents

* [Building](#Building)
* [Examples](#Examples)
* [Command Line Switches](#Switches)
* [Scripting Grammar](#Scripting)

### Building

You will need a C++20 compatible compiler.

gram_grep has the following dependencies:

- <a href="https://www.boost.org/releases/latest/">Boost</a>
- <a href="https://github.com/BenHanson/lexertl17">lexertl17</a>
- <a href="https://github.com/BenHanson/parsertl17">parsertl17</a>
- <a href="https://github.com/BenHanson/wildcardtl">wildcardtl</a>

All dependencies are (thankfully) header only which greatly simplifies the build process.

#### Acquiring the Source Dependencies

```
git clone https://github.com/BenHanson/gram_grep.git
git clone https://github.com/BenHanson/parsertl17.git
git clone https://github.com/BenHanson/lexertl17.git
git clone https://github.com/BenHanson/wildcardtl.git
```

#### Setting BOOST_ROOT
All platforms now require that you set the BOOST_ROOT environment variable to the root of your boost source (e.g. `/home/ben/Dev/boost/boost_1_90_0`).

#### Running the Build command

- If on Windows, using a Developer Command Prompt, you can run `msbuild gram_grep.sln /property:Configuration=Release` from the `gram_grep` directory
- If on Linux you can run `make` from the `gram_grep` directory
- If you would like to use `cmake`, instead follow the below instructions

#### Building Using CMake
```
cd gram_grep
mkdir build
cd build
cmake ..
cmake --build .
```

### Examples

#### Printing a Reversed List

`sample_configs/rev.g`:

```
%token Name
%%
start: Name '::' list { print(format('{}::{}\n', acc, $1)); };
list: Name { acc = $1; };
list: list '::' Name { acc = format('{}::{}', $3, acc); };
%%
%%
::              '::'
[A-Z_a-z][-\w]* Name
%%
```

`test.txt`:

```
a::b::c::d
```

`gram_grep` command:

```
gram_grep --config=sample_configs\rev.g test.txt
```

Output:

```
d::c::b::a
```

### Switches

```
Pattern selection and interpretation:

    -E, --extended-regexp         PATTERNS are extended regular expressions
    -F, --fixed-strings           PATTERN is a string
    -G, --basic-regexp            PATTERNS are basic regular expressions
    -P, --perl-regexp             PATTERN is a Perl regular expression
    -e, --regexp=PATTERNS         use PATTERNS for matching
    -f, --file=FILE               take PATTERNS from FILE
    -i, --ignore-case             ignore case distinctions
        --no-ignore-case          do not ignore case distinctions (default)
    -w, --word-regexp             force PATTERN to match only whole words
    -x, --line-regexp             force PATTERN to match only whole lines

Miscellaneous

    -s, --no-messages             suppress error messages
    -v, --invert-match            select non-matching text
    -V, --version                 print version information and exit
        --help                    display this help and exit

Output control:

    -m, --max-count=NUM           stop after NUM matches
    -b, --byte-offset             print the byte offset with output lines
    -n, --line-number             print line number with output lines
        --line-buffered           flush output on every line
    -H, --with-filename           print the filename for each match
    -h, --no-filename             suppress the prefixing filename on output
        --label=LABEL             print LABEL as filename for standard input
    -o, --only-matching           show only the part of a line matching PATTERN
    -q, --quiet, --silent         suppress all normal output
        --binary-files=TYPE       assume that binary files are TYPE;
                                  TYPE is `binary', `text', or `without-match'
    -a, --text                    equivalent to --binary-files=text
    -I                            equivalent to --binary-files=without-match
    -d, --directories=ACTION      how to handle directories;
                                  ACTION is 'read', 'recurse', or 'skip'
    -r, --recursive               like --directories=recurse
    -R, --dereference-recursive   likewise, but follow all symlinks
        --include=GLOB            search only files that match GLOB (a file pattern)
        --exclude=GLOB            skip files that match GLOB
        --exclude-from=FILE       skip files that match any file pattern from FILE
        --exclude-dir=GLOB        skip directories that match GLOB
    -L, --files-without-match     print only names of FILEs containing no match
    -l, --files-with-matches      print only names of FILEs containing matches
    -c, --count                   print only a count of matches per FILE
    -T, --initial-tab             make tabs line up (if needed)
    -Z, --null                    print 0 byte after FILE name

Context control:

    -B, --before-context=NUM      print NUM lines of trailing context
    -A, --after-context=NUM       print NUM lines of leading context
    -C, --context=NUM             print NUM lines of output context
    -NUM                          same as --context=NUM
        --group-separator=SEP     print SEP on line between matches with context
        --no-group-separator=SEP  do not print separator for matches with context
        --color=[WHEN]
        --colour=[WHEN]           use markers to highlight the matching strings;
                                  WHEN is 'always', 'never', or 'auto'

gram_grep specific switches:

        --checkout=CMD            checkout command (include $1 for pathname)
        --config=CONFIG_FILE      search using config file
        --display-whole-match     display a multiline match
        --dump                    dump DFA regexp
        --dump-argv               dump command line arguments
        --dump-dot                dump DFA regexp in DOT format
        --exec=CMD                Executes the supplied command
        --extend-search           extend the end of the next match to be the end of the current match
        --flex-regexp             PATTERN is a flex style regexp
        --force-write             if a file is read only, force it to be writable
        --if=CONDITION            make search conditional
        --invert-match-all        only match if the search does not match at all
    -N, --line-number-parens      print line number in parenthesis with output lines
        --perform-output          output changes to matching file
    -p, --print=TEXT              print TEXT instead of line of match
        --print-script=SCRIPT     print result of SCRIPT instead of line of match   
        --replace=TEXT            replace match with TEXT
        --replace-script=SCRIPT   replace match with result of SCRIPT
        --return-previous-match   return the previous match instead of the current one
        --shutdown=CMD            command to run when exiting
        --startup=CMD             command to run at startup
        --summary                 show match count footer
        --utf8                    in the absence of a BOM assume UTF-8
    -W, --word-list=PATHNAME      search for a word from the supplied word list
        --writable                only process files that are writable
```

### Scripting

Scripting can be applied at both the regex level and the search grammar level. The indexes (<i>$n</i> syntax) refer to captures in regex mode and rule rhs item index (1 based) in search grammar mode.

#### Top level functions

* erase(\$*n*);
* erase(\$*from*, \$*to*);
* erase(\$*from*.second, \$*to*.first);
* insert(\$*n*, 'text');
* insert(\$*n*.second, 'text');
* match = \$*n*;
* match = substr(\$*n*, &lt;*omit from left*&gt;, &lt;*omit from right*&gt;);
* match += \$*n*;
* match += substr(\$*n*, &lt;*omit from left*&gt;, &lt;*omit from right*&gt;);
* print('text');
* replace(\$*n*, 'text');
* replace(\$*from*, \$*to*, 'text');
* replace(\$*from*.second, \$*to*.first, 'text');

#### Grammar

```
  cmd: Name '=' ret_function
     | Name '+=' ret_function
     | 'erase' '(' Index [ ',' Index ] ',' ret_function ')'
     | 'erase' '(' Index '.' first_second ','
                 Index '.' first_second ',' ret_function ')'
     | 'insert' '(' Index [ '.' 'second' ] ',' ret_function ')'
     | 'match' '=' Index
     | 'match' '=' 'substr' '(' Index ',' UInt ',' UInt ')'
     | 'match' '+=' Index
     | 'match' '+=' 'substr' '(' Index ',' UInt ',' UInt ')'
     | 'print' '(' ret_function ')';
     | 'replace' '(' Index [ ',' Index ] ',' ret_function ')'
     | 'replace' '(' Index '.' first_second ','
                 Index '.' first_second ',' ret_function ')'

  first_second: 'first'
              | 'second';

  ret_function: String
              | Name
              | Index
              | 'capitalise' '(' ret_function ')'
              | 'format' '(' ret_function format_params ')'
              | 'replace_all' '(' ret_function ',' ret_function ','
                              ret_function ')'
              | 'system' '(' ret_function ')'
              | 'tolower' '(' ret_function ')'
              | 'toupper' '(' ret_function ')';

  format_params: %empty
               | format_params ',' ret_function;
```

#### Named Tokens for the Scripting Grammar

```
  Index: $[1-9]\d*
  Name: [A-Z_a-z][-\w]*
  String: '(''|[^'])*'
  UInt: \d+
  ```

#### Format Parameter Grammar

Format parameters used by the `format` function use the Python/C++ format. Although everything is treated as text in `gram_grep`, `float` (`[aAbBdeEfFgG]` specifiers) and `int` (`[oxX]` specifiers) handling is supported by on-the-fly conversion to `float` or `int` when passing these parameters to `std::format()`.

```
%token ANY TYPE UINT

format_spec: '{' opt_colon options width_and_precision [type] '}';
opt_colon: %empty | ':';
options: [fill] [align] [sign] ['z'] ['#'] ['0'];
fill: ANY;
align: '<' | '>' | '=' | '^';
sign: '+' | '-' | ' ';
width_and_precision: width_with_grouping [precision_with_grouping];
width_with_grouping: [width] [grouping];
precision_with_grouping: '.' [precision] [grouping];
width: UINT;
precision: UINT;
grouping: ',' | '_';
type: TYPE;
```

#### Named Tokens for Format Parameter Grammar

```
TYPE: [aAbBdeEfFgGoxX]
UINT: \d+;
ANY: .
```
