#!/bin/bash -x
set -ev

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
    wget https://github.com/google/protobuf/releases/download/v3.5.1/protobuf-all-3.5.1.tar.gz
    tar xaf protobuf-all-3.5.1.tar.gz
    cd protobuf-3.5.1
    ./configure --prefix=/usr
    make -j8
    sudo make install
    cd ../
    rm -Rf protobuf-all-3.5.1.tar.gz protobuf-all-3.5.1
    wget https://capnproto.org/capnproto-c++-0.6.1.tar.gz
    # git clone https://github.com/helicopter88/capnproto.git
    tar -xaf capnproto-c++-0.6.1.tar.gz
    cd capnproto-c++-0.6.1
    #autoreconf -i
    ./configure --prefix=/usr
    make -j8
    sudo make install
    cd ../
    rm -Rf capnproto-c++-0.6.1*
else
  echo "Unhandled TRAVIS_OS_NAME \"${TRAVIS_OS_NAME}\""
  exit 1
fi
