# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CM8600Host_autogen"
  "CMakeFiles\\CM8600Host_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\CM8600Host_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\cm8600_core_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\cm8600_core_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\cm8600_selftest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\cm8600_selftest_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\cm8600_sim_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\cm8600_sim_autogen.dir\\ParseCache.txt"
  "cm8600_core_autogen"
  "cm8600_selftest_autogen"
  "cm8600_sim_autogen"
  "third_party\\snap7\\CMakeFiles\\snap7_autogen.dir\\AutogenUsed.txt"
  "third_party\\snap7\\CMakeFiles\\snap7_autogen.dir\\ParseCache.txt"
  "third_party\\snap7\\snap7_autogen"
  )
endif()
