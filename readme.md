# ![](./logo.png) TINE Is Not a (TempleOS) Emulator
 \* *logo courtesy of CrunkLord420.*
## required skills
 - knowledge of TempleOS
 - knowledge of C
 - (optional but recommended)knowledge of GDB/LLDB(for debugging the loader)

## system requirements
 - AMD64 architecture
 - Operating System: Linux, FreeBSD or Windows

# building
## windows users
### only supports >=Win10(complain to msys2 devs not me), msvc unsupported
install msys2, launch the "MSYS2 MINGW64 Shell", and run the following
```
pacman -Syu yasm make mingw-w64-x86_64-{cmake,gcc,SDL2}
```
if pacman tells you to restart the terminal then do it and run the cmd again(rtfm)
## unix-like system users
install SDL2, cmake, make, yasm and gcc/clang
## building the loader
```
mkdir build;
cd build;
cmake ..; # *nix ***THIS LINE ISNT FOR WINDOWS***
cmake .. -G 'MSYS Makefiles' # ***WINDOWS***
make -j$(nproc);
```
side note: there used to be a makefile to completely statically build an exe on windows but it seems like windows doesnt like it(doesnt output anything, just hangs) so i removed it, so make sure to run the built binary in the mingw terminal to avoid dll hell
## build runtime
```
./tine -f HCRT_BOOTSTRAP.BIN -ctT BuildHCRT.HC
mv T/HCRT.BIN .
```
# running
```
./tine -t T #-h for info on other flags
```
# caveats
due to running in userspace, context switching is around 4 times slower(not that it matters anyway outside of flexing `CPURep(TRUE);` results) and ring 0 routines like In/OutU8 are not present <br>
division by zero is not an exception, it will bring up the debugger(SIGFPE)

# contributing
[read this](./contrib.md)

# documentation
```C
Cd("T:/Server");
#include "run";
//point browser to localhost:8080
```
contributions to wiki appreciated

# building TempleOS from TINE
because this runtime uses the orthodox version of the HolyC compiler,building a TempleOS distro is possible(why) <br>
ISO must end in `ISO.C`

```
//Download an orthodox ISO into your T drive from TempleOS.org
Move("TempleOSLite.ISO","TempleOSLite.ISO.C"); //Move it to end in ISO.C
#include "Boot/DoDistro.HC";
MakeMyISO("/TempleOSLite.ISO.C","/MyDistro.ISO.C");
//ExitTINE; optionally exit idk
//Run the ISO using qemu -m 512M -cdrom T/MyDistro.ISO.C
```
# ref
```C
DirMk("folder");
Dir;
Dir("fold*");//supports wildcards
Cd("folder");
Man("Ed");
Ed("file.HC.Z");
Unzip("file.HC.Z");//unzip TempleOS compression
Zip("file.HC");
Find("str",,"-i");//grep -rn . -e str
FF("file.*");//find .|grep file
MountFile("Something.ISO.C");//MountFile(U8 *f,U8 drv_let='M');
Cd("M:/");//defaults to M
INT3;//force raise debugger
DbgHelp;//help on how to debug
ExitTINE(I32i ec=0);
```
