#include<iostream>
#include<bitset>
#include<unordered_map>
#include<fstream> // I think this is like to involve file stream
// So is like what does input and output stream mean keyboard and maybe screen ok so in memory we have locations of keyboard and screen in the stack
// So like in complier we have an instruction same as in the assembler we have dest = comp, jump; 
#include<cctype> // for isspace,isalpha and so on things
using namespace std;

// Syntax for substr(start,no.of characters)
string convertIntoBinary(int integer) {
    string result;
    result = bitset<16>(integer).to_string();
    // bitset creates a space for 16 bits space container so its size is 16-bit and those bits together combined is converted to string
    return result;
}
string trim(const string &s) {
    string result = "";
    for(auto c : s) {
        if(c == '/') return result; // this maybe unsafe I am not sure but yeah if the input file has valid hack instructions then it is fine
        if(!isspace(c)) result += c; // transverse through line and if you find whitespace then remove.
    }
    return result;
}

// so let us do this way first like giving input to main
// this argc is number of files in the terminal like as follow
// ./assembler input1.txt input2.txt are considered as 3 arguments then argc = 3 and argv[] this array of these files in order
int main(int argc, char* argv[]) {
    if(argc < 2) {
        cout << "We need atleast one argument" << endl;
        return 1;
    } // this is for incorrect file handling like if we don't give them the input file

    ifstream fin(argv[1]);
    if (!fin.is_open()) {
        cout << "Error in file handling" << endl;
        return 1;
    } 
    
    string inputfile = argv[1];
    string outputfile;
    int pos = inputfile.find('.');
    outputfile = inputfile.substr(0,pos) + ".hack";
    ofstream fout(outputfile); // This creates a output file in the same directory with the name same as input but replacing asm with hack

    if(!fout.is_open()) {
        cout << " Output file cannot be opened" << endl;
        return 1;
    } // If there is some issue with opening of file

    // So anything we want to write can be done as like 
    // fout << "This would be the first line of the output file";

    string temp;

    int nextVarAddress = 16; // This would be incremented every time we define a variable

    int currentROMaddress = 0;

    unordered_map<string,int> SymbolTable = {{"SP",0},{"LCL",1},{"ARG",2},{"THIS",3},{"THAT",4},{"SCREEN",16384},{"KBD",24576},{"R0",0},
    {"R1",1},{"R2",2},{"R3",3},{"R4",4},{"R5",5},{"R6",6},{"R7",7},{"R8",8},{"R9",9},{"R10",10},{"R11",11},{"R12",12},
    {"R13",13},{"R14",14},{"R15",15}}; 
    //In SymbolTable we are going to append string names with just their indices like (SP,0);(LCL,1) and any variables defined

    unordered_map<string,string> COMP = {{"0","0101010"},{"1","0111111"},{"-1","0111010"},{"D","0001100"},{"A","0110000"},{"M","1110000"},
    {"!D","0001101"},{"!A","0110011"},{"!M","1110001"},{"-D","0001111"},{"-A","0110011"},{"-M","1110011"},{"D+1","0011111"},{"A+1","0110111"},
    {"M+1","1110111"},{"D-1","0001110"},{"A-1","0110010"},{"M-1","1110010"},{"D+A","0000010"},{"D+M","1000010"},{"D-A","0010011"},{"D-M","1010011"},
    {"A-D","0000111"},{"M-D","1000111"},{"D&A","0000000"},{"D&M","1000000"},{"D|A","0010101"},{"D|M","1010101"}};

    unordered_map<string,string> DEST = {{"","000"},{"M","001"},{"D","010"},{"MD","011"},{"A","100"},{"AM","101"},{"AD","110"},{"ADM","111"}};

    unordered_map<string,string> JUMP = {{"","000"},{"JGT","001"},{"JEQ","010"},{"JGE","011"},{"JLT","100"},{"JNE","101"},{"JLE","110"},{"JMP","111"}};

    while(getline(fin,temp)) {
        // if it is a A or C instruction we need to increment currentROMaddress if it is a label skip before that we need to trim
        // If it is a useless line don't increment currentROMaddress
        temp = trim(temp);
        if(temp == "") continue;
        else if(temp[0] == '(') {
            SymbolTable[temp.substr(1,temp.size()-2)] = currentROMaddress;
        }
        else currentROMaddress++;
    } // This is the first pass

    fin.clear();
    fin.seekg(0);

    while(getline(fin,temp)) {
        // This is the original way to remove comments

        /*size_t pos = temp.find("//");
        temp = temp.substr(0,pos);*/

        temp = trim(temp); // This gives the string after trimming (removing whitespaces and soon)
        // Now we have only a single valid instruction with no space and no comments no we need to check if it is either A or C instruction.

        if(temp == "") continue; // May be a comment or so when trimmed no useful instruction

        string result = "111"; // Every line is converted and this result is written into output file.
        // Let us start with 111 so if it is a A instruction it would be replaced if it is a C instruction then we concatinate.

        if(temp[0] == '@') { // this is an A instruction
            // Now they are 2 cases if it is a variable or a integer address location
            // Like if it is like @2
            // If it is like @X then we have a variable count for which location we need to give to variable
            if(isalpha(temp[1])) {
                // Now for sure we know it is a variable and we need to know that it is not of a known variable
                if(SymbolTable.find(temp.substr(1)) == SymbolTable.end()) {
                    SymbolTable[temp.substr(1)] = nextVarAddress;
                    nextVarAddress++;
                }
                result = convertIntoBinary(SymbolTable[temp.substr(1)]);
                // Now we need to map this variable also to this number
            }
            else {
                int value = stoi(temp.substr(1));
                result = convertIntoBinary(value);
            }
            // result[0] = '0'; // Since it is an A instruction we can ignore this line because for all constants the first bit is always 0
        }
        else if(temp[0] == '(') {
            // result = convertIntoBinary(SymbolTable[temp.substr(1,temp.size()-2)]);
            continue;
        } // This is a label.
        else {
            // Now we have an instruction like D=M
            // Now let us split it into 3 parts dest=comp;jump
            // First find "=" pos this is pos1 and find ";" pos this is pos2
            int pos1 = temp.find("=");
            int pos2 = temp.find(";");
            // If pos1 is +ve then = is present
            // If pos2 is +ve then ; is present
            
            string dest = "",comp = "",jump = "";

            if(pos1 < 0) {
                // Then there is no = in the line
                // then it is like 0;JMP
                comp = temp.substr(0,pos2);
                jump = temp.substr(pos2+1);
                dest = "";
            }
            else {
                if(pos2 < 0) {
                    // there is no jump instruction
                    // Like D=D+1
                    dest = temp.substr(0,pos1);
                    comp = temp.substr(pos1+1);
                    jump = "";
                }
                else{
                    dest = temp.substr(0,pos1);
                    comp = temp.substr(pos1+1,pos2-(pos1+1));
                    jump = temp.substr(pos2+1);
                }
            }

            result = result + COMP[comp] + DEST[dest] + JUMP[jump];

        }
        
        fout << result << endl;

    }

    fin.close();
    fout.close(); // A good practice to close the files at the end of program

}