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

#include "util/timeout.h"
#include <sstream>
#include <iomanip>


class StreambufIndenter : public std::streambuf {
public:
    explicit StreambufIndenter(std::ostream &os)
            : dst(os.rdbuf()),
              src(os)
    {
        // redirect output from os to this class
        // (and this class then writes to dst, which is os.rdbuf())
        src.rdbuf(this);
    }

    ~StreambufIndenter() override {
        // remove this class from the stream chain, i.e. let os write to dst again
        src.rdbuf(dst);
    }

    void increaseIndention() {
        indention.append("   ");
    }

    void decreaseIndention() {
        if (indention.size() >= 3) {
            indention.resize(indention.size()-3);
        } else {
            debugWarn("ProofOutput: decreaseIndention() called with no indention");
        }
    }

    void enableTimestamps(bool enable) {
        printTimestamps = enable;
    }

    // print the given string before the next \n
    void printBeforeNewline(std::string s) {
        beforeNewline = s;
    }

protected:
    // override printing to insert indention
    int overflow(int ch) override {
        if (atStartOfLine && ch != '\n') {
            // possibly print timestamp
            if (printTimestamps) {
                std::string timeStr = formatTimestamp();
                dst->sputn(timeStr.data(), timeStr.size());
            }

            // print indention ("put characters")
            dst->sputn(indention.data(), indention.size());
        }

        if (!beforeNewline.empty() && ch == '\n') {
            dst->sputn(beforeNewline.data(), beforeNewline.size());
            beforeNewline.clear();
        }

        atStartOfLine = (ch == '\n');
        return dst->sputc(ch);
    }

    // helper to display timestamps
    static std::string formatTimestamp() {
        using namespace std;
        timeoutpoint now = chrono::steady_clock::now();
        chrono::duration<float> elapsed = now - Timeout::start();

        stringstream ss;
        ss << "[" << std::fixed << std::setprecision(3) << std::setw(7) << elapsed.count() << "] ";

        return ss.str();
    }

private:
    // The StreambufIndenter forwards output from src to dst, but adds indention
    std::streambuf *dst;
    std::ostream &src;

    // Internal state
    bool atStartOfLine = true;
    std::string indention;
    std::string beforeNewline;

    // Options
    bool printTimestamps = false;
};


// Stores a StreambufIndenter instance to control indention of the stream.
// Also allows colored output with ANSI escape codes.
class ProofOutput : public std::ostream {
public:
    enum Style {
        Section,
        Headline,
        Warning,
        Result
    };

    ProofOutput(std::ostream &s, bool allowAnsiCodes, bool printTimestamps = true)
            : std::ostream(s.rdbuf()),
              allowAnsi(allowAnsiCodes),
              indenter(*this)
    {
        indenter.enableTimestamps(printTimestamps);
    }

    void increaseIndention() {
        indenter.increaseIndention();
    }

    void decreaseIndention() {
        indenter.decreaseIndention();
    }

    // sets the current style, which is reset at the end of line
    // (just writes the corresponding ANSI code to the stream)
    void setLineStyle(Style style) {
        if (allowAnsi) {
            switch (style) {
                case Section: *this << "\033[1;33m"; break; // bold yellow
                case Headline: *this << "\033[1;34m"; break; // bold blue
                case Result: *this << "\033[1;32m"; break; // bold green
                case Warning: *this << "\033[1;31m"; break; // bold red
            }
            indenter.printBeforeNewline("\033[0m");
        }
    }

    // print given string in headline style with spacing
    void headline(const std::string &s) {
        *this << std::endl;
        setLineStyle(Style::Headline);
        *this << s << std::endl;
    }

    // print given string in section style with spacing
    void section(const std::string &s) {
        *this << std::endl;
        setLineStyle(Style::Section);
        *this << "### " << s << " ###" << std::endl;
    }

private:
    bool allowAnsi;
    StreambufIndenter indenter;
};





#endif // PROOFOUTPUT_H
