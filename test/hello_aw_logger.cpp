#ifndef TEST__HELLO_AW_LOGGER_CPP
#define TEST__HELLO_AW_LOGGER_CPP

// aw_logger library
#include "aw_logger/aw_logger.hpp"

int main()
{
    for (int i = 0; i < 10; i++)
    {
        AW_LOG_INFO(aw_logger::getLogger(), "Hello aw_logger!");
    }
    return 0;
}

#endif //! TEST__HELLO_AW_LOGGER_CPP
