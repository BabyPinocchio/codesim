cmake_minimum_required (VERSION 3.10.2)
project(CodeDiff CXX)
include(Config.cmake)

set(CMAKE_CXX_COMPILER "clang++")

# set llvm options
set(LLVMConfig llvm-config-6.0)
llvm_config(GET_INCDIR LLVMIncDir GET_LIBDIR LLVMLibDir)

# common compile options
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${LLVMIncDir})
link_directories(${LLVMLibDir})

add_clang_target(
  codesim FILES "src/*.cpp" DEPENDS
  clangTooling
  clangToolingASTDiff
  clangFrontend
  clangDriver
  clangSerialization
  clangParse
  clangSema
  clangAnalysis
  clangEdit
  clangAST
  clangLex
  clangBasic
  clang
  LLVM
)

target_compile_options(codesim PRIVATE "-ggdb3")
