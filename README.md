# gram_grep
Search text using a grammar, lexer, or straight regex. Chain searches for greater refinement.

See [http://benhanson.net/gram_grep.html](http://benhanson.net/gram_grep.html) for examples.

To build:

gram_grep now has a dependency on boost::regex due to the std version having an outdated version of the egrep syntax.

- See <a href="https://www.boost.org/users/download/">here</a> to download boost.

## To build boost on Windows

From the boost root directory:
```
.\bootstrap.bat
.\b2
```

## To build boost on Linux/UNIX Variants

See <a href="https://www.boost.org/doc/libs/1_87_0/more/getting_started/unix-variants.html#prepare-to-use-a-boost-library-binary">here</a>.

## Acquiring the Source Dependencies

```
git clone https://github.com/BenHanson/gram_grep.git
git clone https://github.com/BenHanson/parsertl17.git
git clone https://github.com/BenHanson/lexertl17.git
git clone https://github.com/BenHanson/wildcardtl.git
```

## Setting BOOST_ROOT
All platforms now require that you set the BOOST_ROOT environment variable to the root of your boost source (e.g. `/home/ben/Dev/boost/boost_1_77_0`).

## Building

- If on Windows, using a Developer Command Prompt, you can run `msbuild gram_grep.sln /property:Configuration=Release` from the `gram_grep` directory
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

