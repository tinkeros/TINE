# ffi
runtime.cxx
```C
uint64_t STK_FunctionName(uint64_t* stk) {
  // ...
}
S_(FunctionName, function arg cnt);
```
`STK_FunctionName` ***MUST*** return void OR a value that is 8 bytes big

T/KERNELA.HH
```C
...after #ifdef IMPORT_BUILTINS
import U64 FunctionName(....);
...#else then lots of extern
extern <same function prototype>;
```
build hcrt and loader again, copy HCRT.BIN to HCRT\_BOOTSTRAP.BIN and commit
# extending the kernel
T/KERNELA.HH
```C
//same as ffi
```
T/HCRT\_TOS.HC
```C
#include "<desired holyc file>"
```
rebuild hcrt, copy HCRT.BIN to HCRT\_BOOTSTRAP.BIN and commit
# header generation
T/FULL\_PACKAGE.HC
```C
#define GEN_HEADERS 1
```
make -> run tine -> T/unfound.DD
```
<functions>
```
copy desired fn prototypes to T/KERNELA.HH
