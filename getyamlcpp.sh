#!/bin/bash

wget https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.3.tar.gz
tar xzf yaml-cpp-0.6.3.tar.gz

#wget https://github.com/jbeder/yaml-cpp/archive/release-0.5.3.tar.gz
#tar xzf release-0.5.3.tar.gz

#wget https://github.com/jbeder/yaml-cpp/archive/release-0.5.2.tar.gz
#tar xzf release-0.5.2.tar.gz

#wget https://yaml-cpp.googlecode.com/files/yaml-cpp-0.5.0.tar.gz
#tar xzf yaml-cpp-0.5.0.tar.gz

cd yaml-cpp-0.6.3
cmake -D CMAKE_INSTALL_PREFIX=$PWD/.. .
make -j
make install
