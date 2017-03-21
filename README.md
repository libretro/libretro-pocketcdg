# libretro-pocketcdg

[![Build Status](https://travis-ci.org/redbug26/libretro-pocketcdg.svg?branch=master)](https://travis-ci.org/travis-ci/travis-api)

Look for .cdg (& .mp3) file format to use PocketCDG at its best: https://github.com/redbug26/pocketcdg-core/wiki/kcr

## Build instructions on other systems

``` 
git clone https://github.com/redbug26/libretro-pocketcdg.git
cd libretro-pocketcdg/
git submodule update --init --recursive
``` 

Compile for osX
``` 
make platform="osx" -j2 CC="cc" 
```

Compile for linux
``` 
make platform="linux" -j2 CC="cc" 
```

Compile for win (via MSYS2 MingW)
``` 
make platform="win" -j2 CC="cc"
```

Compile for raspberry (from Ubuntu)
```
sudo apt-get install gcc-arm-linux-gnueabihf make
make platform="unix" -j2 CC="arm-linux-gnueabihf-gcc"
```

