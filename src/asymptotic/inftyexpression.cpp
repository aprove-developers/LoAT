#include "inftyexpression.hpp"

#include <iostream>

const int DirectionSize = 5;
const char* DirectionNames[] = { "+", "-", "+!", "-!", "+/+!"};


InftyExpression::InftyExpression(const Expression &other, Direction dir)
    : Expression(other) {
    setDirection(dir);
}


void InftyExpression::setDirection(Direction dir) {
    direction = dir;
}


Direction InftyExpression::getDirection() const {
    return direction;
}


bool InftyExpression::isTriviallyUnsatisfiable() const {
    if (this->isNumeric()) {
        if (direction == POS_INF || direction == NEG_INF) {
            return true;

        } else if ((direction == POS_CONS || direction == POS)
                   && (info(GiNaC::info_flags::negative) || is_zero())) {
            return true;

        } else if (direction == NEG_CONS && info(GiNaC::info_flags::nonnegative)) {
            return true;
        }
    }

    return false;
}

std::ostream& operator<<(std::ostream &os, const InftyExpression &ie) {
    os << static_cast<const Expression &>(ie) << " ("
       << DirectionNames[ie.getDirection()] << ")";

    return os;
}
