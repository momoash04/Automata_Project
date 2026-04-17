#include "DFABackend.h"
using namespace std;

string getStateName(int state) {
    switch(state) {
        case 0: return "Start\n0 Ones";
        case 1: return "\n0 Ones\nEnds in 0\n(Reject)";
        case 2: return "\n1 Ones \nEnds in 1\n(Reject)";
        case 3: return "\n1 Ones \nEnds in 0\n(Reject)";
        case 4: return "\n2 Ones \nEnds in 1\n(Reject)";
        case 5: return "\n2 Ones \nEnds in 0\n(Reject)";
        case 6: return "\n0 Ones \nEnds in 1\n(Reject)";
        case 7: return "\n0 Ones \nEnds in 0\n(ACCEPT)";
    }
    return "Unknown";
}

DFAResult checkDFA(const string& input) {
    DFAResult result;
    result.accepted = false;
    result.finalState = 0;
    result.path = getStateName(0);
    
    if (input.empty()) return result;
    
    int state = 0;
    
    for (char c : input) {
        if (c == '0') {
            switch(state) {
                case 0: state = 1; break;
                case 1: state = 1; break;
                case 2: state = 3; break;
                case 3: state = 3; break;
                case 4: state = 5; break;
                case 5: state = 5; break;
                case 6: state = 7; break;
                case 7: state = 7; break;
            }
        } else if (c == '1') {
            switch(state) {
                case 0: state = 2; break;
                case 1: state = 2; break;
                case 2: state = 4; break;
                case 3: state = 4; break;
                case 4: state = 6; break;
                case 5: state = 6; break;
                case 6: state = 2; break;
                case 7: state = 2; break;
            }
        } else {
            result.path += " -> Error";
            return result; 
        }
        result.path += " -> " + getStateName(state);
    }
    
    result.finalState = state;
    result.accepted = (state == 7);
    result.currentStateName = getStateName(state);
    return result;
}
