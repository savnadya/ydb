UNITTEST_FOR(ydb/core/kqp)

FORK_SUBTESTS()
SPLIT_FACTOR(100)

IF (WITH_VALGRIND)
    TIMEOUT(3600)
    SIZE(LARGE)
    TAG(ya:fat)
ELSE()
    TIMEOUT(600)
    SIZE(MEDIUM)
ENDIF()

SRCS(
    kqp_olap_stats_ut.cpp
    GLOBAL kqp_olap_ut.cpp
    sys_view_ut.cpp
    indexes_ut.cpp
    GLOBAL blobs_sharing_ut.cpp
    statistics_ut.cpp
    clickbench_ut.cpp
    aggregations_ut.cpp
)

PEERDIR(
    ydb/core/kqp
    ydb/core/kqp/ut/common
    ydb/library/yql/sql/pg_dummy
    ydb/core/tx/columnshard/hooks/testing
    ydb/core/kqp/ut/olap/helpers
    ydb/core/tx/datashard/ut_common
)

YQL_LAST_ABI_VERSION()

IF (SSA_RUNTIME_VERSION)
    CFLAGS(
        -DSSA_RUNTIME_VERSION=$SSA_RUNTIME_VERSION
    )
ENDIF()

END()
