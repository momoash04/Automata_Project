#ifndef DFA_BACKEND_H
#define DFA_BACKEND_H

#include <string>

struct DFAResult {
    bool accepted;
    int finalState;
    std::string path;
    std::string currentStateName; // Added for the circle drawn feature
};

// DFA Logic to check if the number of 1s is divisible by 3 (at least 1) and the string ends with 0.
DFAResult checkDFA(const std::string& input);

#endif // DFA_BACKEND_H
