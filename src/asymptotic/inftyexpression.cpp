#include "inftyexpression.h"

#include <iostream>

const char* DirectionNames[] = { "+", "-", "+!", "-!", "+/+!"};

InftyExpression::InftyExpression(Direction dir)
    : direction(dir) {
}


InftyExpression::InftyExpression(const GiNaC::basic &other, Direction dir)
    : Expression(other) {
    setDirection(dir);
}


InftyExpression::InftyExpression(const GiNaC::ex &other, Direction dir)
    : Expression(other) {
    setDirection(dir);
}


void InftyExpression::setDirection(Direction dir) {
    direction = dir;
}


Direction InftyExpression::getDirection() const {
    return direction;
}

std::ostream& operator<<(std::ostream &os, const InftyExpression &ie) {
    os << static_cast<const Expression &>(ie) << " ("
       << DirectionNames[ie.getDirection()] << ")";

    return os;
}