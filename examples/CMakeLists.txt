if(NOT TERMINAL_BUILD_EXAMPLES)
    return()
endif()

function(add_example source_file)
    # Convert source file path to unique binary name
    string(REPLACE "/" "_" binary_name ${source_file}) # replace / with _
    string(REGEX REPLACE "\\.[^.]*$" "" binary_name ${binary_name}) # strip file extension

    add_executable(${binary_name} ${source_file})
    target_link_libraries(${binary_name} PUBLIC ${ARGN})
    set_target_properties(${binary_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples)
endfunction()

add_example(exchange/market_trades.cc exchange state)
add_example(exchange/order_book.cc exchange state)
add_example(ui/market_trades.cc ui exchange)
add_example(ui/mid_price.cc ui exchange) # TODO
add_example(ui/order_book.cc ui exchange) # TODO
add_example(ui/scrollable_table.cc ui) # TODO
