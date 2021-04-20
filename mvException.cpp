#include "mvException.h"

namespace mv
{
    BException::BException(int l, std::string f, std::string description)
    {
        line = l;
        file = f;
        error_description = description;
    }

    BException::~BException(void)
    {
    }

    std::string BException::get_type(void)
    {
        return type;
    }

    std::string BException::get_error_description(void)
    {
        std::ostringstream oss;
        oss << std::endl
            << error_description << std::endl
            << "Line: " << line << std::endl
            << "File : " << file << std::endl;
        error_description = oss.str();
        return error_description;
    }
}