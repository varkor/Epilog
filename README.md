# Epilog
A toy Prolog compiler

## Building and testing
```
brew install llvm
cmake -H. -Bbuild -DLLVM_CONFIG:FILEPATH=/usr/local/Cellar/llvm/3.9.0/bin/llvm-config
make -C build
./bin/epilog -f examples/hello.el
```