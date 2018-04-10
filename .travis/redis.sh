#!/bin/bash -x
set -ev

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
    git clone https://github.com/Cylix/cpp_redis.git
    cd cpp_redis
    git submodule init && git submodule update
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j8
    sudo make install
    cd ../
    rm -Rf cpp_redis
else
  echo "Unhandled TRAVIS_OS_NAME \"${TRAVIS_OS_NAME}\""
  exit 1
fi
