REM # Axel '0vercl0k' Souchet - May 2 2020
REM This assume to be run from the libs directory.
REM OK here is a bunch of stuff to understand about the structure of the repositories.
REM   - bochscpu is the repository that users are supposed to use. Because @ypr604 is such
REM   a nice person it is designed to not have to build bochs. So what this repo expects is to
REM   find a 'lib' directory with a bunch of .lib as well as a 'bochs' folder which contains bochs'
REM   sources as well as the object files generated during compilation etc. So in that case, you
REM   would download an archive that has those 'bochs' / 'lib' folder and build bochscpu-ffi (which
REM   builds bochscpu for you).
REM   - If you want to build it yourself though, there is bochscpu-build which clones the svn repo
REM   and where you actually compile bochs. Once you are done with it you drop the .lib I mentioned
REM   above in the bochscpu 'lib' folder, same with the 'bochs' tree.
REM
REM  If you want to enable source debugging, you can set _CL_=/Z7, set _LINK_=/DEBUG:FULL.

REM Use WSL to configure / clone the bochs repository.
bash -c "cd bochscpu-build && sh prep.sh && cd bochs && sh .conf.cpu-msvc"

REM Build bochs; libinstrument.a is expected to fail to build so don't freak out.
REM You can run nmake all-clean to clean up the build.
cd bochscpu-build\bochs
nmake

REM Remove old files in bochscpu.
rmdir /s /q ..\..\bochscpu\bochs
rmdir /s /q ..\..\bochscpu\libs

REM Create the libs directory where we stuff all the libs.
mkdir ..\..\bochscpu\lib
copy cpu\libcpu.a ..\..\bochscpu\lib\cpu.lib
copy cpu\fpu\libfpu.a ..\..\bochscpu\lib\fpu.lib
copy cpu\avx\libavx.a ..\..\bochscpu\lib\avx.lib
copy cpu\cpudb\libcpudb.a ..\..\bochscpu\lib\cpudb.lib

REM Now we want to copy the bochs directory over there.
mkdir ..\..\bochscpu\bochs
robocopy . ..\..\bochscpu\bochs /e

REM Now its time to build it.
cd ..\..\bochscpu-ffi
REM cargo clean -p bochscpu shits its pants on my computer so rebuilding everything
cargo clean
cargo build
cargo build --release

REM Get back to libs.
cd ..