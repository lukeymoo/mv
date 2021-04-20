#ifndef HEADERS_MVEXCEPTION_H_
#define HEADERS_MVEXCEPTION_H_

#include <string>
#include <sstream>

namespace mv
{
    class BException
    {
    public:
        BException(int line, std::string file, std::string message);
        ~BException();

        std::string get_type(void);
        std::string get_error_description(void);

    private:
        int line;
        std::string file;

    protected:
        std::string type;             // changes based on exception source
        std::string error_description; // windows translated description of HR
    };
};

#endif