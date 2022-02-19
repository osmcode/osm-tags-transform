#------------------------------------------------------------------------------
#
#  test/CMakeLists.txt
#
#------------------------------------------------------------------------------

function(check_output _name _command _reference _return_code)
    set(_cmd "$<TARGET_FILE:osm-tags-transform> ${_command}")
    add_test(
        NAME "check-output-${_name}"
        COMMAND ${CMAKE_COMMAND}
        -D cmd:FILEPATH=${_cmd}
        -D dir:PATH=${PROJECT_SOURCE_DIR}/test
        -D reference:FILEPATH=${PROJECT_SOURCE_DIR}/test/${_reference}
        -D output:FILEPATH=${PROJECT_BINARY_DIR}/test/${_dir}/cmd-output-${_name}
        -D return_code=${_return_code}
        -P ${CMAKE_SOURCE_DIR}/cmake/run_test_compare_output.cmake
    )
endfunction()

check_output(help -h output-help.txt 0)
check_output(nosource "-c ${CMAKE_SOURCE_DIR}/example-configs/nosource.lua input-source.opl -f opl" output-source.opl 0)
check_output(remove-buildings "-c ${CMAKE_SOURCE_DIR}/example-configs/remove-buildings.lua input-buildings.opl -f opl" output-buildings.opl 0)

add_test(NAME noconfig COMMAND $<TARGET_FILE:osm-tags-transform> -c no-config-file.lua input-source.opl -f opl)
set_tests_properties(noconfig PROPERTIES WILL_FAIL true)

#------------------------------------------------------------------------------
