#!/bin/bash
#
# genref.sh
# CubismZ
#
# Copyright 2018 ETH Zurich. All rights reserved.
#
set -x #echo on

[[ ! -f ../Data/data_005000-p.h5 ]] && (cd ../Data; ./get_cavitation_data.sh)
h5file=../Data/data_005000-p.h5

bs=32	# block size (per dimension) (total: bs^3)
ds=512	# domain size (per dimension) (total: ds^3)
nb=16	# number of blocks (per dimension) (total: nb^3) # $(echo "$ds/$bs" | bc)

rm -f ref.cz	# remove any previous instance of the reference file ref.cz

# 1x1x1 process grid with 16^3 blocks

export OMP_NUM_THREADS=1
mpirun -n 1 ../../Tools/bin/default/hdf2cz -nprocx 1 -nprocy 1 -nprocz 1 -bpdx 16 -bpdy 16 -bpdz 16 -h5file $h5file -czfile ref.cz

# 2x2x2 process grid with 8^3 blocks on each process
# mpirun -n 8 ../../Tools/bin/default/hdf2cz -nprocx 2 -nprocy 2 -nprocz 2 -bpdx 8 -bpdy 8 -bpdz 8 -h5file $h5file -czfile ref.cz
