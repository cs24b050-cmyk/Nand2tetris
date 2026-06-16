// JackCompiler.cpp
// Single-file Jack -> VM compiler (Tokenizer + Parser + SymbolTable + VMWriter)

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <sstream>
#include <cctype>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

struct Token {
    string type;   // "keyword","symbol","identifier","integerConstant","stringConstant"
    string value;
};

static const vector<string> KEYWORDS = {
    "class","constructor","function","method","field","static","var",
    "int","char","boolean","void","true","false","null","this",
    "let","do","if","else","while","return"
};

bool isKeyword(const string &w) {
    return find(KEYWORDS.begin(), KEYWORDS.end(), w) != KEYWORDS.end();
}
bool isSymbolChar(char c) {
    const string s = "{}()[].,;+-*/&|<>=~";
    return s.find(c) != string::npos;
}
string classifyToken(const string &w) {
    if (isKeyword(w)) return "keyword";
    if (w.size() == 1 && isSymbolChar(w[0])) return "symbol";
    if (!w.empty() && isdigit((unsigned char)w[0])) return "integerConstant";
    if (!w.empty() && w[0] == '"') return "stringConstant";
    return "identifier";
}

// Read a .jack file and emit a token vector.
vector<Token> tokenizeFile(const string &path) {
    ifstream in(path);
    vector<Token> tokens;
    if (!in.is_open()) return tokens;

    string src((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    string cur;
    bool inString = false;

    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];

        // line comment
        if (!inString && c == '/' && i + 1 < src.size() && src[i+1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            continue;
        }
        // block comment
        if (!inString && c == '/' && i + 1 < src.size() && src[i+1] == '*') {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i+1] == '/')) ++i;
            ++i;
            continue;
        }

        if (c == '"') {
            if (inString) {
                tokens.push_back({"stringConstant", cur});
                cur.clear();
                inString = false;
            } else {
                if (!cur.empty()) {
                    tokens.push_back({classifyToken(cur), cur});
                    cur.clear();
                }
                inString = true;
            }
            continue;
        }

        if (inString) {
            cur.push_back(c);
            continue;
        }

        if (isSymbolChar(c)) {
            if (!cur.empty()) {
                tokens.push_back({classifyToken(cur), cur});
                cur.clear();
            }
            tokens.push_back({"symbol", string(1, c)});
            continue;
        }

        if (isspace((unsigned char)c)) {
            if (!cur.empty()) {
                tokens.push_back({classifyToken(cur), cur});
                cur.clear();
            }
            continue;
        }

        cur.push_back(c);
    }

    if (!cur.empty()) tokens.push_back({classifyToken(cur), cur});
    return tokens;
}

class TokenStream {
    vector<Token> tokens;
    size_t pos = 0;
public:
    TokenStream() = default;
    TokenStream(const vector<Token>& t): tokens(t), pos(0) {}
    void reset(const vector<Token>& t) { tokens = t; pos = 0; }

    bool hasMore() const { return pos < tokens.size(); }
    void nextToken() { if (hasMore()) ++pos; }

    string currentToken() const { return hasMore() ? tokens[pos].value : string(); }
    string currentType()  const { return hasMore() ? tokens[pos].type  : string(); }
    string peekToken()    const { return (pos + 1 < tokens.size()) ? tokens[pos+1].value : string(); }

    // Assert current token equals expected and consume it.
    void expect(const string &expected) {
        if (!hasMore() || currentToken() != expected) {
            cerr << "Syntax error: expected '" << expected << "', found '" << currentToken() << "'\n";
            exit(1);
        }
        nextToken();
    }
};

class SymbolTable {
    struct Entry { string type; string kind; int idx; };
    unordered_map<string, Entry> classScope;
    unordered_map<string, Entry> subScope;
    int staticCount = 0;
    int fieldCount = 0;
    int argCount = 0;
    int varCount = 0;
public:
    void startSubroutine() {
        subScope.clear();
        argCount = 0;
        varCount = 0;
    }
    void define(const string &name, const string &type, const string &kind) {
        if (kind == "static") classScope[name] = {type, kind, staticCount++};
        else if (kind == "field") classScope[name] = {type, kind, fieldCount++};
        else if (kind == "argument") subScope[name] = {type, kind, argCount++};
        else if (kind == "var") subScope[name] = {type, "local", varCount++};
    }
    bool exists(const string &name) const {
        return subScope.count(name) || classScope.count(name);
    }
    string kindOf(const string &name) const {
        if (subScope.count(name)) return subScope.at(name).kind;
        if (classScope.count(name)) return classScope.at(name).kind;
        return "NONE";
    }
    string typeOf(const string &name) const {
        if (subScope.count(name)) return subScope.at(name).type;
        if (classScope.count(name)) return classScope.at(name).type;
        return "";
    }
    int indexOf(const string &name) const {
        if (subScope.count(name)) return subScope.at(name).idx;
        if (classScope.count(name)) return classScope.at(name).idx;
        return -1;
    }
    int localCount() const { return varCount; }
    int fieldCountClass() const { return fieldCount; }
};

class VMWriter {
    ofstream out;
public:
    bool open(const string &path) {
        out.open(path);
        return out.is_open();
    }
    void close() { if (out.is_open()) out.close(); }

    void push(const string &segment, int index) { out << "push " << segment << " " << index << "\n"; }
    void pop (const string &segment, int index) { out << "pop "  << segment << " " << index << "\n"; }
    void arithmetic(const string &cmd) { out << cmd << "\n"; }
    void label(const string &name) { out << "label " << name << "\n"; }
    void goTo(const string &name)  { out << "goto " << name << "\n"; }
    void ifGoto(const string &name){ out << "if-goto " << name << "\n"; }
    void call(const string &name,int n){ out << "call " << name << " " << n << "\n"; }
    void function(const string &name,int n){ out << "function " << name << " " << n << "\n"; }
    void ret() { out << "return\n"; }
};

class CompilationEngine {
    TokenStream &tokens;
    VMWriter &writer;
    SymbolTable symbols;
    string currentClass;
    string currentSub;
    int labelId = 0;
public:
    CompilationEngine(TokenStream &ts, VMWriter &vm): tokens(ts), writer(vm) {}

    // Top-level class: class ClassName { ... }
    void compileClass() {
        tokens.expect("class");
        if (tokens.currentType() != "identifier") { cerr << "Expected class name\n"; exit(1); }
        currentClass = tokens.currentToken();
        tokens.nextToken();

        tokens.expect("{");
        while (tokens.currentToken() == "static" || tokens.currentToken() == "field") compileClassVarDec();
        while (tokens.currentToken() == "constructor" || tokens.currentToken() == "function" || tokens.currentToken() == "method") compileSubroutineDec();
        tokens.expect("}");
    }

private:
    // helpers for labels scoped to the current function
    int nextLabelID() { return labelId++; }
    string createLabel(const string &kind, int id) {
        // e.g. Main.myFunc$WHILE0
        return currentClass + "." + currentSub + "$" + kind + to_string(id);
    }

    // classVarDec: (static|field) type varName (',' varName)* ;
    void compileClassVarDec() {
        string kind = tokens.currentToken(); // static or field
        tokens.nextToken();
        string type = parseType();
        string name = requireIdentifierAndAdvance();
        symbols.define(name, type, kind);
        while (tokens.currentToken() == ",") {
            tokens.nextToken();
            string n2 = requireIdentifierAndAdvance();
            symbols.define(n2, type, kind);
        }
        tokens.expect(";");
    }

    // subroutineDec: (constructor|function|method) (void|type) name ( parameterList ) subroutineBody
    void compileSubroutineDec() {
        string subKind = tokens.currentToken();
        tokens.nextToken();

        // return type (void or type)
        if (tokens.currentToken() == "void" || tokens.currentType() == "keyword" || tokens.currentType() == "identifier") {
            tokens.nextToken();
        } else { cerr << "Expected return type\n"; exit(1); }

        if (tokens.currentType() != "identifier") { cerr << "Expected subroutine name\n"; exit(1); }
        string subName = tokens.currentToken();
        tokens.nextToken();

        // begin subroutine scope
        symbols.startSubroutine();
        currentSub = subName;
        labelId = 0;

        if (subKind == "method") {
            // 'this' is argument 0
            symbols.define("this", currentClass, "argument");
        }

        tokens.expect("(");
        compileParameterList();
        tokens.expect(")");
        compileSubroutineBody(subName, subKind);
    }

    // parameterList: (type varName (',' type varName)*)?
    void compileParameterList() {
        if (tokens.currentToken() == ")") return;
        while (true) {
            string type = parseType();
            string name = requireIdentifierAndAdvance();
            symbols.define(name, type, "argument");
            if (tokens.currentToken() == ",") { tokens.nextToken(); continue; }
            break;
        }
    }

    // subroutineBody: { varDec* statements }
    void compileSubroutineBody(const string &subName, const string &subKind) {
        tokens.expect("{");
        while (tokens.currentToken() == "var") compileVarDec();

        int nLocals = symbols.localCount();
        string funcName = currentClass + "." + subName;
        writer.function(funcName, nLocals);

        if (subKind == "constructor") {
            int nFields = symbols.fieldCountClass();
            writer.push("constant", nFields);
            writer.call("Memory.alloc", 1);
            writer.pop("pointer", 0);
        } else if (subKind == "method") {
            writer.push("argument", 0);
            writer.pop("pointer", 0);
        }

        compileStatements();
        tokens.expect("}");
    }

    // varDec: var type varName (',' varName)* ;
    void compileVarDec() {
        tokens.expect("var");
        string type = parseType();
        string name = requireIdentifierAndAdvance();
        symbols.define(name, type, "var");
        while (tokens.currentToken() == ",") {
            tokens.nextToken();
            string n2 = requireIdentifierAndAdvance();
            symbols.define(n2, type, "var");
        }
        tokens.expect(";");
    }

    // statements: (let|if|while|do|return)*
    void compileStatements() {
        while (tokens.hasMore()) {
            string t = tokens.currentToken();
            if (t == "let") compileLet();
            else if (t == "if") compileIf();
            else if (t == "while") compileWhile();
            else if (t == "do") compileDo();
            else if (t == "return") compileReturn();
            else break;
        }
    }

    // let varName ( '[' expression ']' )? = expression ;
    void compileLet() {
        tokens.expect("let");
        string varName = requireIdentifierAndAdvance();
        bool isArray = false;
        if (tokens.currentToken() == "[") {
            isArray = true;
            tokens.expect("[");
            compileExpression();
            tokens.expect("]");
            pushVariable(varName);
            writer.arithmetic("add"); // compute target address
        }
        tokens.expect("=");
        compileExpression();
        tokens.expect(";");
        if (isArray) {
            writer.pop("temp", 0);
            writer.pop("pointer", 1);
            writer.push("temp", 0);
            writer.pop("that", 0);
        } else {
            popToVariable(varName);
        }
    }

    // if (expr) {statements} (else {statements})?
    void compileIf() {
        tokens.expect("if");
        tokens.expect("(");
        compileExpression();
        tokens.expect(")");

        int elseId = nextLabelID();
        int endId  = nextLabelID();
        string elseLabel = createLabel("ELSE", elseId);
        string endLabel  = createLabel("END",  endId);

        // if not expr -> goto elseLabel
        writer.arithmetic("not");
        writer.ifGoto(elseLabel);

        // true branch
        tokens.expect("{");
        compileStatements();
        tokens.expect("}");
        writer.goTo(endLabel);

        // else branch (if any)
        writer.label(elseLabel);
        if (tokens.currentToken() == "else") {
            tokens.nextToken();
            tokens.expect("{");
            compileStatements();
            tokens.expect("}");
        }
        writer.label(endLabel);
    }

    // while (expr) { statements }
    void compileWhile() {
        tokens.expect("while");
        int startId = nextLabelID();
        int endId   = nextLabelID();
        string startLabel = createLabel("WHILE_START", startId);
        string endLabel   = createLabel("WHILE_END", endId);

        writer.label(startLabel);
        tokens.expect("(");
        compileExpression();
        tokens.expect(")");
        writer.arithmetic("not");
        writer.ifGoto(endLabel);

        tokens.expect("{");
        compileStatements();
        tokens.expect("}");
        writer.goTo(startLabel);
        writer.label(endLabel);
    }

    // do subroutineCall ;
    void compileDo() {
        tokens.expect("do");
        compileSubroutineCall();
        tokens.expect(";");
        writer.pop("temp", 0); // discard return
    }

    // return expression? ;
    void compileReturn() {
        tokens.expect("return");
        if (tokens.currentToken() == ";") {
            writer.push("constant", 0);
        } else {
            compileExpression();
        }
        tokens.expect(";");
        writer.ret();
    }

    // expression: term (op term)*
    void compileExpression() {
        compileTerm();
        while (isOperator(tokens.currentToken())) {
            string op = tokens.currentToken();
            tokens.nextToken();
            compileTerm();
            emitOperator(op);
        }
    }

    // term: integer | string | keywordConstant | varName (...) | (expr) | unary term
    void compileTerm() {
        if (tokens.currentType() == "integerConstant") {
            int v = stoi(tokens.currentToken());
            writer.push("constant", v);
            tokens.nextToken();
        } else if (tokens.currentType() == "stringConstant") {
            string s = tokens.currentToken();
            tokens.nextToken();
            writer.push("constant", (int)s.size());
            writer.call("String.new", 1);
            for (unsigned char ch : s) {
                writer.push("constant", (int)ch);
                writer.call("String.appendChar", 2);
            }
        } else if (tokens.currentType() == "keyword") {
            string kw = tokens.currentToken();
            if (kw == "true")      { writer.push("constant", 1); writer.arithmetic("neg"); }
            else if (kw == "false"|| kw == "null") writer.push("constant", 0);
            else if (kw == "this") writer.push("pointer", 0);
            tokens.nextToken();
        } else if (tokens.currentToken() == "(") {
            tokens.expect("(");
            compileExpression();
            tokens.expect(")");
        } else if (tokens.currentToken() == "-" || tokens.currentToken() == "~") {
            string unary = tokens.currentToken();
            tokens.nextToken();
            compileTerm();
            if (unary == "-") writer.arithmetic("neg"); else writer.arithmetic("not");
        } else if (tokens.currentType() == "identifier") {
            string name = tokens.currentToken();
            string next = tokens.peekToken();

            if (next == "[") {
                tokens.nextToken(); // varName
                tokens.expect("[");
                compileExpression();
                tokens.expect("]");
                pushVariable(name);
                writer.arithmetic("add");
                writer.pop("pointer", 1);
                writer.push("that", 0);
            } else if (next == "(" || next == ".") {
                compileSubroutineCall();
            } else {
                pushVariable(name);
                tokens.nextToken();
            }
        } else {
            cerr << "Unexpected term: '" << tokens.currentToken() << "'\n";
            exit(1);
        }
    }

    // expressionList -> returns number of expressions
    int compileExpressionList() {
        if (tokens.currentToken() == ")") return 0;
        int count = 0;
        compileExpression(); ++count;
        while (tokens.currentToken() == ",") {
            tokens.nextToken();
            compileExpression(); ++count;
        }
        return count;
    }

    // subroutineCall: (className|varName).subName(...) | subName(...)
    pair<string,int> compileSubroutineCall() {
        string id = tokens.currentToken(); // identifier
        tokens.nextToken();

        int nArgs = 0;
        string fullName;

        if (tokens.currentToken() == ".") {
            tokens.nextToken();
            string subName = requireIdentifierAndAdvance();
            if (symbols.exists(id)) {
                // method on object instance
                pushVariable(id);
                fullName = symbols.typeOf(id) + "." + subName;
                nArgs = 1;
            } else {
                fullName = id + "." + subName; // class function
            }
        } else {
            // method in this class: push pointer 0
            writer.push("pointer", 0);
            fullName = currentClass + "." + id;
            nArgs = 1;
        }

        tokens.expect("(");
        int extra = compileExpressionList();
        nArgs += extra;
        tokens.expect(")");
        writer.call(fullName, nArgs);
        return {fullName, nArgs};
    }

    //small helpers
    string parseType() {
        if (tokens.currentToken() == "int" || tokens.currentToken() == "char" || tokens.currentToken() == "boolean" || tokens.currentType() == "identifier") {
            string t = tokens.currentToken();
            tokens.nextToken();
            return t;
        }
        cerr << "Expected type but found '" << tokens.currentToken() << "'\n";
        exit(1);
    }

    string requireIdentifierAndAdvance() {
        if (tokens.currentType() != "identifier") { cerr << "Expected identifier but found '" << tokens.currentToken() << "'\n"; exit(1); }
        string n = tokens.currentToken();
        tokens.nextToken();
        return n;
    }

    bool isOperator(const string &s) {
        const string ops = "+-*/&|<>=";
        return s.size() == 1 && ops.find(s[0]) != string::npos;
    }

    void emitOperator(const string &op) {
        if (op == "+") writer.arithmetic("add");
        else if (op == "-") writer.arithmetic("sub");
        else if (op == "*") writer.call("Math.multiply", 2);
        else if (op == "/") writer.call("Math.divide", 2);
        else if (op == "&") writer.arithmetic("and");
        else if (op == "|") writer.arithmetic("or");
        else if (op == "<") writer.arithmetic("lt");
        else if (op == ">") writer.arithmetic("gt");
        else if (op == "=") writer.arithmetic("eq");
    }

    void pushVariable(const string &name) {
        string k = symbols.kindOf(name);
        int idx = symbols.indexOf(name);
        if (k == "static") writer.push("static", idx);
        else if (k == "field" ) writer.push("this", idx);
        else if (k == "argument") writer.push("argument", idx);
        else if (k == "local") writer.push("local", idx);
        else { cerr << "Undefined variable '" << name << "'\n"; exit(1); }
    }

    void popToVariable(const string &name) {
        string k = symbols.kindOf(name);
        int idx = symbols.indexOf(name);
        if (k == "static") writer.pop("static", idx);
        else if (k == "field" ) writer.pop("this", idx);
        else if (k == "argument") writer.pop("argument", idx);
        else if (k == "local") writer.pop("local", idx);
        else { cerr << "Undefined variable '" << name << "'\n"; exit(1); }
    }
};

int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <file.jack | folder>\n";
        return 1;
    }

    string input = argv[1];
    fs::path p(input);
    vector<string> sources;

    if (fs::is_directory(p)) {
        for (auto &e : fs::directory_iterator(p)) {
            if (e.path().extension() == ".jack") sources.push_back(e.path().string());
        }
    } else if (fs::is_regular_file(p) && p.extension() == ".jack") {
        sources.push_back(input);
    } else {
        cerr << "Input must be a .jack file or a directory containing .jack files\n";
        return 1;
    }

    for (auto &file : sources) {
        vector<Token> toks = tokenizeFile(file);
        TokenStream ts(toks);

        fs::path fp(file);
        string out = (fp.parent_path() / (fp.stem().string() + ".vm")).string();

        VMWriter vm;
        if (!vm.open(out)) {
            cerr << "Can't open output file: " << out << "\n";
            return 1;
        }

        CompilationEngine engine(ts, vm);
        engine.compileClass();

        vm.close();
    }

    return 0;
}
