//===-- SolverStats.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_SOLVERSTATS_H
#define KLEE_SOLVERSTATS_H

#include "klee/Statistic.h"

namespace klee {
    namespace stats {

        extern Statistic cexCacheTime;
        extern Statistic queries;
        extern Statistic queriesInvalid;
        extern Statistic queriesValid;
        extern Statistic queryCacheHits;
        extern Statistic queryCacheMisses;
        extern Statistic queryCexCacheHits;
        extern Statistic queryCexCacheMisses;
        extern Statistic queryConstructTime;
        extern Statistic queryConstructs;
        extern Statistic queryCounterexamples;
        extern Statistic queryTime;
        // Persistent cache related statistics
        extern Statistic pcacheTime;
        extern Statistic pcacheHits;
        extern Statistic pcacheMisses;
        extern Statistic pcacheLookupTime;
        extern Statistic pcacheInsertionTime;
        extern Statistic pcacheRedisSize;
        extern Statistic pcachePMapSize;

#ifdef DEBUG
        extern Statistic arrayHashTime;
#endif

    }
}

#endif
