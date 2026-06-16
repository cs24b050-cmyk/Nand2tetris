#include<iostream>
#include<string>
#include<unordered_map>
#include<fstream>
using namespace std;
// For now we are not dealing with Functional commands and branching commands

string asmforeq_gt_lt(const string &s,int counter) {
    string result;
    string truelabel = s + "_TRUE" + to_string(counter);
    string endlabel = s + "_END" + to_string(counter);
    string jumpCom;

    if(s == "eq") {jumpCom = "JEQ";}
    else if(s == "gt") {jumpCom = "JGT";}
    else {jumpCom = "JLT";}
    result = "@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nD=M-D\n@" + truelabel + "\nD;" + jumpCom + "\nD=0\n@" + endlabel + "\n0;JMP\n(" + truelabel +
    ")\nD=-1\n(" + endlabel + ")\n@SP\nA=M\nM=D\n@SP\nM=M+1\n";

    return result;
}

string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start==string::npos)?"":s.substr(start,end-start+1);
}

int main(int argc,char* argv[]) {
    if(argc < 2) {
        cout << "We need atleast 1 argument\n";
        return 1;
    }

    ifstream fin(argv[1]);
    if(!fin.is_open()) {
        cout << "Error in opening the file\n";
        return 1;
    }

    string inputfile = argv[1];

    string outputfile;
    int pos = inputfile.find('.');
    outputfile = inputfile.substr(0,pos) + ".asm";
    ofstream fout(outputfile);
    if(!fout.is_open()) {
        cout << "Error in opening output file\n";
        return 1;
    }

    string temp;

    unordered_map<string,string> Arithemtic_map = {{"add","@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M+D\n@SP\nM=M+1\n"},
    {"sub","@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M-D\n@SP\nM=M+1\n"},{"and","@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M&D\n@SP\nM=M+1\n"},
    {"or","@SP\nAM=M-1\nD=M\n@SP\nAM=M-1\nM=M|D\n@SP\nM=M+1\n"},{"neg","@SP\nAM=M-1\nM=-M\n@SP\nM=M+1\n"},{"not","@SP\nAM=M-1\nM=!M\n@SP\nM=M+1\n"}};

    unordered_map<string,string> segmentMap = {{"this","THIS"},{"that","THAT"},{"local","LCL"},{"argument","ARG"}};

    int Acounter = 0; // To count no.of eq,gt,lt commands

    while(getline(fin,temp)) {
        int commentPos = temp.find("//");
        if(commentPos!=string::npos) temp=temp.substr(0,commentPos);
        temp=trim(temp);
        if(temp.empty()) continue;
        int pos_s = temp.find(' ');
        string result;
        if(pos_s < 0) {
            // Arthimetic command
            if(temp != "eq" && temp != "gt" && temp!= "lt") {
                result = Arithemtic_map[temp];
            }
            else {
                result = asmforeq_gt_lt(temp,Acounter);
                Acounter++;
            }
        }
        else {
            // It could be a Push / pop commands or Branching commands or Function commands
            string command = temp.substr(0,pos_s);
            string rest = temp.substr(pos_s+1);
            int pos_s2 = rest.find(' ');
            string segment = rest.substr(0,pos_s2);
            string index = rest.substr(pos_s2+1);

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
        }
        fout << result;
    }

    fout << "(END)\n@END\n0;JMP\n";
    fin.close();
    fout.close();
}
