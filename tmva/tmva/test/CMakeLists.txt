############################################################################
# CMakeLists.txt file for building ROOT TMVA tests.
# @author Kim Albertsson
############################################################################

set(Libraries TMVA)
include_directories(${ROOT_INCLUDE_DIRS})

# Tests using google test
ROOT_ADD_GTEST(TestRandomGenerator
               TestRandomGenerator.cxx
               LIBRARIES ${Libraries})

project(tmva-tests)
find_package(ROOT REQUIRED)


