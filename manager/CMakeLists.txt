find_package(Pylint)

if(Pylint_FOUND)
    add_custom_target(pylint ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/pylint.sh ${PYLINT_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/nica_manager.py ${CMAKE_CURRENT_SOURCE_DIR}/nica.py)
endif()
