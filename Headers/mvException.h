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

        std::string getType(void);
        std::string getErrorDescription(void);

    private:
        int line;
        std::string file;

    protected:
        std::string type;             // changes based on exception source
        std::string errorDescription; // windows translated description of HR
    };
};

#endif