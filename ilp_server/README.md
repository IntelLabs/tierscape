## OR-Tools installation

### Install OR-Tools
We use the guide given at this location:
https://github.com/google/or-tools/blob/stable/cmake/README.md


````
wget https://github.com/google/or-tools/archive/refs/tags/v9.12.tar.gz
tar -xzvf  <tar file>
cd <dir>

cmake -S. -Bbuild -DBUILD_DEPS:BOOL=ON
cmake --build build --parallel
cd build
sudo make install -j
````

### Debugging installation issues

**Note**: you will need g++12 gcc-12 and cmake new version

````
wget https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-linux-x86_64.sh
chmod +x cmake-3.28.3-linux-x86_64.sh
sudo ./cmake-3.28.3-linux-x86_64.sh --skip-license --prefix=/usr/local
sudo apt install gcc-12 g++-12 
````

Retry building OR-tools after deleting the `build` dir.
