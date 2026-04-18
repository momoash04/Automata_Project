#include <string>
#include <iostream>
#include <vector>
#include <stack>
#include <map>
#include <sstream>
#include <algorithm>
using namespace std;


struct Transition {
    string fromState;
    string readInput;
    string popStack;
    string pushStack;
    string toState;

    void print() const {
        // Output format: δ(fromState, readInput, popStack) = (toState, pushStack)
        string readStr = readInput.empty() ? "ε" : readInput;
        string popStr = popStack.empty() ? "ε" : popStack;
        string pushStr = pushStack.empty() ? "ε" : pushStack;
        
        cout << "Transition: (" << fromState << ", " << readStr << ", " 
             << popStr << ") -> (" << toState << ", " << pushStr << ")\n";
    }
};


int main ()
{
    vector<string> input = take_input();
    CFGParser parser;
    parser.parse(input);

    vector <string> variables;
    vector <string> terminals;
    map<string, vector<string>> rules;
    vector<Transition> transitions;

    variables = parser.getNonTerminals();
    terminals = parser.getTerminals();
    string startSymbol = parser.getStartSymbol();
    rules = parser.getRules();
    
    transitions.push_back({"q0", "", "", "$", "q1"});
    transitions.push_back({"q1", "", "", startSymbol, "q_loop"});
    int stateCounter = 2;

    for (auto rule : rules)
    {
        string endState = "q" + to_string(stateCounter);
        transitions.push_back({"q_loop", "", rule.first, "", endState });
        stateCounter++;
        push_in_reverse(rule.second, stateCounter, transitions);
    }

    for (auto terminal : terminals) {
        transitions.push_back({"q_loop", terminal, terminal, "", "q_loop" });
    }

    transitions.push_back({"q_loop", "", "$", "", "q_final" });

    return 0;
}

int push_in_reverse(vector<string> productions, int& stateCounter, vector<Transition>& transitions) {
    for (const auto& production : productions) {
        string currentState = "q" + to_string(stateCounter);
        string endState;
        for (int i = production.length() - 1; i >= 0; i--) {
            if (i == 0)
                endState = "q_loop";
            else
                endState = "q" + to_string(stateCounter);

            transitions.push_back({currentState, "", "", string(1, production[i]), endState});
            currentState = "q" + to_string(stateCounter);
            stateCounter++;
        }
    }
}


vector<string> take_input()
{
    vector<string> input;
    string line;
    cout << "Enter grammar rules (end with an empty line):\n";
    while (true) {
        getline(cin, line);
        if (line.empty()) break;
        input.push_back(line);
    }
    return input;
}

class CFGParser {
private:
    vector<string> grammar;           // Raw grammar rules (user input)
    vector<string> nonTerminals;      // Array of non-terminals
    vector<string> terminals;         // Array of terminals
    map<string, vector<string>> rules; // Rules map: nonTerminal -> [productions]

    static string trim(const string& s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        if (start == string::npos) return "";
        size_t end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }

    static vector<string> splitSymbolsBySpace(const string& production) {
        vector<string> tokens;
        string token;
        stringstream ss(production);
        while (ss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }


public:
    // Parse user input grammar rules
    string parse(const vector<string>& userInput) {
        grammar = userInput;
        nonTerminals.clear();
        terminals.clear();
        rules.clear();

        // First pass: Extract all non-terminals (left side of =)
        for (const auto& rule : grammar) {
            size_t eqPos = rule.find('=');
            if (eqPos == string::npos) {
                return "Error: Invalid rule format: " + rule;
            }

            string nonTerm = trim(rule.substr(0, eqPos));
            if (nonTerm.empty()) {
                return "Error: Empty non-terminal in rule: " + rule;
            }
            
            // Add to non-terminals array if not already present
            if (find(nonTerminals.begin(), nonTerminals.end(), nonTerm) == nonTerminals.end()) {
                nonTerminals.push_back(nonTerm);
            }
        }

        // Second pass: Extract all terminals and build rules
        for (const auto& rule : grammar) {
            size_t eqPos = rule.find('=');
            string nonTerm = trim(rule.substr(0, eqPos));
            string rightSide = rule.substr(eqPos + 1);

            // Split productions by '|'
            stringstream ss(rightSide);
            string production;
            while (getline(ss, production, '|')) {
                string cleanedProduction = trim(production);
                if (cleanedProduction.empty()) {
                    return "Error: Empty production in rule: " + rule;
                }

                rules[nonTerm].push_back(cleanedProduction);

                // Extract terminals from whitespace-separated symbols.
                vector<string> symbols = splitSymbolsBySpace(cleanedProduction);
                for (const auto& symbol : symbols) {
                    if (symbol == "ε") continue;
                    if (find(nonTerminals.begin(), nonTerminals.end(), symbol) == nonTerminals.end()) {
                        if (find(terminals.begin(), terminals.end(), symbol) == terminals.end()) {
                            terminals.push_back(symbol);
                        }
                    }
                }
            }
        }

        return "OK";
    }

    // Get non-terminals array
    vector<string> getNonTerminals() const {
        return nonTerminals;
    }

    // Get terminals array
    vector<string> getTerminals() const {
        return terminals;
    }

    // Get start symbol
    string getStartSymbol() const {
        if (!nonTerminals.empty()) {
            return nonTerminals[0];
        }
        return "";
    }

    map<string, vector<string>> getRules() const {
        return rules;
    }
};