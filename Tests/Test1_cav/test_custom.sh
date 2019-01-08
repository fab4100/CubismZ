#!/bin/bash
#
# test_custom.sh
# CubismZ
#
# Copyright 2018 ETH Zurich. All rights reserved.
#
set -x #echo on

[[ ! -f ../Data/data_005000-p.h5 ]] && (cd ../Data; ./get_cavitation_data.sh)
h5file=../Data/data_005000-p.h5

mypath="$1"; shift
thresh=$1; shift
nproc=1
if [ ! -z ${1+x} ]
then
    nproc=$1; shift
fi

bs=32
ds=512
nb=$(echo "$ds/$bs" | bc)

rm -f tmp.cz

# check if reference file exists, create it otherwise
if [ ! -f ref.cz ]
then
    ./genref.sh
fi

export OMP_NUM_THREADS=$nproc
mpirun -n 1 ${mypath}/hdf2cz -bpdx $nb -bpdy $nb -bpdz $nb -sim io -h5file $h5file -czfile tmp.cz -threshold $thresh

mpirun -n $nproc ${mypath}/cz2diff -czfile1 tmp.cz  -czfile2 ref.cz
