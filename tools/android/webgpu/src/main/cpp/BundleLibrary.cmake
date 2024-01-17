function(bundle_library input_target output_target)

  function(get_dependencies input_target)
    get_target_property(alias ${input_target} ALIASED_TARGET)
    if(TARGET ${alias})
      set(input_target ${alias})
    endif()
    if(${input_target} IN_LIST all_dependencies)
      return()
    endif()
    list(APPEND all_dependencies ${input_target})

    get_target_property(link_libraries ${input_target} LINK_LIBRARIES)
    get_target_property(interface_link_libraries ${input_target} INTERFACE_LINK_LIBRARIES)
    foreach(dependency IN LISTS link_libraries interface_link_libraries)
      if(TARGET ${dependency})
        get_dependencies(${dependency})
      endif()
    endforeach()

    set(all_dependencies ${all_dependencies} PARENT_SCOPE)
  endfunction()

  add_library(${output_target} STATIC placeholder.cpp)
  add_dependencies(${output_target} ${input_target})

  set(mri_file ${CMAKE_BINARY_DIR}/${output_target}.ar.in)

  file(WRITE ${mri_file} "CREATE ${CMAKE_CURRENT_BINARY_DIR}/lib${output_target}.a\n")

  get_dependencies(${input_target})
  foreach(dependency IN LISTS all_dependencies)
    get_target_property(type ${dependency} TYPE)
    if(${type} STREQUAL "STATIC_LIBRARY")
      file(APPEND ${mri_file} "ADDLIB $<TARGET_FILE:${dependency}>\n")
    endif()
  endforeach()

  file(APPEND ${mri_file} "SAVE\nEND\n")

  file(GENERATE
       OUTPUT ${CMAKE_BINARY_DIR}/${output_target}.ar
       INPUT ${mri_file})

  add_custom_command(TARGET ${output_target}
                     POST_BUILD
                     COMMAND ${CMAKE_AR} -M < ${CMAKE_BINARY_DIR}/${output_target}.ar)

endfunction()
