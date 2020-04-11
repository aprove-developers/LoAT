#include "satfactory.hpp"
#include "yices/yices.hpp"

namespace sat {

std::unique_ptr<Sat> SatFactory::solver(uint timeout) {
    std::unique_ptr<Sat> res(new Yices());
    res->setTimeout(timeout);
    return res;
}

}
