TMQ_define_dynalib(TMQMarket)
target_link_libraries(TMQMarket TMQCore)

TMQ_define_dynalib_swig(TMQMarket python)
TMQ_define_dynalib_swig(TMQMarket csharp)

TMQ_define_dynalib_tests(TMQMarket)
target_link_libraries(TMQMarket TMQCore)

TMQ_define_dynalib_bench(TMQMarket)
target_link_libraries(TMQMarket TMQCore)