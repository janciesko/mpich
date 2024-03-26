#!/bin/bash

OPT=$1
THREAD=$2
DEVICE=$3
NJOBS=$4
PREFIX=$5

if [ x"$DEVICE" = x ]; then
    echo "Usage: make.sh OPT THREAD DEVICE (NJOBS) (PREFIX)"
    echo ""
    echo "OPT = debug | opt | default"
    echo "THREAD = pth | pthvci | pthvciopt | abt"
    echo "DEVICE = ucx | ofi | ofisys"
    echo "env: OPT_NUM = 0 1 2 3 4 5 6 7 8 9 10"
    echo ""
    echo "OPT = debug   : debugging configurations are on"
    echo "    = opt     : optimized"
    echo "    = default : default optimization"
    echo ""
    echo "THREAD = pth       : MPICH 4.0a1 equivalent (VCI is disabled)"
    echo "       = pthvci    : MPICH 4.0a1 equivalent (VCI is enabled)"
    echo "       = pthvciopt : MPICH 4.0a1"
    echo "                     + generic engineering for optimizations (VCI is enabled)"
    echo "       = abt       : MPICH 4.0a1"
    echo "                     + genetic engineering for optimizations"
    echo "                     + special Argobots optimizations (VCI is enabled)"
    echo ""
    echo "DEVICE = ucx    : UCX (autodetected)"
    echo "         ofi    : OFI (autodetected)"
    echo "         ofisys : System OFI (originally it was for Spock)"
    echo ""
    echo "OPTNUM = 1 ... 10 : See the script (this excludes one optimization from THREAD=abt)"
    echo ""
    echo "Example:"
    echo "    sh make.sh opt pth ucx 32 /home/user/install_opt_pth_ucx"
    echo "    OPT_NUM=3 sh make.sh opt abt ucx 32 /home/user/install_opt_pth_ucx"
    exit 1
fi

if [ x"$NJOBS" = x ]; then
    NJOBS=1
fi

if [ x"$PREFIX" = x ]; then
    PREFIX="$(pwd)/install_${DEVICE}_${THREAD}"
fi

if  [ ! -d "argobots" ]; then
    echo "No `$(pwd)/argobots`.  Please download it."
    echo "git clone https://github.com/shintaro-iwasaki/argobots.git argobots"
    echo "cd argobots"
    echo "git checkout exp/abtx"
    echo "cd .."
    exit -1
fi

if  [ ! -d "argobots_unopt" ]; then
    echo "No `$(pwd)/argobots_unopt`.  Please download it."
    echo "git clone https://github.com/pmodels/argobots.git argobots_unopt"
    echo "cd argobots_unopt"
    echo "git checkout v1.1"
    echo "cd .."
    exit -1
fi

if [ ! -f "configure" ]; then
    echo "## Run autogen.sh"
    date
    sh autogen.sh
fi

if [ ! -f "modules/ext_yaksa/install/lib/libyaksa.so" ]; then
    echo "## Compile Yaksa"
    date
    # Compile external Yaksa
    rm -rf modules/ext_yaksa
    cp -r modules/yaksa modules/ext_yaksa
    cd modules/ext_yaksa
    ./configure --prefix=$(pwd)/install
    make -j $NJOBS install
    cd ../../
fi

if  [ ! -f "argobots/install/lib/libabt.so" ]; then
    echo "## Compile Argobots"
    date
    cd argobots
    sh autogen.sh
    ./configure --prefix=$(pwd)/install-fast --enable-perf-opt #--enable-affinity
    make -j $NJOBS install
    ./configure --prefix=$(pwd)/install --enable-fast=O0 --enable-debug=yes --enable-tls-model=initial-exec #--enable-affinity
    make -j $NJOBS install
    cd ../
fi

if  [ ! -f "argobots_unopt/install/lib/libabt.so" ]; then
    echo "## Compile Argobots Unopt"
    date
    cd argobots_unopt
    sh autogen.sh
    ./configure --prefix=$(pwd)/install-fast --enable-perf-opt #--enable-affinity
    make -j $NJOBS install
    ./configure --prefix=$(pwd)/install --enable-fast=O0 --enable-debug=yes --enable-tls-model=initial-exec #--enable-affinity
    make -j $NJOBS install
    cd ../
fi

cflags=""
# If needed: --with-ch4-shmmods=none
config_opts="--disable-fortran --with-yaksa=$(pwd)/modules/ext_yaksa/install --disable-numa"

if [ x"$OPT" = x"debug" ]; then
    config_opts="$config_opts --enable-fast=O0 --enable-g=dbg"
elif [ x"$OPT" = x"opt" ]; then
    config_opts="$config_opts --enable-fast=O3 --enable-g=none --enable-error-checking=no"
fi

optcflags=""
fastabtpath="$(pwd)/argobots_unopt/install-fast"
abtpath="$(pwd)/argobots_unopt/install"
if [ x"$OPT_NUM" != x"1" ]; then
    optcflags="${optcflags} -DVCIEXP_AOS_PROGRESS_COUNTS"
fi
if [ x"$OPT_NUM" != x"2" ]; then
    optcflags="${optcflags} -DVCIEXP_PADDING_MPIDI_UCX_CONTEXT_T"
fi
if [ x"$OPT_NUM" != x"3" ]; then
    optcflags="${optcflags} -DVCIEXP_NO_LOCK_SET_PROGRESS_VCI"
fi
if [ x"$OPT_NUM" != x"4" ]; then
    optcflags="${optcflags} -DVCIEXP_FAST_COUNT_ONE_REF_RELEASE"
fi
if [ x"$OPT_NUM" != x"5" ]; then
    optcflags="${optcflags} -DVCIEXP_FAST_UNSAFE_ADD_REF"
fi
if [ x"$OPT_NUM" != x"6" ]; then
    optcflags="${optcflags} -DVCIEXP_PADDING_OBJ_ALLOC_T"
fi
if [ x"$OPT_NUM" != x"7" ]; then
    optcflags="${optcflags} -ftls-model=initial-exec"
fi
if [ x"$OPT_NUM" != x"8" ]; then
    fastabtpath="$(pwd)/argobots/install-fast"
    abtpath="$(pwd)/argobots/install"
fi
if [ x"$OPT_NUM" = x"9" ]; then
    optcflags=""
    fastabtpath="$(pwd)/argobots_unopt/install-fast"
    abtpath="$(pwd)/argobots_unopt/install"
fi
if [ x"$OPT_NUM" != x"10" ]; then
    optcflags="${optcflags} -DVCIEXP_PER_STATE_PROGRESS_COUNTER"
fi

if [ x"$THREAD" = x"pthvci" ]; then
    cflags="${cflags} -DVCIEXP_LOCK_PTHREADS"
    config_opts="$config_opts --enable-thread-cs=per-vci --with-ch4-max-vcis=60"
elif [ x"$THREAD" = x"pthvciopt" ]; then
    cflags="${cflags} -DVCIEXP_LOCK_PTHREADS ${optcflags}"
    config_opts="$config_opts --enable-thread-cs=per-vci --with-ch4-max-vcis=60"
elif [ x"$THREAD" = x"abt" ]; then
    cflags="${cflags} -DVCIEXP_LOCK_ARGOBOTS ${optcflags}"
    config_opts="$config_opts --enable-thread-cs=per-vci --with-ch4-max-vcis=60 --with-thread-package=argobots"
    if [ x"$OPT" = x"opt" ]; then
        config_opts="$config_opts --with-argobots=$fastabtpath"
    else
        config_opts="$config_opts --with-argobots=$abtpath"
    fi
else
    cflags="${cflags}"
fi

if [ x"$DEVICE" = x"ucx" ]; then
    config_opts="$config_opts --with-device=ch4:ucx"
elif [ x"$DEVICE" = x"ofisys" ]; then
    config_opts="$config_opts --with-device=ch4:ofi --with-libfabric=$(realpath $(dirname $(which fi_info))/../)"
else
    # If needed, --enable-psm2=yes --enable-psm=no --enable-sockets=no --enable-verbs=no
    config_opts="$config_opts --with-device=ch4:ofi"
fi

echo "## Configure MPICH"
echo "./configure --prefix=\"${PREFIX}\" ${config_opts} CFLAGS=\"$cflags\""
date
rm -f Makefile
./configure --prefix="${PREFIX}" ${config_opts} CFLAGS="$cflags"

echo "## Compile MPICH"
date
rm -rf ${PREFIX}
make -j $NJOBS install
