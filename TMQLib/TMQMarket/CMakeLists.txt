TMQ_define_dynalib(TMQMarket)
TMQ_add_lib_include_dir(TMQMarket TMQUtils)
append_link_libraries(TMQMarket TMQUtils)

TMQ_define_dynalib_tests(TMQMarket)
TMQ_add_lib_include_dir(TMQMarket TMQUtils)
append_link_libraries(TMQMarket TMQUtils)

TMQ_define_dynalib_bench(TMQMarket)
TMQ_add_lib_include_dir(TMQMarket TMQUtils)
append_link_libraries(TMQMarket TMQUtils)