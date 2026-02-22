cmake_minimum_required(VERSION 3.16)
project(lcd LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED libgpiod)
pkg_check_modules(FREETYPE REQUIRED freetype2)

# Tools library (generic SPI and GPIO functions)
add_library(tools
    src/spi_linux.cpp
    src/gpio_gpiod.cpp
)
target_include_directories(tools PUBLIC include ${GPIOD_INCLUDE_DIRS})
target_link_libraries(tools PUBLIC ${GPIOD_LIBRARIES})
target_compile_options(tools PRIVATE -Wall -Wextra -Wpedantic)

# Display library (display-specific code)
add_library(lcd_display
    src/st7565.cpp
    src/ili9488.cpp
    src/graphics.cpp
    src/ft_text.cpp
    src/four_line_display.cpp
)
target_include_directories(lcd_display PUBLIC include ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(lcd_display PUBLIC tools ${FREETYPE_LIBRARIES})
target_compile_options(lcd_display PRIVATE -Wall -Wextra -Wpedantic)

# Demo executable
add_executable(lcd src/main_demo.cpp)
target_link_libraries(lcd PRIVATE lcd_display tools)

# Testing with GoogleTest
option(BUILD_TESTS "Build the tests" ON)

if(BUILD_TESTS)
    # Download and configure GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    # Add test executable
    add_executable(test_four_line_display
        tests/test_four_line_display.cpp
    )
    add_executable(test_ili9488
        tests/test_ili9488.cpp
    )
    add_executable(test_ft_text
        tests/test_ft_text.cpp
    )
    target_link_libraries(test_four_line_display
        PRIVATE
        lcd_display
        tools
        GTest::gtest
        GTest::gtest_main
    )
    target_link_libraries(test_ili9488
        PRIVATE
        lcd_display
        tools
        GTest::gtest
        GTest::gtest_main
    )
    target_link_libraries(test_ft_text
        PRIVATE
        lcd_display
        tools
        GTest::gtest
        GTest::gtest_main
    )

    # Discover tests
    include(GoogleTest)
    gtest_discover_tests(test_four_line_display)
    gtest_discover_tests(test_ili9488)
    gtest_discover_tests(test_ft_text)
endif()
