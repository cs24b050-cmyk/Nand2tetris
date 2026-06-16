#include <iostream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

// Trim removes the whitespaces that are trailing
string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

// checks if the given command is whether a branching command or not
bool isBranching(const string &s) {
    return s == "label" || s == "goto" || s == "if-goto";
}

// checks if it is a functional command or not
bool isFunctional(const string &s) {
    return s == "function" || s == "call" || s == "return";
}

// returns the asm code for the arithmetic operations eq,gt,lt
string asmforeq_gt_lt(const string &s, int counter, const string &currFunction="") {
    string prefix = currFunction.empty() ? "" : currFunction + "$";
    string truelabel = prefix + s + "_TRUE" + to_string(counter);
    string endlabel = prefix + s + "_END" + to_string(counter);
    string jumpCom = (s == "eq") ? "JEQ" : (s == "gt") ? "JGT" : "JLT";

    return "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nD=M-D\n@" + truelabel + "\nD;" + jumpCom +
           "\nD=0\n@" + endlabel + "\n0;JMP\n(" + truelabel + ")\nD=-1\n(" + endlabel +
           ")\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
}

// returns the asm code for the command call
string writeCall(const string &functionName, int nArgs, int &Fcounter) {
    string returnLabel = "RETURN_" + to_string(Fcounter++);
    string asmCode = "";

    asmCode += "@" + returnLabel + "\nD=A\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"; // push return-address
    asmCode += "@LCL\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"; // push LCL
    asmCode += "@ARG\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"; // push ARG
    asmCode += "@THIS\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"; // push THIS
    asmCode += "@THAT\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n"; // push THAT

    asmCode += "@SP\nD=M\n@5\nD=D-A\n@" + to_string(nArgs) + "\nD=D-A\n@ARG\nM=D\n";
    asmCode += "@SP\nD=M\n@LCL\nM=D\n";
    asmCode += "@" + functionName + "\n0;JMP\n";
    asmCode += "(" + returnLabel + ")\n";

    return asmCode;
}

// returns the asm code for the command return 
string writeReturn() {
    string asmCode = "";
    asmCode += "@LCL\nD=M\n@R13\nM=D\n";        // FRAME = LCL
    asmCode += "@5\nA=D-A\nD=M\n@R14\nM=D\n";   // RET = *(FRAME-5)
    asmCode += "@SP\nAM=M-1\nD=M\n@ARG\nA=M\nM=D\n"; // *ARG = pop()
    asmCode += "@ARG\nD=M+1\n@SP\nM=D\n";       // SP = ARG+1
    asmCode += "@R13\nAM=M-1\nD=M\n@THAT\nM=D\n"; // THAT = *(FRAME-1)
    asmCode += "@R13\nAM=M-1\nD=M\n@THIS\nM=D\n"; // THIS = *(FRAME-2)
    asmCode += "@R13\nAM=M-1\nD=M\n@ARG\nM=D\n";  // ARG = *(FRAME-3)
    asmCode += "@R13\nAM=M-1\nD=M\n@LCL\nM=D\n";  // LCL = *(FRAME-4)
    asmCode += "@R14\nA=M\n0;JMP\n";              // goto RET
    return asmCode;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "We need atleast one argument\n";
        return 1;
    }

    // we are taking directory as an input because we need to collect all the vm files
    string inputPath = argv[1];
    vector<string> vmFiles;

    // we are checking if the inputpath is a directory or not in the filesystem it is just a safety check
    if (fs::is_directory(inputPath)) {
        for (auto &entry : fs::directory_iterator(inputPath)) {
            if (entry.path().extension() == ".vm") {
                vmFiles.push_back(entry.path().string());
            }
        }
    } else {
        vmFiles.push_back(inputPath);
    }

    // To check if we require a bootstrap code or not to make this to work for project 7 also
    bool needsBootstrap = false;
    for (const auto &file : vmFiles) {
        ifstream fin(file);
        string line;
        while (getline(fin, line)) {
            if (line.find("function Sys.init") != string::npos) {
                needsBootstrap = true;
                break;
            }
        }
        fin.close();
        if (needsBootstrap) break;
    }

    // creating the output in the same directory with the directory name
    string outputFile;
    if (fs::is_directory(inputPath)) {
        string dirName = fs::path(inputPath).filename().string();
        outputFile = inputPath + "/" + dirName + ".asm";
    } else {
        outputFile = fs::path(inputPath).replace_extension(".asm").string();
    }

    // We want to translate the sys.vm first
    sort(vmFiles.begin(), vmFiles.end(), [](const string &a, const string &b) {
        if (a.find("Sys.vm") != string::npos) return true;
        if (b.find("Sys.vm") != string::npos) return false;
        return a < b;
    });

    ofstream fout(outputFile);
    if (!fout.is_open()) {
        cout << "Error opening output file.\n";
        return 1;
    }

    // Arithmetic map this has the asm code for the arithmetic commands
    unordered_map<string,string> Arithemtic_map = {{"add", "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M+D\n@SP\nM=M+1\n"},
    {"sub", "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M-D\n@SP\nM=M+1\n"},{"and", "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M&D\n@SP\nM=M+1\n"},
    {"or", "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M|D\n@SP\nM=M+1\n"},{"neg", "@SP\nAM=M-1\nM=-M\n@SP\nM=M+1\n"},
    {"not", "@SP\nAM=M-1\nM=!M\n@SP\nM=M+1\n"}};

    unordered_map<string,string> segmentMap = {{"local", "LCL"}, {"argument", "ARG"}, {"this", "THIS"}, {"that", "THAT"}};

    int Acounter = 0; // counter for eq/gt/lt
    int Fcounter = 0; // counter for function,call,return
    int SIZE = 0; // for counting no.of vm files in the folder
    string currFunction = "";

    // Bootstrap code
    if (needsBootstrap) {
        fout << "@256\nD=A\n@SP\nM=D\n";
        fout << writeCall("Sys.init", 0, Fcounter);
    }

    // Process each VM file
    while (SIZE < vmFiles.size()) {
        string inputfile = vmFiles[SIZE];
        ifstream fin(inputfile);
        string line;
        string fileName = fs::path(inputfile).stem().string();

        while (getline(fin, line)) {
            size_t commentPos = line.find("//");
            if (commentPos != string::npos) line = line.substr(0, commentPos);
            // If we find a comment in a line we take upto that comment and then trim
            line = trim(line);
            if (line.empty()) continue; // after taking to before comment if it is empty then continue

            size_t pos_s = line.find(' ');
            string command = (pos_s == string::npos) ? line : line.substr(0, pos_s);
            string arg1 = "", arg2 = "";
            if (pos_s != string::npos) {
                size_t pos_s2 = line.find(' ', pos_s + 1);
                if (pos_s2 != string::npos) {
                    arg1 = line.substr(pos_s + 1, pos_s2 - pos_s - 1);
                    arg2 = line.substr(pos_s2 + 1);
                } else {
                    arg1 = line.substr(pos_s + 1);
                }
            }

            string asmCode = "";

            if (command == "add" || command == "sub" || command == "neg" || command == "and" ||
                command == "or" || command == "not" || command == "eq" || command == "gt" || command == "lt") {
                if (command == "eq" || command == "gt" || command == "lt") {
                    asmCode = asmforeq_gt_lt(command, Acounter++, currFunction);
                } else {
                    asmCode = Arithemtic_map[command];
                }
            }
            // If it is a branching command
            else if (isBranching(command)) {
                string scoped = arg1;
                if (!currFunction.empty()) scoped = currFunction + "$" + arg1;

                if (command == "label") asmCode = "(" + scoped + ")\n";
                else if (command == "goto") asmCode = "@" + scoped + "\n0;JMP\n";
                else asmCode = "@SP\nAM=M-1\nD=M\n@" + scoped + "\nD;JNE\n";
            }
            // If it is a functional command
            else if (isFunctional(command)) {
                if (command == "function") {
                    currFunction = arg1;
                    int nLocals = stoi(arg2);
                    asmCode = "(" + currFunction + ")\n";
                    for (int i = 0; i < nLocals; i++) asmCode += "@SP\nA=M\nM=0\n@SP\nM=M+1\n";
                } else if (command == "call") {
                    int nArgs = stoi(arg2);
                    asmCode = writeCall(arg1, nArgs, Fcounter);
                } else { // return
                    asmCode = writeReturn();
                }
            }
            //if we are here then it is a push or pop
            else if (command == "push" || command == "pop") {
                if (arg1 == "constant") {
                    if (command == "push") asmCode = "@" + arg2 + "\nD=A\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                } else if (arg1 == "local" || arg1 == "argument" || arg1 == "this" || arg1 == "that") {
                    string base = segmentMap[arg1];
                    if (command == "push") asmCode = "@" + base + "\nD=M\n@" + arg2 + "\nA=D+A\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                    else asmCode = "@" + base + "\nD=M\n@" + arg2 + "\nD=D+A\n@R13\nM=D\n@SP\nAM=M-1\nD=M\n@R13\nA=M\nM=D\n";
                } else if (arg1 == "temp") {
                    int addr = 5 + stoi(arg2);
                    if (command == "push") asmCode = "@" + to_string(addr) + "\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                    else asmCode = "@" + to_string(addr) + "\nD=A\n@R13\nM=D\n@SP\nAM=M-1\nD=M\n@R13\nA=M\nM=D\n";
                } else if (arg1 == "pointer") {
                    string reg = (arg2 == "0") ? "THIS" : "THAT";
                    if (command == "push") asmCode = "@" + reg + "\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                    else asmCode = "@SP\nAM=M-1\nD=M\n@" + reg + "\nM=D\n";
                } else if (arg1 == "static") {
                    string var = fileName + "." + arg2;
                    if (command == "push") asmCode = "@" + var + "\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                    else asmCode = "@SP\nAM=M-1\nD=M\n@" + var + "\nM=D\n";
                }
            }

            fout << asmCode;
        }

        fin.close();
        SIZE++;
    }

    if(!needsBootstrap) {
        fout << "(END)\n@END\n0;JMP\n";
    }

    fout.close();
    return 0;
}
