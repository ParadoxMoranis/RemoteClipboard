function(rc_add_test test_name)
    add_executable(${test_name} ${ARGN})
    set_target_properties(${test_name} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

rc_add_test(frame_parser_test frame_parser_test.cpp)
target_link_libraries(frame_parser_test PRIVATE RemoteClipboard::ServerCommon)

rc_add_test(protocol_contract_test protocol_contract_test.cpp)
target_link_libraries(protocol_contract_test PRIVATE RemoteClipboard::Protocol)

rc_add_test(sqlite_storage_test sqlite_storage_test.cpp)
target_link_libraries(sqlite_storage_test PRIVATE RemoteClipboard::ServerCommon)

rc_add_test(tcpserver_integration_test tcpserver_integration_test.cpp)
target_link_libraries(tcpserver_integration_test PRIVATE RemoteClipboard::ServerCommon)

set_tests_properties(tcpserver_integration_test PROPERTIES TIMEOUT 30)
