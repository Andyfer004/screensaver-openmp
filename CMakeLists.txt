cmake_minimum_required(VERSION 3.20)
project(openmp_screensaver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(ENABLE_OMP "Enable OpenMP" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

if(ENABLE_OMP)
  find_package(OpenMP)
endif()

include_directories(
  ${SDL2_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
)

link_directories(${SDL2_LIBRARY_DIRS})

# ---- core lib ----
add_library(core
  src/core/app_config.cpp
  src/core/cli.cpp
  src/core/fps_counter.cpp
  src/core/random.cpp
  src/core/field.cpp
  src/core/screensaver.cpp
  # opcional/placeholder:
  src/core/entity.cpp
)

target_link_libraries(core PUBLIC ${SDL2_LIBRARIES})
if(ENABLE_OMP AND OpenMP_CXX_FOUND)
  target_compile_definitions(core PUBLIC HAVE_OPENMP=1)
  target_link_libraries(core PUBLIC OpenMP::OpenMP_CXX)
endif()

# ---- executables ----
add_executable(screensaver_seq src/seq/main_seq.cpp)
target_link_libraries(screensaver_seq PRIVATE core)

add_executable(screensaver_omp src/omp/main_omp.cpp)
target_link_libraries(screensaver_omp PRIVATE core)
target_compile_definitions(screensaver_omp PRIVATE USE_OPENMP=1)

# Flags sugeridos
if (APPLE)
  target_compile_options(core PRIVATE ${SDL2_CFLAGS_OTHER})
endif()