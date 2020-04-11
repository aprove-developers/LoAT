#ifndef YICESMANAGER_HPP
#define YICESMANAGER_HPP

#include <atomic>

class YicesManager
{

public:

    static void inc();
    static void dec();
    static void init();
    static void exit();

private:

    static std::atomic_uint running;

};

#endif // YICESMANAGER_HPP
