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

#include "config.h"
#include "util/timeout.h" // for timestamps


// Intermediate buffer between the proofout stream and cout (or any other ostream).
// Inserts indention (and timestamps) after each newline.
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

    bool setEnabled(bool on) {
        std::swap(enabled, on);
        return on;
    }

    // print the given string before the next \n
    void printBeforeNewline(const std::string &s) {
        beforeNewline = s;
    }

protected:
    // override printing to insert indention
    int overflow(int ch) override {
        if (!enabled) {
            return ch; // return "success"
        }

        // print timestamp (if enabled)
        if (atStartOfLine && Config::Output::Timestamps) {
            std::string timeStr = formatTimestamp();
            dst->sputn(timeStr.data(), timeStr.size());
        }

        // print indention
        if (atStartOfLine && ch != '\n') {
            dst->sputn(indention.data(), indention.size());
        }

        // print given text at end of line (used to reset color codes)
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
    bool enabled = true;
    bool atStartOfLine = true;
    std::string indention;
    std::string beforeNewline;
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

    explicit ProofOutput(std::ostream &s) : std::ostream(s.rdbuf()), indenter(*this) {}

    void increaseIndention() {
        indenter.increaseIndention();
    }

    void decreaseIndention() {
        indenter.decreaseIndention();
    }

    bool setEnabled(bool on) {
        return indenter.setEnabled(on);
    }

    // sets the current style, which is reset at the end of line
    // (just writes the corresponding ANSI code to the stream)
    void setLineStyle(Style style) {
        if (Config::Output::Colors) {
            switch (style) {
                case Section: *this << Config::Color::Section; break; // bold yellow
                case Headline: *this << Config::Color::Headline; break; // bold blue
                case Result: *this << Config::Color::Result; break; // bold green
                case Warning: *this << Config::Color::Warning; break; // bold red
            }
            indenter.printBeforeNewline(Config::Color::None);
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

    // print given string in warning style with spacing
    void warning(const std::string &s) {
        *this << std::endl;
        setLineStyle(Style::Warning);
        *this << s << std::endl << std::endl;
    }

private:
    StreambufIndenter indenter;
};


#endif // PROOFOUTPUT_H
