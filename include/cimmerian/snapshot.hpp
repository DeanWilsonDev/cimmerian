#pragma once

#ifndef CIMMERIAN_ENABLE_SNAPSHOT_TESTING
#error                                                                                                              \
    "cimmerian/snapshot.hpp requires snapshot testing support to be compiled in: configure cimmerian with -DCIMMERIAN_ENABLE_SNAPSHOT_TESTING=ON"
#endif

#include "snapshot/hash-snapshot-store.hpp"
#include "snapshot/inline-snapshot-rewriter.hpp"
#include "snapshot/snapshot-key.hpp"
#include "snapshot/snapshot-macros.hpp"
#include "snapshot/snapshot-run-mode.hpp"
#include "snapshot/string-snapshot-store.hpp"
