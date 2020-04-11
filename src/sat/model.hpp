#ifndef SAT_MODEL_HPP
#define SAT_MODEL_HPP

#include <map>

namespace sat {

class Model
{
public:

    Model(std::map<uint, bool> vars);

    bool get(uint i) const;
    bool contains(uint i) const;

    friend std::ostream& operator<<(std::ostream &s, const Model &e);

private:

    std::map<uint, bool> vars;

};

}

#endif // SAT_MODEL_HPP
