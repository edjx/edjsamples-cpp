#include <edjx/logger.hpp>

using edjx::logger::info;

int main(void) {
    info("Hello World!");
}

//
// edjExecutor calls init() instead of _start()
// (constructors of global objects are not called if _start() is not executed)
//
extern "C" void _start(void);

__attribute__((export_name("init")))
extern "C" void init(void) {
    _start();
}
