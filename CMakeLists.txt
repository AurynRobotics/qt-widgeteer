cmake_minimum_required(VERSION 3.18)
project(widgeteer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Export compile commands for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings as errors
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-unused-parameter  # Qt MOC generates unused params
  )
elseif(MSVC)
  add_compile_options(/W4 /WX)
endif()

# Debug build options: Sanitizers and Code Coverage
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Debug build: Enabling sanitizers and code coverage")

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Code coverage
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)

    # AddressSanitizer
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)

    # UndefinedBehaviorSanitizer
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
  endif()
endif()

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Test Network WebSockets)

# Enable Qt MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Library sources
set(WIDGETEER_SOURCES
  src/Protocol.cpp
  src/ElementFinder.cpp
  src/UIIntrospector.cpp
  src/EventInjector.cpp
  src/Synchronizer.cpp
  src/CommandExecutor.cpp
  src/ActionRecorder.cpp
  src/EventBroadcaster.cpp
  src/Server.cpp
  src/WidgeteerClient.cpp
)

set(WIDGETEER_HEADERS
  include/widgeteer/Export.h
  include/widgeteer/Protocol.h
  include/widgeteer/Result.h
  include/widgeteer/ElementFinder.h
  include/widgeteer/UIIntrospector.h
  include/widgeteer/EventInjector.h
  include/widgeteer/Synchronizer.h
  include/widgeteer/CommandExecutor.h
  include/widgeteer/ActionRecorder.h
  include/widgeteer/EventBroadcaster.h
  include/widgeteer/Server.h
  include/widgeteer/WidgeteerClient.h
)

# Create the library
add_library(widgeteer ${WIDGETEER_SOURCES} ${WIDGETEER_HEADERS})

target_include_directories(widgeteer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(widgeteer PUBLIC
  Qt6::Core
  Qt6::Widgets
  Qt6::Gui
  Qt6::Test
  Qt6::Network
  Qt6::WebSockets
)

# Build sample application
option(WIDGETEER_BUILD_SAMPLE "Build sample application" ON)
if(WIDGETEER_BUILD_SAMPLE)
  add_subdirectory(sample)
endif()

# Build tests
option(WIDGETEER_BUILD_TESTS "Build unit tests" ON)
if(WIDGETEER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/cpp)
endif()

# Install targets
install(TARGETS widgeteer
  EXPORT widgeteer-targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/widgeteer
  DESTINATION include
)

install(EXPORT widgeteer-targets
  FILE widgeteer-config.cmake
  NAMESPACE widgeteer::
  DESTINATION lib/cmake/widgeteer
)
