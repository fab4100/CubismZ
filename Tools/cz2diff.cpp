/*
 * cz2diff.cpp
 * CubismZ
 *
 * Copyright 2018 ETH Zurich. All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <iostream>
#include <string>
#include <sstream>
#include <mpi.h>

#include <float.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "ArgumentParser.h"
#include "Reader_WaveletCompression.h"
#include "Reader_WaveletCompression_plain.h"

int main(int argc, char **argv)
{
	/* Initialize MPI */
        int provided;
        MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);

#if defined(_USE_SZ_)
	printf("sz.config...\n");
	SZ_Init((char *)"sz.config");
#ifdef _OPENMP
	omp_set_num_threads(1);
#endif
#endif

    const double init_t0 = MPI_Wtime();

    /* MPI variables */
    MPI_Comm comm  = MPI_COMM_WORLD;

    int mpi_rank, mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    const bool isroot = !mpi_rank;

    ArgumentParser argparser(argc, (const char **)argv);

	const string inputfile_name1 = argparser("-czfile1").asString("none");
	const string inputfile_name2 = argparser("-czfile2").asString("none");

	if (argparser.exist("-help") || ((inputfile_name1 == "none")||(inputfile_name2 == "none")))
	{
        printf("Usage: %s -czfile1 <cz file1> [-wtype <wt>] -czfile2 <cz reference file2>\n", argv[0]);
		exit(1);
	}

	if (isroot)
		argparser.loud();
	else
		argparser.mute();

	const bool swapbytes = argparser.check("-swap");
	const int wtype = argparser("-wtype").asInt(3);

	Reader_WaveletCompressionMPI  myreader1(comm, inputfile_name1, swapbytes, wtype);
	Reader_WaveletCompressionMPI_plain myreader2(comm, inputfile_name2, swapbytes, wtype);

	myreader1.load_file();
	myreader2.load_file();
	const double init_t1 = MPI_Wtime();

	const double t0 = MPI_Wtime();

	int NBX1 = myreader1.xblocks();
	int NBY1 = myreader1.yblocks();
	int NBZ1 = myreader1.zblocks();
	if (isroot) fprintf(stdout, "[czfile1] I found in total %dx%dx%d blocks.\n", NBX1, NBY1, NBZ1);

	int NBX2 = myreader2.xblocks();
	int NBY2 = myreader2.yblocks();
	int NBZ2 = myreader2.zblocks();
	if (isroot) fprintf(stdout, "[czfile2] I found in total %dx%dx%d blocks.\n", NBX2, NBY2, NBZ2);

	if ((NBX1 != NBX2) || (NBZ1 != NBZ2) ||(NBZ1 != NBZ2)) {
		printf("Dimensions differ, exiting..\n");
		exit(1);
	}

	static Real targetdata1[_BLOCKSIZE_][_BLOCKSIZE_][_BLOCKSIZE_];
	static Real targetdata2[_BLOCKSIZE_][_BLOCKSIZE_][_BLOCKSIZE_];

	const int nblocks = NBX1*NBY1*NBZ1;
	const int b_end = ((nblocks + (mpi_size - 1))/ mpi_size) * mpi_size;

	long n = 0;
	double e_inf = 0;
	double e_1 = 0;
	double e_2 = 0;

	double n_inf = 0;
	double n_1 = 0;
	double n_2 = 0;

	double f1, f2;
	double maxdata = -DBL_MAX;
	double mindata =  DBL_MAX;

	for (int b = mpi_rank; b < b_end; b += mpi_size)
	{
		int z = b / (NBY1 * NBX1);
		int y = (b / NBX1) % NBY1;
		int x = b % NBX1;

		if (b < nblocks)
		{
#if defined(VERBOSE)
			fprintf(stdout, "loading block( %d, %d, %d )...\n", x, y, z);
#endif
			double zratio1 = myreader1.load_block2(x, y, z, targetdata1);
			double zratio2 = myreader2.load_block2(x, y, z, targetdata2);
			(void)zratio1; (void)zratio2;	// avoid warnings

			for (int zb = 0; zb < _BLOCKSIZE_; zb++)
				for (int yb = 0; yb < _BLOCKSIZE_; yb++)
					for (int xb = 0; xb < _BLOCKSIZE_; xb++) {
						f1 = (double) targetdata1[zb][yb][xb];
						f2 = (double) targetdata2[zb][yb][xb];

						if (f2 > maxdata) maxdata = f2;
						if (f2 < mindata) mindata = f2;

						double v = fabs(f2);

						if (v > n_inf) {
							n_inf = v;
						}
						n_1 += v;
						n_2 += v*v;

						double err = fabs(f1-f2);

						if (err > e_inf) {
#if VERBOSE
							printf("%15.8f vs %15.8f -> %15.8f (rel_err = %15.8f%%)\n", f1, f2, err, 100.0*(err/v));
#endif
							e_inf = err;
						}
						e_1 += err;
						e_2 += err*err;
						n++;
					}

		}
		else {
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);
	const double t1 = MPI_Wtime();

	if (!mpi_rank)
	{
		fprintf(stdout, "Init time = %.3lf seconds\n", init_t1-init_t0);
		fprintf(stdout, "Elapsed time = %.3lf seconds\n", t1-t0);
		fflush(0);
	}

	// MPI_Reduce for all of the following
	if (!mpi_rank) {
		MPI_Reduce(MPI_IN_PLACE, &mindata, 	1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
		MPI_Reduce(MPI_IN_PLACE, &maxdata, 	1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		MPI_Reduce(MPI_IN_PLACE, &e_inf, 	1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		MPI_Reduce(MPI_IN_PLACE, &n_inf, 	1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		MPI_Reduce(MPI_IN_PLACE, &e_1,   	1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(MPI_IN_PLACE, &n_1,   	1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(MPI_IN_PLACE, &e_2,   	1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(MPI_IN_PLACE, &n_2,   	1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(MPI_IN_PLACE, &n,     	1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
	} else {
		MPI_Reduce(&mindata, &mindata, 		1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
		MPI_Reduce(&maxdata, &maxdata, 		1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		MPI_Reduce(&e_inf, &e_inf, 		1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		MPI_Reduce(&n_inf, &n_inf, 		1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
		MPI_Reduce(&e_1, &e_1,   		1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(&n_1, &n_1,   		1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(&e_2, &e_2,   		1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(&n_2, &n_2,   		1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(&n, &n,     			1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
	}


	if (!mpi_rank)
	{
#if VERBOSE
		printf("=========================\n");
		printf("e_inf      = %e\n", e_inf);
		printf("n_inf      = %e\n", n_inf);
		printf("rel(e_inf) = %e\n", e_inf/n_inf);
		printf("\n");

		printf("e_1        = %e\n", e_1);
		printf("rel(e_1)   = %e\n", e_1/n_1);
		printf("mean(e_1)  = %e\n", e_1/n);
		printf("\n");

		printf("e_2        = %e\n", sqrt(e_2));
		printf("rel(e_2)   = %e\n", sqrt(e_2)/sqrt(n_2));
		printf("mean(e_2)  = %e\n", sqrt(e_2)/n);
		printf("\n");

		printf("n          = %ld\n", n);
#endif
		double linf = e_inf;
		long nall = n;
		double l1 = e_1 / nall;
		const long double mse = e_2 / nall;

		double uncompressed_footprint = sizeof(Real) * nall;
		double compressed_footprint = uncompressed_footprint;

		//quickly open the initial file and get its size
		FILE *fpz = fopen(inputfile_name1.c_str(), "rb");
		if (fpz == NULL)
                        printf("fp == NULL for %s\n", argv[3]);
		else
		{
			int fd = fileno(fpz); //if you have a stream (e.g. from fopen), not a file descriptor.
			struct stat buf;
			fstat(fd, &buf);
			compressed_footprint = buf.st_size;
			fclose(fpz);
		}

		printf("compression-rate: %.2f rel-linf-error: %e rel-mean-error: %e\n",
			uncompressed_footprint / compressed_footprint, linf, l1);

#if VERBOSE
		printf("mindata = %f\n", mindata);
		printf("maxdata = %f\n", maxdata);
#endif
		//https://en.wikipedia.org/wiki/Peak_signal-to-noise_ratio
		double psnr = 20 * log10((maxdata - mindata) / (2 * sqrt(mse)));

		printf("RES: %12s %12s %12s %12s %12s %12s %12s %12s\n", "CR", "rel(e_inf)", "rel(e_1)", "mean(e_1)", "rel(e_2)", "mean(e_2)", "BPS", "PSNR");
		printf("RES: %12.2f %.6e %.6e %.6e %.6e %.6e %12.04f %12.04f\n",
			uncompressed_footprint / compressed_footprint,	// compression-rate
			e_inf/n_inf,					// rel(e_inf)
			e_1/n_1,					// rel(e_1)
			e_1/n,						// mean(e_1)
			sqrt(e_2)/sqrt(n_2),				// rel(e_2)
			sqrt(e_2)/n,					// mean(e_2)
			compressed_footprint * 8. / nall,		// BITRATE (bits per element)
			psnr);						// PSNR
	}

	/* Close/release resources */
	MPI_Finalize();

	return 0;
}
