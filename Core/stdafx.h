#pragma once

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <iostream>
#include <string>
#include <cctype>
#include <vector>
#include <algorithm>
#include <sstream>
#include <list>
#include <atomic>
#include <unordered_map>
#include <deque>

#include "../Utilities/UTF8Util.h"

#ifndef __MINGW32__
	#ifdef __clang__
		#define __forceinline __attribute__((always_inline)) inline
	#else
		#ifdef __GNUC__
			#define __forceinline __attribute__((always_inline)) inline
		#endif
	#endif
#endif

using std::vector;
using std::ios;
using std::istream;
using std::ostream;
using std::stringstream;
using std::unordered_map;
using std::deque;
using utf8::ifstream;
using utf8::ofstream;
using std::list;
using std::max;
using std::string;
using std::atomic_flag;
using std::atomic;
