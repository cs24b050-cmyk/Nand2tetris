#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <filesystem>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

// Structure to represent a token
struct Token {
    string type;
    string value;
};

// Jack keywords
vector<string> keywords = {
    "class", "constructor", "function", "method", "field", "static", "var",
    "int", "char", "boolean", "void", "true", "false", "null", "this",
    "let", "do", "if", "else", "while", "return"
};

// Check if the character is a symbol
bool isSymbol(char c) {
    const string symbols = "{}()[].,;+-*/&|<>=~";
    return symbols.find(c) != string::npos;
}

// Check if string is a keyword
bool isKeyword(const string &word) {
    return find(keywords.begin(), keywords.end(), word) != keywords.end();
}

// Determine token type
string getTokenType(const string &word) {
    if (isKeyword(word)) return "keyword";
    if (word.size() == 1 && isSymbol(word[0])) return "symbol";
    if (!word.empty() && isdigit(word[0])) return "integerConstant";
    if (!word.empty() && word[0] == '"') return "stringConstant";
    return "identifier";
}

// Tokenize a .jack file
vector<Token> tokenizeFile(const string &filepath) {
    ifstream file(filepath);
    vector<Token> tokens;

    if (!file.is_open()) return tokens;

    string data((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    string curr;
    bool inString = false;

    for (size_t i = 0; i < data.size(); i++) {
        char c = data[i];

        if (!inString && c == '/' && i + 1 < data.size() && data[i + 1] == '/') {
            while (i < data.size() && data[i] != '\n') i++;
            continue;
        }

        if (!inString && c == '/' && i + 1 < data.size() && data[i + 1] == '*') {
            i += 2;
            while (i + 1 < data.size() && !(data[i] == '*' && data[i + 1] == '/')) i++;
            i++;
            continue;
        }

        if (c == '"') {
            if (inString) {
                tokens.push_back({"stringConstant", curr});
                curr.clear();
                inString = false;
            } else {
                if (!curr.empty()) {
                    tokens.push_back({getTokenType(curr), curr});
                    curr.clear();
                }
                inString = true;
            }
            continue;
        }

        if (inString) {
            curr += c;
            continue;
        }

        if (isSymbol(c)) {
            if (!curr.empty()) {
                tokens.push_back({getTokenType(curr), curr});
                curr.clear();
            }
            tokens.push_back({"symbol", string(1, c)});
            continue;
        }

        if (isspace(c)) {
            if (!curr.empty()) {
                tokens.push_back({getTokenType(curr), curr});
                curr.clear();
            }
            continue;
        }

        curr += c;
    }

    if (!curr.empty())
        tokens.push_back({getTokenType(curr), curr});

    return tokens;
}

// Create a token XML file in same directory
void writeTokensToXML(const string &inputFile, const vector<Token> &tokens) {
    fs::path p(inputFile);
    string outputFile = (p.parent_path() / ("my" + p.stem().string() + "T.xml")).string();

    ofstream out(outputFile);

    out << "<tokens>\n";
    for (const auto &tk : tokens) {
        string val = tk.value;
        if (val == "<") val = "&lt;";
        else if (val == ">") val = "&gt;";
        else if (val == "&") val = "&amp;";

        out << "  <" << tk.type << "> " << val << " </" << tk.type << ">\n";
    }
    out << "</tokens>\n";
}

// Main program
int main(int argc, char* argv[]) {

    if (argc != 2) {
        return 1;
    }

    string dirPath = argv[1];
    vector<string> jackFiles;

    for (const auto &entry : fs::directory_iterator(dirPath)) {
        if (entry.path().extension() == ".jack") {
            jackFiles.push_back(entry.path().string());
        }
    }

    for (size_t i = 0; i < jackFiles.size(); i++) {
        vector<Token> tokens = tokenizeFile(jackFiles[i]);
        writeTokensToXML(jackFiles[i], tokens);
    }

    return 0;
}
