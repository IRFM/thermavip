#!/bin/bash

# Error handling is enabled below; this comment used to describe a
# `set -e` that was commented out.
#required packages: pkg-config autotools autoconf binutils nasm yasm
# from IRFM

# Source global definitions
if [ -f /etc/bashrc ]; then
	. /etc/bashrc
fi

# Stop on error. Without it a failed configure step still ran make and
# make install, and the script exited zero, so CMake believed the
# third-party build had succeeded.
#
# set -u is deliberately absent: the ffmpeg configure line below passes
# -Wl,-rpath='$ORIGIN' inside DOUBLE quotes, so bash expands ORIGIN, which
# is never defined. Enabling -u would abort there. The empty rpath that
# results is a separate defect, to be fixed before -u can be turned on.
set -eo pipefail
CFLAGS=""

unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     machine=Linux;;
    Darwin*)    machine=Mac;;
    CYGWIN*)    machine=Cygwin;;
    MINGW*)     machine=MinGw;;
    *)          machine="UNKNOWN:${unameOut}"
esac
echo ${machine}

ffmeg_extra_ldflags=""
if [ ${machine} = "Linux" ]; then
   ffmeg_extra_ldflags="-lm -lpthread"
else
  echo "Compiling on non Linux os"
fi

export FFMPEG_VERSION="7.1.1"
export FFMPEG_PREFIX="$PWD/ffmpeg-$FFMPEG_VERSION"

export PKG_CONFIG_PATH="$PWD/install/lib/pkgconfig"

# ARCH="$(uname -p)"
ARCH="native"

function escape_slashes {
    sed 's/\//\\\//g'
}

function change_line {
    local OLD_LINE_PATTERN=$1; shift
    local NEW_LINE=$1; shift
    local FILE=$1

    local NEW=$(echo "${NEW_LINE}" | escape_slashes)
    sed -i .bak '/'"${OLD_LINE_PATTERN}"'/s/.*/'"${NEW}"'/' "${FILE}"
    mv "${FILE}.bak" /tmp/
}

function configLine {
  local OLD_LINE_PATTERN=$1; shift
  local NEW_LINE=$1; shift
  local FILE=$1
  local NEW=$(echo "${NEW_LINE}" | sed 's/\//\\\//g')
  touch "${FILE}"
  # Status captured explicitly: sed returns 100 here on purpose, and under
  # set -e the script would stop before the test below. A `|| true` would
  # overwrite that 100 with 0 and invert the decision.
  local status=0
  sed -i '/'"${OLD_LINE_PATTERN}"'/{s/.*/'"${NEW}"'/;h};${x;/./{x;q100};x}' "${FILE}" || status=$?
  if [[ ${status} -ne 100 ]] && [[ ${NEW_LINE} != '' ]]
  then
    echo "${NEW_LINE}" >> "${FILE}"
  fi
}
NASM_VERSION="2.15.05"
NASM_PACKAGE="nasm-$NASM_VERSION"
# Digest of nasm-2.15.05.tar.bz2 as published on www.nasm.us, taken over a
# validated TLS connection on 2026-09-08. Upstream ships no checksum file, so
# re-derive it from the same host if the version changes.
NASM_SHA256="3c4b8339e5ab54b1bcb2316101f8985a5da50a3f9e504d43fa6f35668bee2fd0"
#build nasm
# `command -v` is the safe form; `nasm -v COMMAND` passed a stray argument.
if ! command -v nasm > /dev/null 2>&1
then
    if [ -d $NASM_PACKAGE ]; then
       echo "Dir $NASM_PACKAGE exists."
    else
       echo "Dir $NASM_PACKAGE does not exist."
       if [ ! -f $NASM_PACKAGE.tar.bz2 ]; then
           echo "Fetching $NASM_PACKAGE.tar.bz2"
           # Certificate validation was switched off here, so anything on the path
           # could hand over a different archive; the script then builds and installs
           # it, and the result assembles everything else.
           wget --https-only https://www.nasm.us/pub/nasm/releasebuilds/$NASM_VERSION/$NASM_PACKAGE.tar.bz2
           echo "$NASM_SHA256  $NASM_PACKAGE.tar.bz2" | sha256sum -c -
       fi
       tar -xjf $NASM_PACKAGE.tar.bz2
    fi

    if [ -f $NASM_PACKAGE/install/bin/nasm ]; then
        echo "File nasm exists."
    else
        cd $NASM_PACKAGE || { echo "Missing directory $NASM_PACKAGE, the fetch failed" ; exit 1 ; }
        ./configure --prefix="$PWD/install"
        make -j
        make install
        chmod 755 install/bin/nasm
        cd ..
    fi
    export PATH=$PWD/$NASM_PACKAGE/install/bin:$PATH
    set PATH=$PWD/$NASM_PACKAGE/install/bin:$PATH
	export AS=$PWD/$NASM_PACKAGE/install/bin/nasm
else
	AS=nasm
fi


#build aom-av1
# FILE=aom
# if [ -d $FILE ]; then
#    echo "Dir $FILE exists."
# else
#    echo "Dir $FILE does not exist."
#    git clone https://aomedia.googlesource.com/aom
# fi
# cd aom
# AOM_DIR=$PWD
# AOM_INSTALL=$PWD/install
# cd ..
# mkdir -p aom_build
# cd aom_build
# FILE=aom.pc
# if [ -f $FILE ]; then
#    echo "File $FILE exists."
# else
#    rm -f CMakeCache.txt
#    rm -rf CMakeFiles
#    cmake $AOM_DIR -DBUILD_STATIC_LIBS=1  -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=0 -DCMAKE_INSTALL_PREFIX=$AOM_INSTALL -G "Unix Makefiles"
#    make -j16
# fi
# if [ -d $AOM_INSTALL ]; then
#    echo "File $AOM_INSTALL exists."
# else
#    make install -j16
# fi
# cd ..
# export PKG_CONFIG_PATH=$AOM_INSTALL/lib64/pkgconfig:$PKG_CONFIG_PATH
# export LD_LIBRARY_PATH=$AOM_INSTALL/lib64:$LD_LIBRARY_PATH

# build libvpx

# FILE=libvpx
# if [ -d $FILE ]; then
#    echo "Dir $FILE exists."
# else
#    echo "Dir $FILE does not exist."
#    git clone --depth 1 https://chromium.googlesource.com/webm/libvpx.git
# fi
# cd libvpx
# LIBVPX_INSTALL=$PWD/install
# git fetch --tags
# git checkout v1.10.0
# FILE=vpx.pc
# if [ -f $FILE ]; then
#    echo "File $FILE exists."
# else
#    echo "File $FILE does not exist."
#    ./configure --enable-vp9 --disable-examples --disable-tools --disable-docs --enable-pic --prefix=$LIBVPX_INSTALL --as=nasm
#    make -j
#    make install -j

# fi
# export PKG_CONFIG_PATH=$LIBVPX_INSTALL/lib/pkgconfig:$PKG_CONFIG_PATH
# export LD_LIBRARY_PATH=$LIBVPX_INSTALL/lib:$LD_LIBRARY_PATH
# cd ..


KVAZAAR_VERSION="v2.3.2"
#build kvazaar:
FILE=kvazaar
if [ -d $FILE ]; then
   echo "Dir $FILE exists."
else
   echo "Dir $FILE does not exist."
   # A tag, not the tip of the default branch: what gets linked into the shipped
   # binary should not change from one build to the next.
   git clone https://github.com/ultravideo/kvazaar.git
   ( cd kvazaar && git checkout "$KVAZAAR_VERSION" )
fi

FILE=kvazaar/install/lib/pkgconfig/kvazaar.pc
if [ -f $FILE ]; then
   echo "File $FILE exists."
else
   cd kvazaar || { echo "Missing directory kvazaar, the fetch failed" ; exit 1 ; }
   #export CFLAGS=$CFLAGS:"-fPIC"
   echo 'Configuring kvazaar...'
   #replace line AM_PROG_AR by m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) as it fails to work properly with older version of autoconf
   sed -i '/AM_PROG_AR/c\m4_ifdef([AM_PROG_AR], [AM_PROG_AR])' ./configure.ac
   ./autogen.sh
   ./configure --enable-static=yes --enable-shared=no --prefix=$PWD/install CFLAGS="-fPIC" #--extra-ldflags="-Wl,-rpath='$ORIGIN'"
   make -j
   make install
   
   # Add -lm -lpthread to kvazaar libs
   line=$(grep "lkvazaar" ./install/lib/pkgconfig/kvazaar.pc)
   # line_num=$(grep -n "lkvazaar" ./install/lib/pkgconfig/kvazaar.pc | cut -f1 -d:)
   line="$line -lm -lpthread"
   sed -i "/lkvazaar/c $line" ./install/lib/pkgconfig/kvazaar.pc
   # sed -i '${line_num}i\-lm -lpthread\' ./install/lib/pkgconfig/kvazaar.pc
   cd ..
fi
export PKG_CONFIG_PATH=$PWD/kvazaar/install/lib/pkgconfig:$PKG_CONFIG_PATH
export LD_LIBRARY_PATH=$PWD/kvazaar/install/lib:$LD_LIBRARY_PATH

X264_COMMIT="b35605ace3ddf7c1a5d67a2eb553f034aef41d55"
#build x264
FILE=x264
if [ -d $FILE ]; then
   echo "Dir $FILE exists."
else
   echo "Dir $FILE does not exist."
   # x264 publishes no tags, so the stable branch is pinned by commit.
   git clone https://code.videolan.org/videolan/x264.git
   ( cd x264 && git checkout "$X264_COMMIT" )
fi

FILE=x264/x264.pc
if [ -f $FILE ]; then
   echo "File $FILE exists."
else
   cd x264 || { echo "Missing directory x264, the fetch failed" ; exit 1 ; }
   echo 'Configuring x264...'
   #sed -i 's/asm=\"auto\"/asm=\"nasm\"/' configure
   ./configure --enable-static --enable-pic --prefix=$PWD/install --enable-strip #--enable-rpath --extra-ldflags="-Wl,-rpath='$ORIGIN'"
   make -j
   make install
   # libx264 already produces a valid pc file
   cd ..
fi
export PKG_CONFIG_PATH=$PWD/x264/install/lib/pkgconfig:$PKG_CONFIG_PATH


echo 'current pkg-config paths: '"$PKG_CONFIG_PATH"
echo "$(pkg-config --libs kvazaar)"

#build ffmpeg
FILE=ffmpeg
if [ -d $FILE ]; then
   echo "Dir $FILE exists."
else
   echo "Dir $FILE does not exist."
   git clone https://git.ffmpeg.org/ffmpeg.git

fi
#FFMPEG_VERSION="4.4"
cd ffmpeg || { echo "Missing directory ffmpeg, the fetch failed" ; exit 1 ; }
git checkout n$FFMPEG_VERSION
FILE=config.h
if [ -f $FILE ]; then
   echo "File $FILE exists."
else
   echo 'Configuring ffmpeg...'
   ./configure --arch=native --enable-asm --disable-libvpx --disable-libaom --disable-static --enable-shared --enable-libx264 --enable-libkvazaar --extra-cflags=-DKVZ_STATIC_LIB --prefix=$PWD/install --enable-gpl --enable-rpath --extra-ldflags="-L/usr/local/lib -ldl $ffmeg_extra_ldflags -fPIC -m64 -Wl,-rpath='$ORIGIN'" --enable-pic --disable-debug
   #~ if [[ $1 == "gpl" ]]; then
   OUTPUT=$(cat ffbuild/config.log)
   echo "${OUTPUT}"
   echo $PKG_CONFIG_PATH
   # ./configure --arch=$ARCH --enable-asm --enable-libvpx --enable-libaom --disable-static --enable-shared --enable-libx264 --enable-libkvazaar --prefix=$PWD/install --enable-gpl --enable-rpath --extra-ldflags="-L/usr/local/lib -ldl -fPIC -m64 -Wl,-rpath='$ORIGIN'" --enable-pic
   #~ else
   #~ echo $PKG_CONFIG_PATH
      #~ ./configure --arch=$ARCH --enable-asm --enable-libvpx --enable-libaom --disable-static --enable-shared --enable-libkvazaar --prefix=$PWD/install --disable-gpl --enable-rpath --extra-ldflags="-L/usr/local/lib -ldl -fPIC -m64" --enable-pic
   #~ fi

fi

FILE=install/include
if [ -d $FILE ]; then
    echo "Dir $FILE exists."
else
    echo 'Building ffmpeg...'
    #make clean
    make -j
    make install
fi
cd ..


# copy kvazaar shared lib to ffmpeg, x264 has been built statically


# if [[ ${machine} != "Linux" ]]; then
    # cp -P kvazaar/install/lib/libkvazaar.dll* ffmpeg/install/lib
    # cp -P kvazaar/install/bin/libkvazaar*.dll* ffmpeg/install/bin
# else
	# cp -P kvazaar/install/lib/libkvazaar.so* ffmpeg/install/lib
	# cp -P kvazaar/install/lib/libkvazaar.so* ffmpeg/install/bin
# fi
