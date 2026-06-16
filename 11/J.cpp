// simple jack to vm converter 

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <sstream>
#include <cctype>
#include <set>

using namespace std;
namespace fs = std::filesystem;

// single token unit with its type and text
struct Token {
    string type;  // can be keyword, symbol, identifier, integerConstant, or stringConstant
    string text;
};

// list of all jack keywords
set<string> jackKeywords = {
    "class","constructor","function","method","field","static","var",
    "int","char","boolean","void","true","false","null","this",
    "let","do","if","else","while","return"
};

// to figure out the type of token (keyword, symbol, etc)
string classifyToken(const string& word) {
    if (jackKeywords.count(word)) return "keyword";
    if (word.size() == 1 && string("{}()[].,;+-*/&|<>=~").find(word) != string::npos) return "symbol";
    if (!word.empty() && isdigit((unsigned char)word[0])) return "integerConstant";
    if (!word.empty() && word[0] == '"') return "stringConstant";
    return "identifier";
}

// to break jack file into tokens
vector<Token> tokenizeJackFile(const string& path) {
    ifstream fin(path);
    vector<Token> tokens;
    if (!fin) return tokens; // file not found

    string content((istreambuf_iterator<char>(fin)), {});
    string buffer;
    bool inString = false;

    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];

        // handle single line comments
        if (!inString && c == '/' && i + 1 < content.size() && content[i + 1] == '/') {
            while (i < content.size() && content[i] != '\n') ++i;
            continue;
        }

        // handle block comments
        if (!inString && c == '/' && i + 1 < content.size() && content[i + 1] == '*') {
            i += 2;
            while (i + 1 < content.size() && !(content[i] == '*' && content[i + 1] == '/')) ++i;
            ++i;
            continue;
        }

        // handle strings
        if (c == '"') {
            if (inString) {
                tokens.push_back({"stringConstant", buffer});
                buffer.clear();
                inString = false;
            } else {
                if (!buffer.empty()) {
                    tokens.push_back({classifyToken(buffer), buffer});
                    buffer.clear();
                }
                inString = true;
            }
            continue;
        }

        if (inString) {
            buffer.push_back(c);
            continue;
        }

        // if symbol found, store buffer and symbol
        if (string("{}()[].,;+-*/&|<>=~").find(c) != string::npos) {
            if (!buffer.empty()) {
                tokens.push_back({classifyToken(buffer), buffer});
                buffer.clear();
            }
            tokens.push_back({"symbol", string(1, c)});
            continue;
        }

        // skip spaces and finalize any current buffer
        if (isspace((unsigned char)c)) {
            if (!buffer.empty()) {
                tokens.push_back({classifyToken(buffer), buffer});
                buffer.clear();
            }
            continue;
        }

        buffer += c;
    }

    if (!buffer.empty())
        tokens.push_back({classifyToken(buffer), buffer});

    return tokens;
}

// class to move through tokens easily
class TokenStream {
    vector<Token> all;
    size_t pos = 0;
public:
    TokenStream(const vector<Token>& t) : all(t) {}
    bool hasMore() const { return pos < all.size(); }
    string text() const { return hasMore() ? all[pos].text : ""; }
    string type() const { return hasMore() ? all[pos].type : ""; }
    string nextText() const { return (pos + 1 < all.size()) ? all[pos + 1].text : ""; }
    void advance() { if (hasMore()) ++pos; }

    // check expected token
    void expect(const string& s) {
        if (!hasMore() || text() != s) {
            cerr << "error: expected '" << s << "', found '" << text() << "'\n";
            exit(1);
        }
        advance();
    }

    // ensure current token is an identifier
    void expectIdentifier() {
        if (!hasMore() || type() != "identifier") {
            cerr << "error: identifier expected but got '" << text() << "'\n";
            exit(1);
        }
    }
};

// symbol table to store variable information
class SymbolTable {
    struct Info { string type; string segment; int index; };
    map<string, Info> classScope, subScope;
    int staticCount = 0, fieldCount = 0, argCount = 0, varCount = 0;
public:
    void resetSubroutine() { subScope.clear(); argCount = 0; varCount = 0; }

    // define a variable into class or subroutine scope
    void define(const string& name, const string& type, const string& kind) {
        if (kind == "static") classScope[name] = {type, "static", staticCount++};
        else if (kind == "field") classScope[name] = {type, "this", fieldCount++};
        else if (kind == "argument") subScope[name] = {type, "argument", argCount++};
        else if (kind == "var") subScope[name] = {type, "local", varCount++};
    }

    bool exists(const string& name) { return subScope.count(name) || classScope.count(name); }

    string segmentOf(const string& name) {
        if (subScope.count(name)) return subScope[name].segment;
        if (classScope.count(name)) return classScope[name].segment;
        return "";
    }

    string typeOf(const string& name) {
        if (subScope.count(name)) return subScope[name].type;
        if (classScope.count(name)) return classScope[name].type;
        return "";
    }

    int indexOf(const string& name) {
        if (subScope.count(name)) return subScope[name].index;
        if (classScope.count(name)) return classScope[name].index;
        return -1;
    }

    int localCount() const { return varCount; }
    int fieldCountNum() const { return fieldCount; }
};

// simple writer for vm commands
class VMWriter {
    ofstream out;
public:
    bool open(const string& path) { out.open(path); return out.is_open(); }
    void close() { if (out) out.close(); }

    void write(const string& line) { out << line << "\n"; }
    void push(const string& seg, int i) { write("push " + seg + " " + to_string(i)); }
    void pop(const string& seg, int i) { write("pop " + seg + " " + to_string(i)); }
    void op(const string& cmd) { write(cmd); }
    void label(const string& l) { write("label " + l); }
    void go(const string& l) { write("goto " + l); }
    void ifgo(const string& l) { write("if-goto " + l); }
    void call(const string& f, int n) { write("call " + f + " " + to_string(n)); }
    void func(const string& f, int n) { write("function " + f + " " + to_string(n)); }
    void ret() { write("return"); }
};

// compiler class that handles parsing and vm generation
class JackCompiler {
    TokenStream &tk;
    VMWriter &vm;
    SymbolTable symbols;
    string className, funcName;
    int labelCount = 0;
public:
    JackCompiler(TokenStream& t, VMWriter& v) : tk(t), vm(v) {}

    void compileClass() {
        tk.expect("class");
        tk.expectIdentifier(); className = tk.text(); tk.advance();
        tk.expect("{");

        while (tk.text() == "static" || tk.text() == "field") compileClassVar();
        while (tk.text() == "constructor" || tk.text() == "function" || tk.text() == "method")
            compileSubroutine();

        tk.expect("}");
    }

private:
    string newLabel(const string& prefix) {
        return className + "." + funcName + "$" + prefix + to_string(labelCount++);
    }

    void compileClassVar() {
        string kind = tk.text(); tk.advance();
        string type = compileType();
        tk.expectIdentifier(); string name = tk.text(); tk.advance();
        symbols.define(name, type, kind);

        while (tk.text() == ",") {
            tk.advance();
            tk.expectIdentifier(); string name2 = tk.text(); tk.advance();
            symbols.define(name2, type, kind);
        }
        tk.expect(";");
    }

    void compileSubroutine() {
        string subType = tk.text(); tk.advance();
        if (tk.text() == "void" || tk.type() == "keyword" || tk.type() == "identifier")
            tk.advance();
        else {
            cerr << "error: invalid return type\n";
            exit(1);
        }

        tk.expectIdentifier();
        funcName = tk.text();
        tk.advance();
        symbols.resetSubroutine();
        labelCount = 0;

        if (subType == "method")
            symbols.define("this", className, "argument");

        tk.expect("(");
        compileParameterList();
        tk.expect(")");
        compileSubroutineBody(subType);
    }

    void compileParameterList() {
        if (tk.text() == ")") return;
        while (true) {
            string type = compileType();
            tk.expectIdentifier();
            string name = tk.text();
            tk.advance();
            symbols.define(name, type, "argument");
            if (tk.text() == ",") { tk.advance(); continue; }
            break;
        }
    }

    void compileSubroutineBody(const string& kind) {
        tk.expect("{");

        while (tk.text() == "var")
            compileVar();

        vm.func(className + "." + funcName, symbols.localCount());

        if (kind == "constructor") {
            vm.push("constant", symbols.fieldCountNum());
            vm.call("Memory.alloc", 1);
            vm.pop("pointer", 0);
        } else if (kind == "method") {
            vm.push("argument", 0);
            vm.pop("pointer", 0);
        }

        compileStatements();
        tk.expect("}");
    }

    void compileVar() {
        tk.expect("var");
        string type = compileType();
        tk.expectIdentifier();
        string name = tk.text(); tk.advance();
        symbols.define(name, type, "var");

        while (tk.text() == ",") {
            tk.advance();
            tk.expectIdentifier();
            string name2 = tk.text(); tk.advance();
            symbols.define(name2, type, "var");
        }
        tk.expect(";");
    }

    void compileStatements() {
        while (tk.hasMore()) {
            string t = tk.text();
            if (t == "let") compileLet();
            else if (t == "if") compileIf();
            else if (t == "while") compileWhile();
            else if (t == "do") compileDo();
            else if (t == "return") compileReturn();
            else break;
        }
    }

    // let statement
    void compileLet() {
        tk.expect("let");
        tk.expectIdentifier();
        string name = tk.text(); tk.advance();
        bool isArray = false;

        if (tk.text() == "[") {
            isArray = true;
            tk.advance();
            compileExpression();
            tk.expect("]");
            pushVar(name);
            vm.op("add");
        }

        tk.expect("=");
        compileExpression();
        tk.expect(";");

        if (isArray) {
            vm.pop("temp", 0);
            vm.pop("pointer", 1);
            vm.push("temp", 0);
            vm.pop("that", 0);
        } else {
            popVar(name);
        }
    }

    // if statement
    void compileIf() {
        tk.expect("if");
        tk.expect("(");
        compileExpression();
        tk.expect(")");
        string elseLbl = newLabel("ELSE");
        string endLbl = newLabel("END_IF");
        vm.op("not");
        vm.ifgo(elseLbl);

        tk.expect("{");
        compileStatements();
        tk.expect("}");
        vm.go(endLbl);
        vm.label(elseLbl);

        if (tk.text() == "else") {
            tk.advance();
            tk.expect("{");
            compileStatements();
            tk.expect("}");
        }
        vm.label(endLbl);
    }

    // while statement
    void compileWhile() {
        tk.expect("while");
        string startLbl = newLabel("LOOP");
        string endLbl = newLabel("END");
        vm.label(startLbl);
        tk.expect("(");
        compileExpression();
        tk.expect(")");
        vm.op("not");
        vm.ifgo(endLbl);
        tk.expect("{");
        compileStatements();
        tk.expect("}");
        vm.go(startLbl);
        vm.label(endLbl);
    }

    // do statement
    void compileDo() {
        tk.expect("do");
        compileCall();
        tk.expect(";");
        vm.pop("temp", 0);
    }

    // return statement
    void compileReturn() {
        tk.expect("return");
        if (tk.text() != ";")
            compileExpression();
        else
            vm.push("constant", 0);
        tk.expect(";");
        vm.ret();
    }

    // handle expression logic
    void compileExpression() {
        compileTerm();
        while (isOp(tk.text())) {
            string op = tk.text();
            tk.advance();
            compileTerm();
            emitOp(op);
        }
    }

    // compile a term inside expression
    void compileTerm() {
        if (tk.type() == "integerConstant") {
            vm.push("constant", stoi(tk.text()));
            tk.advance();
        } else if (tk.type() == "stringConstant") {
            string s = tk.text(); tk.advance();
            vm.push("constant", s.size());
            vm.call("String.new", 1);
            for (char c : s) {
                vm.push("constant", (int)c);
                vm.call("String.appendChar", 2);
            }
        } else if (tk.text() == "(") {
            tk.advance();
            compileExpression();
            tk.expect(")");
        } else if (tk.text() == "-" || tk.text() == "~") {
            string u = tk.text(); tk.advance();
            compileTerm();
            vm.op(u == "-" ? "neg" : "not");
        } else if (tk.type() == "identifier") {
            string name = tk.text(), look = tk.nextText();
            if (look == "[") {
                tk.advance(); tk.advance();
                compileExpression();
                tk.expect("]");
                pushVar(name);
                vm.op("add");
                vm.pop("pointer", 1);
                vm.push("that", 0);
            } else if (look == "(" || look == ".") {
                compileCall();
            } else {
                pushVar(name);
                tk.advance();
            }
        } else {
            cerr << "unexpected term: " << tk.text() << endl;
            exit(1);
        }
    }

    // compile subroutine or function calls
    void compileCall() {
        string name = tk.text(); tk.advance();
        string full;
        int nArgs = 0;
        if (tk.text() == ".") {
            tk.advance();
            string sub = tk.text(); tk.advance();
            if (symbols.exists(name)) {
                pushVar(name);
                full = symbols.typeOf(name) + "." + sub;
                nArgs = 1;
            } else {
                full = name + "." + sub;
            }
        } else {
            vm.push("pointer", 0);
            full = className + "." + name;
            nArgs = 1;
        }
        tk.expect("(");
        nArgs += compileExprList();
        tk.expect(")");
        vm.call(full, nArgs);
    }

    int compileExprList() {
        if (tk.text() == ")") return 0;
        int count = 0;
        compileExpression();
        ++count;
        while (tk.text() == ",") { tk.advance(); compileExpression(); ++count; }
        return count;
    }

    string compileType() {
        if (tk.text() == "int" || tk.text() == "char" || tk.text() == "boolean" || tk.type() == "identifier") {
            string res = tk.text();
            tk.advance();
            return res;
        }
        cerr << "error: type expected but got '" << tk.text() << "'\n";
        exit(1);
    }

    bool isOp(const string& s) { return s.size() == 1 && string("+-*/&|<>=").find(s) != string::npos; }

    void emitOp(const string& op) {
        if (op == "+") vm.op("add");
        else if (op == "-") vm.op("sub");
        else if (op == "*") vm.call("Math.multiply", 2);
        else if (op == "/") vm.call("Math.divide", 2);
        else if (op == "&") vm.op("and");
        else if (op == "|") vm.op("or");
        else if (op == "<") vm.op("lt");
        else if (op == ">") vm.op("gt");
        else if (op == "=") vm.op("eq");
    }

    void pushVar(const string& name) {
        string seg = symbols.segmentOf(name);
        int idx = symbols.indexOf(name);
        if (seg.empty()) {
            cerr << "use of undeclared var: " << name << endl;
            exit(1);
        }
        vm.push(seg, idx);
    }

    void popVar(const string& name) {
        string seg = symbols.segmentOf(name);
        int idx = symbols.indexOf(name);
        if (seg.empty()) {
            cerr << "assignment to undeclared var: " << name << endl;
            exit(1);
        }
        vm.pop(seg, idx);
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        cerr << "usage: " << argv[0] << " <jack file or directory>" << endl;
        return 1;
    }

    fs::path input(argv[1]);
    vector<string> files;

    // check if given path is directory or single file
    if (fs::is_directory(input)) {
        for (auto& entry : fs::directory_iterator(input)) {
            if (entry.path().extension() == ".jack")
                files.push_back(entry.path().string());
        }
    } else if (input.extension() == ".jack") {
        files.push_back(input.string());
    } else {
        cerr << "no valid .jack file found" << endl;
        return 1;
    }

    // process each jack file and produce vm file
    for (const auto& f : files) {
        auto tokens = tokenizeJackFile(f);
        TokenStream tk(tokens);

        fs::path out = fs::path(f).parent_path() /
                      (fs::path(f).stem().string() + ".vm");

        VMWriter vm;
        if (!vm.open(out.string())) {
            cerr << "cannot open output file: " << out << endl;
            return 1;
        }

        JackCompiler compiler(tk, vm);
        compiler.compileClass();
        vm.close();
    }

    return 0;
}
