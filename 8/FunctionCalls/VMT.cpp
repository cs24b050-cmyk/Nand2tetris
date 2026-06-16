#include<iostream>
#include<string>
#include<unordered_map>
#include<fstream>
#include<filesystem>
#include<vector>
#include<algorithm>
using namespace std;
namespace fs = std::filesystem;

string asmforeq_gt_lt(const string &s, int counter, const string &currFunction = "") {
    string prefix = currFunction.empty() ? "" : currFunction + "$";
    string truelabel = prefix + s + "_TRUE" + to_string(counter);
    string endlabel = prefix + s + "_END" + to_string(counter);
    string jumpCom;

    if (s == "eq") jumpCom = "JEQ";
    else if (s == "gt") jumpCom = "JGT";
    else jumpCom = "JLT";

    return "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nD=M-D\n@" + truelabel + "\nD;" + jumpCom +
           "\nD=0\n@" + endlabel + "\n0;JMP\n(" + truelabel + ")\nD=-1\n(" + endlabel +
           ")\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
}


string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start==string::npos)?"":s.substr(start,end-start+1);
}

bool isBranching(const string &s) {
    return s == "label" || s == "goto" || s == "if-goto";
}

bool isFunctional(const string &s) {
    return s == "function" || s == "call" || s == "return";
}

string writeCall(const string &functionName, int nArgs, int &Fcounter) {
    string returnLabel = "RETURN_" + to_string(Fcounter++);
    string asmCode = "";

    // push return-address
    asmCode += "@" + returnLabel + "\nD=A\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";

    // push LCL, ARG, THIS, THAT
    asmCode += "@LCL\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    asmCode += "@ARG\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    asmCode += "@THIS\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    asmCode += "@THAT\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";

    // ARG = SP - nArgs - 5
    int total = nArgs + 5;
    asmCode += "@SP\nD=M\n@" + to_string(total) + "\nD=D-A\n@ARG\nM=D\n";

    // LCL = SP
    asmCode += "@SP\nD=M\n@LCL\nM=D\n";

    // goto functionName
    asmCode += "@" + functionName + "\n0;JMP\n";

    // (return-address label)
    asmCode += "(" + returnLabel + ")\n";

    return asmCode;
}

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


int main(int argc,char* argv[]) {
    if(argc < 2) {
        cout << "We need atleast 1 argument\n";
        return 1;
    }

    string inputPath = argv[1];
    vector<string> vmFiles;

    // Collect all .vm files
    if(fs::is_directory(inputPath)) {
        for(auto &entry : fs::directory_iterator(inputPath)) {
            if(entry.path().extension() == ".vm") {
                vmFiles.push_back(entry.path().string());
            }
        }
    } else {
        vmFiles.push_back(inputPath);
    }

    bool needsBootstrap = false;
    for(const auto& file : vmFiles) {
        ifstream fin(file);
        string line;
        while(getline(fin, line)) {
            if(line.find("function Sys.init") != string::npos) {
                needsBootstrap = true;
                break;
            }
        }
        fin.close();
        if(needsBootstrap) break;
    }

    // Decide output file
    string outputfile;
    if (fs::is_directory(inputPath)) {
        string dirName = fs::path(inputPath).filename().string();
        outputfile = inputPath + "/" + dirName + ".asm";
    } else {
        outputfile = fs::path(inputPath).replace_extension(".asm").string();
    }

    sort(vmFiles.begin(), vmFiles.end(), [](const string &a, const string &b) {
        if (a.find("Sys.vm") != string::npos) return true;
        if (b.find("Sys.vm") != string::npos) return false;
        return a < b;
    });

    ofstream fout(outputfile);
    if(!fout.is_open()) {
        cout << "Error in opening output file\n";
        return 1;
    }

    unordered_map<string,string> Arithemtic_map = {{"add","@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M+D\n@SP\nM=M+1\n"},
    {"sub","@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M-D\n@SP\nM=M+1\n"},{"and","@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M&D\n@SP\nM=M+1\n"},
    {"or","@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M|D\n@SP\nM=M+1\n"},{"neg","@SP\nAM=M-1\nM=-M\n@SP\nM=M+1\n"},{"not","@SP\nAM=M-1\nM=!M\n@SP\nM=M+1\n"}};

    unordered_map<string,string> segmentMap = {{"this","THIS"},{"that","THAT"},{"local","LCL"},{"argument","ARG"}};

    int Acounter = 0; // To count no.of eq,gt,lt commands
    int Fcounter = 0; // To make unique labels for function calls
    int SIZE = 0;
    string currFunction = "";          // current function name (for label scoping)


    // Always bootstrap if Sys.init exists
    if (needsBootstrap) {
        fout << "@256\nD=A\n@SP\nM=D\n";
        fout << writeCall("Sys.init", 0, Fcounter);
    }

    while(SIZE != vmFiles.size()) {

        string inputfile = vmFiles[SIZE];
        string temp;

        ifstream fin(inputfile);
        while(getline(fin,temp)) {
            size_t commentPos = temp.find("//");
            if(commentPos!=string::npos) temp=temp.substr(0,commentPos);
            temp=trim(temp);
            if(temp.empty()) continue;
            size_t pos_s = temp.find(' ');
            string result = "";

            string command, rest, segment, index;

            if(pos_s == string::npos) {
                // No space → arithmetic command
                command = temp;
            } else {
                // Has space → could be memory, branching, or function commands
                command = temp.substr(0, pos_s);
                rest = temp.substr(pos_s + 1);

                size_t pos_s2 = rest.find(' ');
                if(pos_s2 != string::npos) {
                    segment = rest.substr(0, pos_s2);
                    index = rest.substr(pos_s2 + 1);
                } else {
                    segment = rest;  // for things like "function MyFunc"
                    index = "";
                }
            }

            if(pos_s == string::npos) {
                // Arthimetic command
                if(temp != "eq" && temp != "gt" && temp!= "lt") {
                    result = Arithemtic_map[temp];
                }
                else {
                    result = asmforeq_gt_lt(temp, Acounter, currFunction);
                    Acounter++;
                }
            }
            else if(isBranching(command)) {
                string scoped = rest;
                if (!currFunction.empty()) scoped = currFunction + "$" + rest;
                if (command == "label") {
                    fout << "(" << scoped << ")" << endl;
                } else if (command == "goto") {
                    fout << "@" << scoped << "\n0;JMP\n";
                } else { // if-goto
                    fout << "@SP\nAM=M-1\nD=M\n@" << scoped << "\nD;JNE\n";
                }
            }
            else if(isFunctional(command)) {
                if (command == "function") {
                    currFunction = segment; // set current function
                    fout << "(" << currFunction << ")\n";
                    int loop = stoi(index);
                    while (loop--) {
                        fout << "@SP\nA=M\nM=0\n@SP\nM=M+1\n";
                    }
                } else if (command == "call") {
                    int nArgs = stoi(index);
                    fout << writeCall(segment, nArgs, Fcounter);
                } else { // return
                    fout << writeReturn();
                }
            }
            else {
                // It could be a Push / pop commands
                if(segment == "pointer") {
                    string reg = (index == "0") ? "THIS" : "THAT";
                    if(command == "push") {
                        result = "@" + reg + "\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                    }
                    else {
                        result = "@SP\nAM=M-1\nD=M\n@" + reg + "\nM=D\n";
                    }
                }
                else if(segment == "temp") {
                    int addr = 5 + stoi(index);
                    if(command == "push")
                        result = "@" + to_string(addr) + "\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                    else
                        result = "@" + index + "\nD=A\n@5\nD=A+D\n@R13\nM=D\n@SP\nAM=M-1\nD=M\n@R13\nA=M\nM=D\n";
                }
                else {
                    string fileName = inputfile.substr(inputfile.find_last_of("/\\")+1);
                    fileName = fileName.substr(0, fileName.find('.'));
                    string var = fileName + "." + index;
                    if(segment == "static") {
                        if(command == "push")
                            result = "@" + var + "\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                        else
                            result = "@SP\nAM=M-1\nD=M\n@" + var + "\nM=D\n";
                    }
                }

                if(command == "push") {

                    if(segment == "constant") {
                        result = "@" + index + "\nD=A\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                    }
                    else if(segment == "local" || segment == "argument" || segment == "this" || segment == "that") {
                        string base = segmentMap[segment];
                        result = "@" + base + "\nD=M\n@" + index + "\nA=D+A\nD=M\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";
                    }
                }

                else if(command == "pop") {
                    if(segment == "local" || segment == "argument" || segment == "this" || segment == "that") {
                        string base = segmentMap[segment];
                        result = "@" + base + "\nD=M\n@" + index + "\nD=D+A\n@R13\nM=D\n@SP\nAM=M-1\nD=M\n@R13\nA=M\nM=D\n";
                    }
                }
                fout << result;
            }
        }
        fin.close();
        SIZE++;
    }
    
    if(!needsBootstrap) {
        fout << "(END)\n@END\n0;JMP\n";
    }

    fout.close();
}
