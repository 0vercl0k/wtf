export CC=clang-13
export CXX=clang++-13
export CFLAGS='-fsanitize=address'
export CXXFLAGS='-fsanitize=address'
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja && cmake --build .
