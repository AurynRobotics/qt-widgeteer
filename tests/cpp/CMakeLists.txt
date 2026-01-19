# Qt Test based unit tests

# Helper macro to add a test
macro(add_widgeteer_test TEST_NAME)
  add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
  target_link_libraries(${TEST_NAME} PRIVATE
    widgeteer
    Qt6::Test
    Qt6::Widgets
  )
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  set_tests_properties(${TEST_NAME} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )
endmacro()

# Test targets
add_widgeteer_test(test_protocol)
add_widgeteer_test(test_action_recorder)
add_widgeteer_test(test_event_broadcaster)
add_widgeteer_test(test_element_finder)
add_widgeteer_test(test_ui_introspector)
add_widgeteer_test(test_synchronizer)
add_widgeteer_test(test_event_injector)
add_widgeteer_test(test_command_executor)
add_widgeteer_test(test_server)
add_widgeteer_test(test_widgeteer_client)
