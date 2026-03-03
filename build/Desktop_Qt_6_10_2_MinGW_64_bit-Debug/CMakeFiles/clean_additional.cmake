# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\Image_Stitcher_Auto_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\Image_Stitcher_Auto_autogen.dir\\ParseCache.txt"
  "Image_Stitcher_Auto_autogen"
  )
endif()
