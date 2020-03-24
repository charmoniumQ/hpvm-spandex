file(GLOB entries *)
foreach(entry ${entries})
  if(IS_DIRECTORY ${entry} AND EXISTS ${entry}/CMakeLists.txt)
    if((NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/compiler-rt) AND
       (NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/dragonegg) AND
       (NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/libcxx) AND
       (NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/libcxxabi) AND
       (NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/libunwind) AND
       (NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/test-suite) AND
       (NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/parallel-libs) AND
       (NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/openmp) AND
       (NOT ${entry} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR}/debuginfo-tests))
      get_filename_component(entry_name "${entry}" NAME)
      add_llvm_external_project(${entry_name})
    endif()
  endif()
endforeach(entry)
