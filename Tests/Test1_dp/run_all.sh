#!/usr/bin/env bash
# run_all.sh
# CubismZ
#
# Copyright 2018 ETH Zurich. All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.
#
nproc=1
if [[ "$#" -eq 1 ]]; then
    nproc=$1
fi

fout='run_all.txt'

mymsg()
{
    echo ''
    echo '###############################################################################'
    echo "RUNNING: $1"
    echo '###############################################################################'
    echo ''
}



# Test if dependencies are satisfied
use_awk=1
command -v mpirun >/dev/null 2>&1 || { echo >&2 "Can not find 'mpirun' command. Aborting."; exit 1; }
command -v bc >/dev/null 2>&1 || { echo >&2 "Can not find 'bc' command. Aborting."; exit 1; }
command -v awk >/dev/null 2>&1 || { echo >&2 "Can not find 'awk' command for filtered output."; use_awk=0; }

output_filter()
{
    if [ "$use_awk" -eq 1 ]; then
        awk '$0 ~ /(^RE.?:)/ { print $0; }' >> $fout
    else
        cat >> $fout
    fi
}

# remove any previous instances
rm -f $fout
rm -f tmp.cz
rm -f ref.cz

# generate reference
mymsg 'genref.sh' >> $fout
./genref.sh | output_filter

# wavelets + zlib
mymsg 'test_wavz.sh' >> $fout
./test_wavz.sh -1 $nproc | output_filter

# zfp
mymsg 'test_zfp.sh' >> $fout
./test_zfp.sh -1 $nproc | output_filter

# fpzip
mymsg 'test_fpzip.sh' >> $fout
./test_fpzip.sh -1 $nproc | output_filter

# sz
mymsg 'test_sz.sh' >> $fout
./test_sz.sh -1 $nproc | output_filter

rm -f tmp.cz
rm -f ref.cz

exit 0
