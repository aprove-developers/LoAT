/*  This file is part of LoAT.
 *  Copyright (c) 2015-2016 Matthias Naaf, RWTH Aachen University, Germany
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses>.
 */

#ifndef PROOFOUTPUT_H
#define PROOFOUTPUT_H

#include <streambuf>
#include <ostream>
#include <string>

#include <sstream>
#include <iomanip>

#include "../config.hpp"
#include "../util/timeout.hpp" // for timestamps


// Stores a StreambufIndenter instance to control indention of the stream.
// Also allows colored output with ANSI escape codes.
class ProofOutput {
public:
    enum Style {
        Section,
        Headline,
        Warning,
        Result,
        None
    };

    bool setEnabled(bool on) {
        bool res = enabled;
        enabled = on;
        return res;
    }

    void appendLine(const std::string &s) {
        appendLine(Style::None, s);
    }

    void appendLine(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        appendLine(str.str());
    }

    void appendLine(const Style &style, const std::string &s) {
        if (enabled) {
            proof.push_back({style, s});
        }
    }

    void newline() {
        appendLine(std::stringstream());
    }

    void headline(const std::string &s) {
        newline();
        appendLine(Headline, s);
    }

    // print given string in headline style with spacing
    void headline(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        headline(str.str());
    }

    void section(const std::string &s) {
        newline();
        appendLine(Section, s);
    }

    // print given string in section style with spacing
    void section(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        section(str.str());
    }

    void result(const std::string &s) {
        appendLine(Result, s);
    }

    // print given string in section style with spacing
    void result(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        result(str.str());
    }

    void warning(const std::string &s) {
        newline();
        appendLine(Warning, s);
        newline();
    }

    // print given string in warning style with spacing
    void warning(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        warning(str.str());
    }

    void print() {
        for (const ProofLine &l: proof) {
            if (Config::Output::Colors) {
                switch (l.style) {
                case None: std::cout << Config::Color::None;
                    break;
                case Result: std::cout << Config::Color::Result;
                    break;
                case Section: std::cout << Config::Color::Section;
                    break;
                case Warning: std::cout << Config::Color::Warning;
                    break;
                case Headline: std::cout << Config::Color::Headline;
                    break;
                }
            }
            std::cout << l.s << std::endl;
        }
    }

private:

    struct ProofLine {
        const Style style;
        const std::string s;
    };

    std::vector<ProofLine> proof;
    bool enabled = true;
};

#endif // PROOFOUTPUT_H
