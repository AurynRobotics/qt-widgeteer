cmake_minimum_required(VERSION 3.18)
project(widgeteer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Test Network)

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
  src/Server.cpp
)

set(WIDGETEER_HEADERS
  include/widgeteer/Export.h
  include/widgeteer/Protocol.h
  include/widgeteer/ElementFinder.h
  include/widgeteer/UIIntrospector.h
  include/widgeteer/EventInjector.h
  include/widgeteer/Synchronizer.h
  include/widgeteer/CommandExecutor.h
  include/widgeteer/Server.h
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
)

# Build sample application
option(WIDGETEER_BUILD_SAMPLE "Build sample application" ON)
if(WIDGETEER_BUILD_SAMPLE)
  add_subdirectory(sample)
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
