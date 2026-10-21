find_package(Qt6 REQUIRED COMPONENTS Test)

function(rb_add_test name label)
  add_executable(${name} ${name}.cpp ${ARGN})
  # The test classes live in the .cpp (`#include "<name>.moc"`).
  set_target_properties(${name} PROPERTIES AUTOMOC ON)
  target_link_libraries(${name} PRIVATE rclone-browser-core Qt6::Test rb_warnings)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${label} TIMEOUT 300
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;RB_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures;RB_TEST_RCLONE=${RB_TEST_RCLONE}")
endfunction()

rb_add_test(tst_utils unit)
if(RB_TEST_RCLONE)
  rb_add_test(tst_e2e_rclone_smoke e2e)
endif()
