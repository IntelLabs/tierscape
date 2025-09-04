cd /tmp

wget https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-linux-x86_64.sh
chmod +x cmake-3.28.3-linux-x86_64.sh
sudo ./cmake-3.28.3-linux-x86_64.sh --skip-license --prefix=/usr/local
sudo apt install gcc-12 g++-12 



wget https://github.com/google/or-tools/archive/refs/tags/v9.12.tar.gz
tar -xzvf  v9.12.tar.gz
cd or-tools-9.12/
cmake -S. -Bbuild -DBUILD_DEPS:BOOL=ON
cmake --build build --parallel
cd build
sudo make install -j