#include "yiceserror.hpp"

#include <stdio.h>
#include <yices.h>

YicesError::YicesError() : std::exception() {
    yices_print_error(stderr);
}
