include_directories(./include/)
add_subdirectory(lib)
add_subdirectory(projects)
add_subdirectory(tools)

add_subdirectory(test)

# Add a global check rule now that all subdirectories have been traversed
# and we know the total set of lit testsuites.
get_property(LLVM_LIT_TESTSUITES GLOBAL PROPERTY LLVM_LIT_TESTSUITES)
get_property(LLVM_LIT_PARAMS GLOBAL PROPERTY LLVM_LIT_PARAMS)
get_property(LLVM_LIT_DEPENDS GLOBAL PROPERTY LLVM_LIT_DEPENDS)
get_property(LLVM_LIT_EXTRA_ARGS GLOBAL PROPERTY LLVM_LIT_EXTRA_ARGS)
get_property(LLVM_ADDITIONAL_TEST_TARGETS
                 GLOBAL PROPERTY LLVM_ADDITIONAL_TEST_TARGETS)

add_lit_target(hpmv-check-all
"Running all regression tests"
${LLVM_LIT_TESTSUITES}
PARAMS ${LLVM_LIT_PARAMS}
DEPENDS ${LLVM_LIT_DEPENDS} ${LLVM_ADDITIONAL_TEST_TARGETS}
ARGS ${LLVM_LIT_EXTRA_ARGS}
)

