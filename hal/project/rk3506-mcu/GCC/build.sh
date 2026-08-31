#! /bin/bash

make -j10

cd ..
./mkimage.sh
cd -
