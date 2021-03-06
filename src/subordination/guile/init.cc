#include <libguile.h>

#include <cstdlib>

#include <subordination/guile/init.hh>
#include <subordination/guile/kernel.hh>

namespace {

    sbn::guile::main_function_type nested_main;

    void sbn_init(void*) {
        using namespace sbn::guile;
        kernel_define();
        std::atexit([]() { scm_gc(); scm_run_finalizers(); });
    }

    void inner_main(void*, int argc, char** argv) {
        scm_c_define_module("sbn", sbn_init, nullptr);
        scm_c_use_module("sbn");
        nested_main(argc, argv);
    }

}

void sbn::guile::main(int argc, char* argv[], main_function_type nested_main) {
    ::nested_main = nested_main;
    scm_boot_guile(argc, argv, inner_main, 0);
}
