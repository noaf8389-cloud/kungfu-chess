cmake_minimum_required(VERSION 3.16)
project(KungFuChessServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# hiredis always builds (no external system dependency to detect, unlike libpqxx),
# but a real Redis instance is only reachable in the Docker Compose environment —
# so its integration tests are gated behind this explicit flag, not auto-detection.
option(REDIS_TESTS "Build integration tests that require a real Redis instance" OFF)
# Same reasoning as REDIS_TESTS: nats.c always builds, but a real NATS server is
# only reachable in the Docker Compose environment.
option(NATS_TESTS "Build integration tests that require a real NATS instance" OFF)
# Requires the real WsGateway/MatchmakerServer containers (Phase 5) reachable
# by service name -- only true in the Docker Compose environment.
option(PROCESS_SPLIT_TESTS "Build integration tests that require the real WsGateway/MatchmakerServer containers" OFF)

include(FetchContent)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.5
)
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

FetchContent_Declare(
    sqlite3
    GIT_REPOSITORY https://github.com/azadkuh/sqlite-amalgamation.git
    GIT_TAG 3.36.0
)
FetchContent_MakeAvailable(sqlite3)
set(SQLITE3_ROOT ${sqlite3_SOURCE_DIR})

FetchContent_Declare(
    hiredis
    GIT_REPOSITORY https://github.com/redis/hiredis.git
    GIT_TAG v1.2.0
)
set(DISABLE_TESTS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(hiredis)

# hiredis registers its own "hiredis-test" via test.sh regardless of DISABLE_TESTS
# on some versions; that script isn't runnable through ctest on Windows and isn't
# our code anyway — disable it explicitly rather than let it show up as a failure.
if(TEST hiredis-test)
    set_tests_properties(hiredis-test PROPERTIES DISABLED TRUE)
endif()

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
    natsc
    GIT_REPOSITORY https://github.com/nats-io/nats.c.git
    GIT_TAG v3.8.0
)
set(NATS_BUILD_STREAMING OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_WITH_TLS OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# nats.c gates its own ~100-test internal suite (requires a real NATS/STAN server)
# behind the standard BUILD_TESTING variable, not a library-specific option like
# hiredis's DISABLE_TESTS — turn it off only for this dependency, doesn't affect
# our own KungFuChessServerTests registration (that's driven by enable_testing()/
# add_test(), independent of BUILD_TESTING).
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(natsc)

set(LOGIC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/logic")
set(SHARED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
file(GLOB_RECURSE LOGIC_SOURCES "${LOGIC_ROOT}/*.cpp")

file(GLOB SHARED_BUS_SOURCES "${SHARED_ROOT}/bus/*.cpp")
file(GLOB SHARED_LOGGING_SOURCES "${SHARED_ROOT}/logging/*.cpp")
file(GLOB NETWORK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/network/*.cpp")
file(GLOB SERVER_ROOT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB DB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/db/*.cpp")
file(GLOB RATING_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rating/*.cpp")
file(GLOB MATCHES_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/matches/*.cpp")
file(GLOB LOBBY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lobby/*.cpp")
file(GLOB AUTH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/auth/*.cpp")
file(GLOB ROOMS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rooms/*.cpp")

# Each of these three owns its own main() and becomes a separate process
# (Implementation_Roadmap.md Phase 5) -- excluded from SERVER_ROOT_SOURCES so
# they don't collide with main.cpp's main() inside KungFuChessServer, the
# original single-process executable, kept working unchanged.
list(REMOVE_ITEM SERVER_ROOT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/api_gateway_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ws_gateway_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/matchmaker_main.cpp"
)

# SERVER_ROOT_SOURCES minus the two single-process-only entry-point files
# (main.cpp/server.cpp) -- e.g. game_session.cpp/player_registry.cpp, needed
# by every executable (including the three Phase 5 process-split ones below
# and KungFuChessServerTests further down) but not owning their own main().
set(SERVER_ROOT_SOURCES_FOR_TESTS ${SERVER_ROOT_SOURCES})
list(REMOVE_ITEM SERVER_ROOT_SOURCES_FOR_TESTS
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/server.cpp"
)

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(PQXX libpqxx)
endif()

file(GLOB_RECURSE TEST_SOURCES "tests/unit/*.cpp")
if(NOT PQXX_FOUND)
    list(REMOVE_ITEM DB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/db/postgres_player_account_store.cpp")
    list(REMOVE_ITEM MATCHES_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/matches/postgres_game_history_store.cpp")
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/DB/test_postgres_player_account_store.cpp")
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_postgres_game_history_store.cpp")
endif()
if(NOT REDIS_TESTS)
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/lobby/test_redis_lobby_store.cpp")
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_redis_game_index_store.cpp")
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/lobby/test_redis_matchmaker_queue_store.cpp")
endif()
if(NOT NATS_TESTS)
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/bus/test_nats_publisher_integration.cpp")
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/bus/test_game_match_nats_integration.cpp")
endif()
if(NOT PROCESS_SPLIT_TESTS)
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/network/test_process_split_integration.cpp")
    list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/network/test_rooms_rest_integration.cpp")
endif()

add_executable(KungFuChessServer ${SERVER_ROOT_SOURCES} ${LOGIC_SOURCES} ${SHARED_BUS_SOURCES} ${SHARED_LOGGING_SOURCES} ${NETWORK_SOURCES} ${DB_SOURCES} ${RATING_SOURCES} ${MATCHES_SOURCES} ${LOBBY_SOURCES} ${AUTH_SOURCES} ${ROOMS_SOURCES} ${SQLITE3_ROOT}/sqlite3.c)
target_include_directories(KungFuChessServer PRIVATE ${LOGIC_ROOT} ${SHARED_ROOT} ${CMAKE_CURRENT_SOURCE_DIR} ${SQLITE3_ROOT})
target_link_libraries(KungFuChessServer PRIVATE ixwebsocket hiredis httplib::httplib nats_static)

if(PQXX_FOUND)
    target_include_directories(KungFuChessServer PRIVATE ${PQXX_INCLUDE_DIRS})
    target_link_libraries(KungFuChessServer PRIVATE ${PQXX_LIBRARIES})
endif()

# The three Phase 5 process-split executables share this code (game logic,
# DB, network, matches, lobby, auth, rooms) with KungFuChessServer. Built
# ONCE as an OBJECT library rather than compiled separately per executable --
# an earlier version of this file listed these sources directly on each of
# the three add_executable() calls below, which meant every one of these
# ~90 files was compiled three extra times (once per new executable, on top
# of KungFuChessServer/KungFuChessServerTests already compiling them once
# each) -- 5x the necessary compilation, which is what made the Docker build
# balloon from ~160-300s to 600s+ and destabilize the daemon. See
# Implementation_Roadmap.md Phase 5 Decision Log.
add_library(CommonObjects OBJECT ${SERVER_ROOT_SOURCES_FOR_TESTS} ${LOGIC_SOURCES} ${SHARED_BUS_SOURCES} ${SHARED_LOGGING_SOURCES} ${NETWORK_SOURCES} ${DB_SOURCES} ${RATING_SOURCES} ${MATCHES_SOURCES} ${LOBBY_SOURCES} ${AUTH_SOURCES} ${ROOMS_SOURCES} ${SQLITE3_ROOT}/sqlite3.c)
target_include_directories(CommonObjects PUBLIC ${LOGIC_ROOT} ${SHARED_ROOT} ${CMAKE_CURRENT_SOURCE_DIR} ${SQLITE3_ROOT})
target_link_libraries(CommonObjects PUBLIC ixwebsocket hiredis httplib::httplib nats_static)
if(PQXX_FOUND)
    target_include_directories(CommonObjects PUBLIC ${PQXX_INCLUDE_DIRS})
    target_link_libraries(CommonObjects PUBLIC ${PQXX_LIBRARIES})
    # Lets ApiGateway/WsGateway pick PostgresPlayerAccountStore over
    # SqlitePlayerAccountStore at compile time -- only defined where libpqxx
    # actually built (Docker/Linux), never on native Windows.
    target_compile_definitions(CommonObjects PUBLIC HAS_POSTGRES)
endif()

add_executable(ApiGateway api_gateway_main.cpp)
target_link_libraries(ApiGateway PRIVATE CommonObjects)

add_executable(WsGateway ws_gateway_main.cpp)
target_link_libraries(WsGateway PRIVATE CommonObjects)

add_executable(MatchmakerServer matchmaker_main.cpp)
target_link_libraries(MatchmakerServer PRIVATE CommonObjects)

enable_testing()

add_executable(KungFuChessServerTests
    tests/test_main.cpp
    tests/catch2/catch_amalgamated.cpp
    ${NETWORK_SOURCES}
    ${SERVER_ROOT_SOURCES_FOR_TESTS}
    ${TEST_SOURCES}
    ${LOGIC_SOURCES}
    ${SHARED_BUS_SOURCES}
    ${SHARED_LOGGING_SOURCES}
    ${DB_SOURCES}
    ${RATING_SOURCES}
    ${MATCHES_SOURCES}
    ${LOBBY_SOURCES}
    ${AUTH_SOURCES}
    ${ROOMS_SOURCES}
    ${SQLITE3_ROOT}/sqlite3.c
)

target_include_directories(KungFuChessServerTests PRIVATE ${LOGIC_ROOT} ${SHARED_ROOT} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests ${SQLITE3_ROOT})
target_link_libraries(KungFuChessServerTests PRIVATE ixwebsocket hiredis httplib::httplib nats_static)

if(PQXX_FOUND)
    target_include_directories(KungFuChessServerTests PRIVATE ${PQXX_INCLUDE_DIRS})
    target_link_libraries(KungFuChessServerTests PRIVATE ${PQXX_LIBRARIES})
endif()

add_test(NAME KungFuChessServerTests COMMAND KungFuChessServerTests)
