#/bin/bash
set -e

mkdir -p build
cd build
rm * -rf

echo "----- start build "

cmake ..

make -j $(nproc)

echo "----- build done"