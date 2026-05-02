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
        // Output format: Î´(fromState, readInput, popStack) = (toState, pushStack)
        string readStr = readInput.empty() ? "e" : readInput;
        string popStr = popStack.empty() ? "e" : popStack;
        string pushStr = pushStack.empty() ? "e" : pushStack;

        cout << "Transition: (" << fromState << ", " << readStr << ", "
            << popStr << ") -> (" << toState << ", " << pushStr << ")\n";
    }
};

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

    static vector<string> splitSymbolsBySpace(const string& production) {
        vector<string> tokens;
        string token;
        stringstream ss(production);
        while (ss >> token) {
            tokens.push_back(token);
        }
        return tokens;
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
                    if (symbol == "e") continue;
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

void push_in_reverse(vector<string> productions, int& stateCounter, vector<Transition>& transitions) {
    string RuleCurrentState = "q" + to_string(stateCounter);
    stateCounter++;
    for (const auto& production : productions) {
        vector<string> symbols = splitSymbolsBySpace(production);
        string symbol = "";
        for (int i = symbols.size() - 1; i >= 0; i--) {
            symbol = symbol + symbols[i] + " " ;
        }
        transitions.push_back({ RuleCurrentState, "", "", symbol, "q_loop" });
    }
    return;
}


// Recursive function to simulate the Non-Deterministic PDA
bool simulatePDA(const vector<Transition>& transitions, string currentState, 
                 vector<string> remainingInput, vector<string> stack, int depth = 0) {
    
    // Safety check: Prevent infinite loops from left-recursive epsilon transitions
    if (depth > 5000) return false; 

    // Accept condition: Reached q_final and all input tokens have been consumed
    if (currentState == "q_final" && remainingInput.empty()) {
        return true;
    }

    // Explore every possible transition from the current state
    for (const auto& t : transitions) {
        if (t.fromState != currentState) continue;

        // 1. Check Input Condition
        bool inputMatches = false;
        vector<string> nextInput = remainingInput;
        if (t.readInput == "" || t.readInput == "e") { 
            inputMatches = true; // Epsilon transition (doesn't consume input)
        } else if (!remainingInput.empty() && remainingInput.front() == t.readInput) {
            inputMatches = true;
            nextInput.erase(nextInput.begin()); // Consume the matched input token
        }
        if (!inputMatches) continue;

        // 2. Check Stack Condition
        bool stackMatches = false;
        vector<string> nextStack = stack;
        if (t.popStack == "" || t.popStack == "e") {
            stackMatches = true; // Epsilon transition (doesn't pop)
        } else if (!nextStack.empty() && nextStack.back() == t.popStack) {
            stackMatches = true;
            nextStack.pop_back(); // Pop the matched symbol from the top of the stack
        }
        if (!stackMatches) continue;
 
        // 3. Apply Push Operation
        if (t.pushStack != "" && t.pushStack != "e") {
            vector<string> symbolsToPush = splitSymbolsBySpace(t.pushStack);
            
            // Push each token onto the stack.
            for (const string& sym : symbolsToPush) {
                nextStack.push_back(sym);
            }
        }

        // 4. Recurse to the next state with the updated input and stack
        if (simulatePDA(transitions, t.toState, nextInput, nextStack, depth + 1)) {
            return true; // Path found!
        }
    }
    
    // If no transitions from this state lead to an acceptance, backtrack
    return false; 
}

int main()
{
    vector<string> input = take_input();
    CFGParser parser;
    parser.parse(input);

    vector<string> variables;
    vector<string> terminals;
    map<string, vector<string>> rules;
    vector<Transition> transitions;

    variables = parser.getNonTerminals();
    terminals = parser.getTerminals();
    string startSymbol = parser.getStartSymbol();
    rules = parser.getRules();

    transitions.push_back({ "q0", "", "", "$", "q1" });
    transitions.push_back({ "q1", "", "", startSymbol, "q_loop" });
    int stateCounter = 2;

    for (auto rule : rules)
    {
        string endState = "q" + to_string(stateCounter);
        transitions.push_back({ "q_loop", "", rule.first, "", endState });
        push_in_reverse(rule.second, stateCounter, transitions);
    }

    for (auto terminal : terminals) {
        transitions.push_back({ "q_loop", terminal, terminal, "", "q_loop" });
    }

    transitions.push_back({ "q_loop", "", "$", "", "q_final" });

    for (const auto& transition : transitions) {
        transition.print();
    }

    // --- NEW SIMULATION BLOCK ---
    cout << "\n=== PDA String Simulation ===\n";
    cout << "Enter a string to test (space-separated tokens, e.g., 'a b b a'), or type 'exit' to quit:\n";
    
    string testString;
    while (getline(cin, testString)) {
        if (testString == "exit") break;
        
        // Convert the input string into a vector of tokens
        vector<string> inputTokens = splitSymbolsBySpace(testString);
        vector<string> initialStack; // PDA starts with an empty stack
        
        // Start simulation from initial state q0
        bool isAccepted = simulatePDA(transitions, "q0", inputTokens, initialStack, 0);
        
        if (isAccepted) {
            cout << "Result: ACCEPTED\n\n";
        } else {
            cout << "Result: REJECTED\n\n";
        }
        cout << "Enter another string to test (or 'exit'):\n";
    }

    return 0;
}