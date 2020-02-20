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
#include <iostream>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "../config.hpp"

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

    void concat(const ProofOutput &that) {
        proof.insert(proof.end(), that.proof.begin(), that.proof.end());
    }

    void append(const std::string &s) {
        append(Style::None, s);
    }

    void append(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        append(str.str());
    }

    void append(const Style &style, std::string s) {
        if (enabled) {
            std::vector<std::string> lines;
            boost::split(lines, s, boost::is_any_of("\n"));
            for (const std::string &l: lines) {
                proof.push_back({style, l});
            }
        }
    }

    void newline() {
        append(std::stringstream());
    }

    void headline(const std::string &s) {
        newline();
        append(Headline, s);
    }

    // print given string in headline style with spacing
    void headline(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        headline(str.str());
    }

    void section(const std::string &s) {
        newline();
        append(Section, s);
    }

    // print given string in section style with spacing
    void section(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        section(str.str());
    }

    void result(const std::string &s) {
        append(Result, s);
    }

    // print given string in section style with spacing
    void result(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        result(str.str());
    }

    void warning(const std::string &s) {
        newline();
        append(Warning, s);
        newline();
    }

    // print given string in warning style with spacing
    void warning(const std::ostream &s) {
        std::stringstream str;
        str << s.rdbuf();
        warning(str.str());
    }

    void print() {
        for (const auto &l: proof) {
            if (Config::Output::Colors) {
                switch (l.first) {
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
            std::cout << l.second << std::endl;
        }
    }

    void writeToFile(const boost::filesystem::path &file) const {
        boost::filesystem::ofstream myfile;
        myfile.open(file);
        for (const auto &l: proof) {
            myfile << l.second << std::endl;
        }
        myfile.close();
    }

private:

    std::vector<std::pair<Style, std::string>> proof;
    bool enabled = true;
};

#endif // PROOFOUTPUT_H
