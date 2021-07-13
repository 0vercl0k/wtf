# Axel '0vercl0k' Souchet - May 2 2020
# Configure / clone the bochs repository.
cd bochscpu-build
sh prep.sh && cd bochs && sh .conf.cpu && make || true

# Remove old files in bochscpu.
rm -rf ../../bochscpu/bochs
rm -rf ../../bochscpu/libs

# Create the libs directory where we stuff all the libs.
mkdir ../../bochscpu/lib
cp cpu/libcpu.a ../../bochscpu/lib/libcpu.a
cp cpu/fpu/libfpu.a ../../bochscpu/lib/libfpu.a
cp cpu/avx/libavx.a ../../bochscpu/lib/libavx.a
cp cpu/cpudb/libcpudb.a ../../bochscpu/lib/libcpudb.a
make all-clean

# Now we want to copy the bochs directory over there.
cd ..
mv bochs ../bochscpu/bochs

# Now its time to build it.
cd ../bochscpu-ffi

cargo clean
cargo build
cargo build --release

# Get back to libs.
cd ..
