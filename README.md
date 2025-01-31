# gram_grep
Search text using a grammar, lexer, or straight regex. Chain searches for greater refinement.

See [http://benhanson.net/gram_grep.html](http://benhanson.net/gram_grep.html) for examples.

To build:
```
git clone https://github.com/BenHanson/gram_grep.git
git clone https://github.com/BenHanson/parsertl17.git
git clone https://github.com/BenHanson/lexertl17.git
git clone https://github.com/BenHanson/wildcardtl.git
```
- If on Windows, using a Developer Command Prompt, you can use run `msbuild gram_grep.sln /property:Configuration=Release` from the `gram_grep` directory
- If on Linux you can run `make` from the `gram_grep` directory
- If you would like to use `cmake`, instead follow the below instructions

### Building Using CMake
```
cd gram_grep
mkdir build
cd build
cmake ..
cmake --build .
```
