# Makefile
# CubismZ
#
# Copyright 2018 ETH Zurich. All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.
#
###############################################################################
# User options
MPICXX ?= mpic++
MPICC ?= mpicc
config ?= release
# hdf-incdir ?= /opt/hdf5/include
# hdf-libdir ?= /opt/hdf5/lib
hdf-incdir ?= $(HDF5_ROOT)/include
hdf-libdir ?= $(HDF5_ROOT)/lib
hdf-libs ?= -lhdf5

# general software configuration options
blocksize ?= 32
precision ?= float
omp ?= 1

# options for the first compression stage (floating point)
# wavz                     (to enable wavz=1)
# fpzip                    (to enable fpzip=1)
# zfp                      (to enable zfp=1)
# sz                       (to enable sz=1)

# options for the second compression stage (encoding)
# zlib                     (to enable zlib=1)
# lz4                      (to enable lz4=1)

# options (bit zeroing, byte shuffling) for the wavelet coefficients, applied between the first and second stage
# zerobits: for wavz + zerobits (to enable zerobits=4 or zerobits=8 or zerobits=12 or zerobits=16)
# shuffle3: for wavz + shuffle3 (to enable shuffle3=1)
###############################################################################

###############################################################################
include Makefile.config
export
###############################################################################
