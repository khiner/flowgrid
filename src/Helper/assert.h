#pragma once

#include <iostream>
#include <sstream>
#include <execinfo.h>

using std::string;
using std::stringstream;

class Exception : public std::runtime_error {
public:
    Exception(const std::string &msg = "") : std::runtime_error(msg) {}
    Exception(const char *msg) : std::runtime_error(msg) {}

    std::string Message() { return what(); }
    void Print() { std::cerr << what(); }
};

void assertAux(bool condition, const string &file, int line) {
    if (condition) return;

    stringstream str;
    str << "Assertion failed.";
    str << "File: " << file.substr(file.find_last_of('/') + 1) << ", Line: " << line << '\n';

    const int callstack_size = 20;
    void *callstack[callstack_size];
    const int frames = backtrace(callstack, callstack_size);
    char **symbols = backtrace_symbols(callstack, frames);
    str << "====== Stack trace start ======\n";
    for (int i = 0; i < frames; ++i) str << symbols[i] << "\n";
    str << "====== Stack trace stop ======\n";
    
    free(symbols);
    throw Exception(str.str());
}

#define fgassert(cond) assertAux((cond), __FILE__, __LINE__)
