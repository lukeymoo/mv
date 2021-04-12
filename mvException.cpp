#include "mvException.h"

namespace mv
{
    BException::BException(int l, std::string f, std::string description)
    {
        line = l;
        file = f;
        errorDescription = description;
    }

    BException::~BException(void)
    {
    }

    std::string BException::getType(void)
    {
        return type;
    }

    std::string BException::getErrorDescription(void)
    {
        std::ostringstream oss;
        oss << std::endl
            << errorDescription << std::endl
            << "Line: " << line << std::endl
            << "File : " << file << std::endl;
        errorDescription = oss.str();
        return errorDescription;
    }
}