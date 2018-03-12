#!/bin/bash -x
set -ev

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
    wget https://github.com/google/protobuf/releases/download/v3.5.1/protobuf-all-3.5.1.tar.gz
    tar xaf protobuf-all-3.5.1.tar.gz
    cd protobuf-3.5.1
    ./configure --prefix=/usr
    make -j8
    sudo make install
else
  echo "Unhandled TRAVIS_OS_NAME \"${TRAVIS_OS_NAME}\""
  exit 1
fi
