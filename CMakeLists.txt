cmake_minimum_required(VERSION 3.30 FATAL_ERROR) # v3.30 is required for C++23 `import std;` support
project(terminal LANGUAGES CXX)

# ==================================================
# = Compilation Settings
# ==================================================

set(TERMINAL_BUILD_EXAMPLES ON CACHE BOOL "Build examples")

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
        GIT_TAG 07fd3e685ade6275f718913720345134ed2e8242
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

# ------------------- Libraries -------------------
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

add_library(state STATIC)
target_sources(state
        PUBLIC FILE_SET cxx_modules
        TYPE CXX_MODULES
        FILES src/state/state.ccm
        PRIVATE
        src/state/trade.cc
        src/state/order_book.cc
)
target_link_libraries(state PUBLIC
        exchange
        nlohmann_json::nlohmann_json
        rpp
        spdlog::spdlog
)

add_library(ui.component STATIC)
target_sources(ui.component
        PUBLIC FILE_SET cxx_modules
        TYPE CXX_MODULES
        FILES
        src/ui/component/component.ccm
        PRIVATE
        src/ui/component/scroller.cc
        src/ui/component/table.cc
)
target_link_libraries(ui.component PUBLIC rpp ftxui::component)

add_library(ui.widget STATIC)
target_sources(ui.widget
        PUBLIC FILE_SET cxx_modules
        TYPE CXX_MODULES
        FILES
        src/ui/widget/widget.ccm
        PRIVATE
        src/ui/widget/market_trades.cc
        src/ui/widget/order_book.cc
)
target_link_libraries(ui.widget PUBLIC ui.component state)

add_library(ui STATIC)
target_sources(ui
        PUBLIC FILE_SET cxx_modules
        TYPE CXX_MODULES
        FILES
        src/ui/ui.ccm
        PRIVATE
)
target_link_libraries(ui PUBLIC ui.widget)

# ------------------- Main Binary -------------------
add_executable(${PROJECT_NAME} src/main.cc)
target_include_directories(${PROJECT_NAME} PRIVATE src ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE exchange state ui)

# ------------------- Examples -------------------
add_subdirectory(examples)

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
