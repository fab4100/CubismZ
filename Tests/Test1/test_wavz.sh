#!/bin/bash
#
# test_wavz.sh
# CubismZ
#
# Copyright 2018 ETH Zurich. All rights reserved.
#
set -x #echo on

[[ ! -f ../Data/demo.h5 ]] && tar -C ../Data -xJf ../Data/data.tar.xz
h5file=../Data/demo.h5

if [ -z ${1+x} ]
then
	echo "setting err=0.00005"
	err=0.00005
else
	err=$1
	if [ "$1" -eq "-1" ]; then
		echo "setting err=0.00005"
		err=0.00005
	fi
	shift
fi

nproc=1
if [ ! -z ${1+x} ]
then
    nproc=$1; shift
fi

bs=32
ds=128
nb=$(echo "$ds/$bs" | bc)
wt=3	# wavelet type

rm -f tmp.cz

# check if reference file exists, create it otherwise
if [ ! -f ref.cz ]
then
    ./genref.sh
fi

export OMP_NUM_THREADS=$nproc
mpirun -n 1 ../../Tools/bin/wavz_zlib/hdf2cz -bpdx $nb -bpdy $nb -bpdz $nb -sim io -h5file $h5file -czfile tmp.cz  -threshold $err -wtype $wt

mpirun -n $nproc ../../Tools/bin/wavz_zlib/cz2diff -czfile1 tmp.cz -wtype $wt -czfile2 ref.cz
