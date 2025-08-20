#include "log.h"

// 简单屏幕输出测试
void simple_test() {
    d_mod_1_debug("This is a debug message from mod 1");
    d_mod_1_info("This is an info message from mod 1");
    d_mod_1_warn("This is a warning message from mod 1");
    d_mod_1_error("This is an error message from mod 1");

    d_mod_2_debug("This is a debug message from mod 2");
    d_mod_2_info("This is an info message from mod 2");
    d_mod_2_warn("This is a warning message from mod 2");
    d_mod_2_error("This is an error message from mod 2");
}

int main() {
    simple_test();
    
    return 0;
}