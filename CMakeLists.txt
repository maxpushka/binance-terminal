cmake_minimum_required(VERSION 3.30 FATAL_ERROR) # v3.30 is required for C++23 `import std;` support
project(terminal LANGUAGES CXX)

# ==================================================
# = Compilation Settings
# ==================================================

# Configure options
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdangling-gsl -Wreturn-stack-address")
set(CMAKE_CXX_SCAN_FOR_MODULES 1)

# ==================================================
# = Dependencies Setup
# ==================================================
include(FetchContent)

# ------------------- Boost.Asio & Boost.Beast -------------------
find_package(Boost 1.87 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# ------------------- nlohmann/json -------------------
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# ------------------- spdlog -------------------
FetchContent_Declare(spdlog URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.1.tar.gz)
FetchContent_MakeAvailable(spdlog)

# ------------------- ReactivePlusPlus -------------------
FetchContent_Declare(
        rpp
        GIT_REPOSITORY https://github.com/victimsnino/ReactivePlusPlus.git
        GIT_TAG v2.2.2
        GIT_SHALLOW 1
)
FetchContent_MakeAvailable(rpp)

# ------------------- FTXUI -------------------
FetchContent_Declare(ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
        GIT_TAG d75108e96058b6e00da6306a7d99c9555f645434
        GIT_SHALLOW 1
)
FetchContent_MakeAvailable(ftxui)

# ------------------- Google Test -------------------
# message(STATUS "Downloading github.com/google/googletest")
# FetchContent_Declare(
#         googletest
#         URL https://github.com/google/googletest/archive/c00fd25b71a17e645e4567fcb465c3fa532827d2.zip
# )
# FetchContent_MakeAvailable(googletest)

# ==================================================
# = Executable Definitions
# ==================================================

add_library(exchange STATIC)
target_sources(exchange
        PUBLIC FILE_SET cxx_modules
        TYPE CXX_MODULES
        FILES src/exchange/exchange.ccm
        PRIVATE
        src/exchange/websocket.cc
        src/exchange/websocket_streams.cc
        src/exchange/websocket_api.cc
)
target_link_libraries(exchange PUBLIC
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

# ------------------- Main Binary -------------------
add_executable(${PROJECT_NAME}
        src/main.cc
        src/trade.cc
        #        src/orderbook.cc
)
target_include_directories(${PROJECT_NAME} PRIVATE src ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE exchange ftxui::component rpp)

# ------------------- Tests -------------------
# enable_testing()
# add_executable(${PROJECT_NAME}_tests
# )
# target_include_directories(${PROJECT_NAME}_tests PRIVATE src)
# target_link_libraries(${PROJECT_NAME}_tests
#         GTest::gtest_main
#         Boost::system
# )
# target_compile_definitions(${PROJECT_NAME}_tests PRIVATE UNIT_TEST)
# include(GoogleTest)
# gtest_discover_tests(${PROJECT_NAME}_tests)

# ==================================================
# = Target Configurations
# ==================================================

# ------------------- Link Time Optimizations -------------------
include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if (supported)
    message(STATUS "IPO / LTO enabled")
    set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    # set_target_properties(${PROJECT_NAME}_tests PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
else ()
    message(WARNING "IPO / LTO not supported: <${error}>")
endif ()
