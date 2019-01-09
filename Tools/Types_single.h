/*
 * Types_single.h
 * CubismZ
 *
 * Copyright 2018 ETH Zurich. All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifdef _FLOAT_PRECISION_
typedef float Real;
#else
typedef double Real;
#endif

#include <fstream>
#include "math.h"

using namespace std;

#include <Grid.h>

class Simulation
{
public:
	virtual void setup() { }
	virtual void dispose() { }
	virtual ~Simulation() { }
};

struct FluidElement
{
	Real u;

	void clear() { u = 0; }


	FluidElement& operator = (const FluidElement & gp)
	{
		this->u = gp.u;

		return *this;
	}
};

struct FluidBlock
{
	static const int sizeX = _BLOCKSIZE_;
	static const int sizeY = _BLOCKSIZE_;
	static const int sizeZ = _BLOCKSIZE_;

	static const int gptfloats = sizeof(FluidElement)/sizeof(Real);

	typedef FluidElement ElementType;
	typedef FluidElement element_type;

	FluidElement data[_BLOCKSIZE_][_BLOCKSIZE_][_BLOCKSIZE_];

	void clear_data()
	{
		const int N = sizeX*sizeY*sizeZ;
		FluidElement * const e = &data[0][0][0];
		for(int i=0; i<N; ++i) e[i].clear();
	}

	void clear()
	{
		clear_data();
	}

	inline FluidElement& operator()(int ix, int iy=0, int iz=0)
	{
		assert(ix>=0 && ix<sizeX);
		assert(iy>=0 && iy<sizeY);
		assert(iz>=0 && iz<sizeZ);

		return data[iz][iy][ix];
	}

};


struct StreamerGridPointIterative
{
	static const int channels = 1;

	FluidBlock * ref;
	StreamerGridPointIterative(FluidBlock& b): ref(&b) {}
	StreamerGridPointIterative(): ref(NULL) {}

	template<int channel>
	static inline Real operate(const FluidElement& input) { abort(); return 0; }

	inline Real operate(const int ix, const int iy, const int iz) const
	{
		cout << "You must not call this operate method of StreamerGridPointIterative" << endl;
		abort();
		return 0;
	}

	const char * name() { return "StreamerGridPointIterative" ; }
};

template<> inline Real StreamerGridPointIterative::operate<0>(const FluidElement& e) { return e.u; }

typedef Grid <FluidBlock, std::allocator> FluidGridBase;
typedef FluidGridBase FluidGrid;
