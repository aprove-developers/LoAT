#ifndef YICESERROR_HPP
#define YICESERROR_HPP

#include <exception>

class YicesError : public std::exception {
public:
    YicesError();
};

#endif // YICESERROR_HPP
