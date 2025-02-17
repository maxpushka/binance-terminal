cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
project(terminal LANGUAGES CXX)

# ==================================================
# = Compilation Settings
# ==================================================

# Set the compile flags
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdangling-gsl -Wreturn-stack-address")

# ==================================================
# = Dependencies Setup
# ==================================================
include(FetchContent)

# ------------------- Boost.Asio & Boost.Beast -------------------
find_package(Boost 1.85 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# ------------------- nlohmann/json -------------------
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# ------------------- FTXUI -------------------
# FetchContent_Declare(ftxui
#   GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
#   GIT_TAG d75108e96058b6e00da6306a7d99c9555f645434 
# )
# FetchContent_MakeAvailable(ftxui)

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

# ------------------- Main Binary -------------------
add_executable(${PROJECT_NAME}
        src/main.cc
        src/exchange/websocket.cc
        src/exchange/websocket_streams.cc
        src/trade.cc
        src/orderbook.cc
)
target_include_directories(${PROJECT_NAME} PRIVATE src ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
)

# ------------------- Tests -------------------
# enable_testing()
# add_executable(${PROJECT_NAME}_tests
# )
# target_include_directories(${PROJECT_NAME}_tests PRIVATE src)
# target_link_libraries(${PROJECT_NAME}_tests
#         GTest::gtest_main
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
    # set_target_property(${PROJECT_NAME}_tests PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else ()
    message(WARNING "IPO / LTO not supported: <${error}>")
endif ()

