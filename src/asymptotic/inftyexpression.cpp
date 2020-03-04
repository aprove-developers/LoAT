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
                   && isNumeric() && !toNumeric().is_positive()) {
            return true;

        } else if (direction == NEG_CONS && isNumeric() && toNumeric().is_nonneg_integer()) {
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
