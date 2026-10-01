cmake_minimum_required(VERSION 3.16)
project(gestionnaire_eleves
        VERSION 1.0.0
        DESCRIPTION "Backend C++ de gestion des eleves"
        LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options globales
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_TESTING "Compiler les tests" ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Dependances vendorisees (aucun telechargement necessaire)
# ---------------------------------------------------------------------------
set(THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# -- SQLite 3 (amalgamation) ------------------------------------------------
add_library(sqlite3 STATIC ${THIRD_PARTY}/sqlite3/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${THIRD_PARTY}/sqlite3)
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_OMIT_LOAD_EXTENSION)
if(NOT MSVC)
    target_compile_options(sqlite3 PRIVATE -w)
endif()
find_package(Threads REQUIRED)
target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# -- Headers only -----------------------------------------------------------
add_library(third_party_headers INTERFACE)
target_include_directories(third_party_headers SYSTEM INTERFACE ${THIRD_PARTY})

# ---------------------------------------------------------------------------
# Bibliotheque metier : gestionnaire_core
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(REMOVE_ITEM CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

if(CORE_SOURCES)
    add_library(gestionnaire_core STATIC ${CORE_SOURCES})
else()
    add_library(gestionnaire_core INTERFACE)
endif()

if(CORE_SOURCES)
    target_include_directories(gestionnaire_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(gestionnaire_core PUBLIC
        sqlite3 third_party_headers Threads::Threads)
else()
    target_include_directories(gestionnaire_core INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(gestionnaire_core INTERFACE
        sqlite3 third_party_headers Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Executable serveur
# ---------------------------------------------------------------------------
add_executable(gestionnaire_server ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(gestionnaire_server PRIVATE gestionnaire_core)

# ---------------------------------------------------------------------------
# Tests (doctest + CTest)
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
         ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
    if(TEST_SOURCES)
        add_executable(gestionnaire_tests ${TEST_SOURCES})
        target_link_libraries(gestionnaire_tests PRIVATE gestionnaire_core)
        add_test(NAME unit_and_integration COMMAND gestionnaire_tests)
    endif()
endif()

message(STATUS "=== ${PROJECT_NAME} ${PROJECT_VERSION} | ${CMAKE_BUILD_TYPE} | C++${CMAKE_CXX_STANDARD} ===")
