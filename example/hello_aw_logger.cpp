#ifndef HELLO_AW_LOGGER_CPP
#define HELLO_AW_LOGGER_CPP

// aw_logger library
#include "aw_logger/aw_logger.hpp"

int main(int argc, char** argv)
{
    AW_LOG_INFO(aw_logger::getLogger("hello"), "Hello, aw_logger!");
    return 0;
}

#endif //! HELLO_AW_LOGGER_CPP
