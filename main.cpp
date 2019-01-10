#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>

using std::string; using std::vector; using std::istream; using std::move;

/*
 * Lexer - manages tokenization for Syntaxer
 */
class Lexer {
public:
    typedef enum {
        null, lparen, rparen, lbrace, rbrace, times, slash, plus,
        minus, mod, andsym, orsym, eql, neq, lss, gtr, semicolon, comma, ifsym, negation, ident, number
    } Type;

    struct Token {
        string value;
        Type type;

        Token() {
            type = null;
            value = "";
        };

        Token(string value, Type type) {
            this->value = value;
            this->type = type;
        }
    };

    std::istream *buf;

    std::vector<Token> tokens;

    Lexer(std::istream &s);

private:
    void skipws();

    bool match(Token &);

    bool next(Token &);

public:

    bool eof();

    bool analyse(std::vector<Token> &);
};

Lexer::Lexer(std::istream &s) {
    buf = &s;
    tokens.emplace_back("(", lparen);
    tokens.emplace_back(")", rparen);
    tokens.emplace_back("{", lbrace);
    tokens.emplace_back("}", rbrace);
    tokens.emplace_back("*", times);
    tokens.emplace_back("/", slash);
    tokens.emplace_back("+", plus);
    tokens.emplace_back("-", minus);
    tokens.emplace_back("%", mod);
    tokens.emplace_back("&&", andsym);
    tokens.emplace_back("||", orsym);
    tokens.emplace_back("==", eql);
    tokens.emplace_back("!=", neq);
    tokens.emplace_back("<", lss);
    tokens.emplace_back(">", gtr);
    tokens.emplace_back(";", semicolon);
    tokens.emplace_back(",", comma);
    tokens.emplace_back("if", ifsym);
    tokens.emplace_back("!", negation);
    tokens.emplace_back("", ident);
    tokens.emplace_back("", number);
}

void Lexer::skipws() { while (isspace(buf->peek())) buf->get(); }

bool Lexer::match(Lexer::Token &token) {
    skipws();
    auto pos = buf->tellg();

    string s;
    if (token.type == ident || token.type == number) {
        while ((token.type == ident ? isalpha(buf->peek()) : isdigit(buf->peek())))
            s += (char) buf->get(); // TODO - inefficient
        if (!s.empty()) {
            token.value = s;
            return true;
        }
    } else {
        while (buf->peek() == token.value[buf->tellg() - pos]) {
            s += string(1, buf->get());
            if (s.size() == token.value.size()) return true;
        }
    }

    buf->seekg(pos);
    return false;
}

bool Lexer::next(Token &token) {
    for (const auto &i : tokens) {
        token = i;
        if (match(token))
            return true;
    }
    return false;
}

bool Lexer::eof() {
    skipws();
    return buf->eof() || buf->peek() == EOF;
}

bool Lexer::analyse(std::vector<Lexer::Token> &tokens) {
    Token t;
    while (next(t)) tokens.push_back(t);
    return eof();
}

#define FUNC(func) &Syntaxer::func
#define TERMINAL(token) Node::Ptr token() { return move(terminal(Lexer::token)); };

#define BEGIN_RULE(name) \
Node::Ptr name() \
{ \
    Node::Ptr node = Node::MakePtr(); \
    auto orig_pos = pos;

#define END_RULE \
    if (node == nullptr) { pos = orig_pos; } return move(node); \
};

/**
 * Syntaxer - analyzes syntax and if valid returns the parse tree.
 */
class Syntaxer {
public:
    struct Node {
        typedef std::unique_ptr<Node> Ptr;
        typedef std::vector<Ptr> PtrVec;
        typedef enum {
            generic,
            function,
            body,
            group,
            function_call,
            conditional_expression,
            binary_operator,
            basic_value,
            condition
        } Type;

        Type type;
        PtrVec next;
        Lexer::Token token;


        explicit Node() {
            token = Lexer::Token();
            type = generic;
        }

        explicit Node(Lexer::Token token) {
            this->token = move(token);
            type = basic_value;
        }

        Node(const Node &) = delete;

        Node(Node &&other) noexcept : next(move(other.next)), type(other.type) {}

        //Node& operator=(const Node&& other) noexcept { next = move(other.next); type = other.type; }
        Node &operator=(const Node &) = delete;

        ~Node() = default;

        static Ptr MakePtr() { return move(std::make_unique<Node>()); }

        static Ptr MakePtr(Lexer::Token t) { return move(std::make_unique<Node>(t)); }

        bool is_terminal() { return token.type != Lexer::null; }

        string value() { return token.value; }

        void append(Ptr &&node) {
            if (node == nullptr) return;
            for (auto &n : node->next) {
                if (n == nullptr) continue;
                else next.push_back(move(n));
            }
            node = nullptr;
        }
    };

    typedef Syntaxer::Node::Ptr (Syntaxer::*FuncPtr)();

    std::vector<Lexer::Token> tokens;
    std::vector<Lexer::Token>::iterator pos;

    explicit Syntaxer(std::vector<Lexer::Token>&& tokens) { this->tokens = move(tokens); }

    Node::Ptr terminal(Lexer::Type type) {
        Node::Ptr node = Node::MakePtr();

        if (eof() || pos->type != type) { node = nullptr; }
        else {
            node = Node::MakePtr(*pos);
            pos++;
        }

        return move(node);
    }

    Node::Ptr conjunction(std::vector<FuncPtr> funcs) {
        Node::Ptr node = Node::MakePtr();
        auto orig_pos = pos;

        for (auto &func : funcs) {
            if (Node::Ptr child = (this->*func)())
                node->next.push_back(move(child));
            else {
                pos = orig_pos;
                node = nullptr;
                break;
            }
        }

        return move(node);
    };

    Node::Ptr disjunction(std::vector<FuncPtr> funcs) {
        Node::Ptr node = nullptr;
        auto orig_pos = pos;

        for (auto &func : funcs)
            if (node = (this->*func)())
                break;

        return move(node);
    };

    TERMINAL(lbrace)

    TERMINAL(rbrace)

    TERMINAL(lparen)

    TERMINAL(rparen)

    TERMINAL(times)

    TERMINAL(slash)

    TERMINAL(plus)

    TERMINAL(minus)

    TERMINAL(mod)

    TERMINAL(andsym)

    TERMINAL(orsym)

    TERMINAL(eql)

    TERMINAL(neq)

    TERMINAL(lss)

    TERMINAL(gtr)

    TERMINAL(comma)

    TERMINAL(ifsym)

    TERMINAL(negation)

    TERMINAL(semicolon)

    TERMINAL(ident)

    TERMINAL(number)

    BEGIN_RULE(params_call)
        if (Node::Ptr child = expression())
            node->next.push_back(move(child));
        while (true) {
            if (Node::Ptr child = comma()) {
                node->next.push_back(move(child));
            } else break;
            if (Node::Ptr child = expression())
                node->next.push_back(move(child));
            else {
                node = nullptr;
                break;
            }
        }
    END_RULE

    BEGIN_RULE(function_call)
        node = conjunction({FUNC(ident), FUNC(lparen), FUNC(params_call), FUNC(rparen)});
        if (node != nullptr) node->type = Node::function_call;
    END_RULE

    BEGIN_RULE(binary_operator)
        node = disjunction(
                {FUNC(times), FUNC(slash), FUNC(plus), FUNC(minus), FUNC(mod), FUNC(eql), FUNC(neq), FUNC(lss),
                 FUNC(gtr)});
        if (node != nullptr) node->type = Node::binary_operator;
    END_RULE

    BEGIN_RULE(logical_operator)
        node = disjunction({FUNC(andsym), FUNC(orsym)});
    END_RULE

    BEGIN_RULE(logical_operation)
        node = conjunction({FUNC(logical_operator), FUNC(expression)});
    END_RULE

    BEGIN_RULE(binary_operation)
        node = conjunction({FUNC(binary_operator), FUNC(expression)});
    END_RULE

    BEGIN_RULE(condition)
        if (Node::Ptr child = negation())
            node->next.push_back(move(child));
        if (Node::Ptr child = expression())
            node->next.push_back(move(child));
        else node = nullptr;

        if (node != nullptr)
            if (auto op = move(logical_operation()))
                node->next.push_back(move(op));
        if (node != nullptr) node->type = Node::condition;
    END_RULE

    BEGIN_RULE(condition_expression)
        node = conjunction({FUNC(ifsym), FUNC(lparen), FUNC(condition), FUNC(rparen), FUNC(body), FUNC(body)});
        if (node != nullptr) node->type = Node::conditional_expression;
    END_RULE

    BEGIN_RULE(group)
        node = conjunction({FUNC(lparen), FUNC(expression), FUNC(rparen)});
        if (node != nullptr) node->type = Node::group;
    END_RULE

    BEGIN_RULE(expression)
        if (auto child =
                disjunction({FUNC(body), FUNC(group), FUNC(function_call), FUNC(condition_expression), FUNC(number),
                             FUNC(ident)}))
            node->next.push_back(move(child));
        if (!node->next.empty()) if (auto op = move(binary_operation())) node->next.push_back(move(op));
        if (node->next.empty()) node = nullptr;
    END_RULE

    BEGIN_RULE(params)
        while (true) {
            if (Node::Ptr child = terminal(Lexer::ident))
                node->next.push_back(move(child));
            else break;
        }
    END_RULE

    BEGIN_RULE(body_inner)  // TODO - use same format as params_call
        while (true) {
            if (Node::Ptr child = expression())
                node->next.push_back(move(child));
            else {
                node = nullptr;
                break;
            }
            if (Node::Ptr child = semicolon())
                node->next.push_back(move(child));
            else break;
        }
        if (node != nullptr && node->next.empty()) node = nullptr;
    END_RULE

    BEGIN_RULE(body)
        node = conjunction({FUNC(lbrace), FUNC(body_inner), FUNC(rbrace)});
        if (node != nullptr) node->type = Node::body;
    END_RULE

    BEGIN_RULE(function)
        node = conjunction({FUNC(ident), FUNC(params), FUNC(body)});
        if (node != nullptr) node->type = Node::function;
    END_RULE


    BEGIN_RULE(program)
        while (!eof()) {
            if (Node::Ptr child = function())
                node->next.push_back(move(child));
            else {
                node = nullptr;
                break;
            }
        }

        if (node != nullptr && node->next.empty()) node = nullptr;
    END_RULE

    Node::Ptr analyse() {
        pos = tokens.begin();
        return move(program());
    };

    bool eof() { return (pos == tokens.end()); };
};

/*
 * Compiler attempts to turn a parse tree generated in Syntaxer to the string result, if semantically correct.
 */
class Compiler {

    std::stringstream os_temp;
    std::ostream *os;
    Syntaxer::Node::Ptr parse_tree;
    std::map<string, int> func_idents = {};
    std::set<string> var_idents;

    const string number = "number";
    const string ws = " ";
    const string lbrace = "{";
    const string rbrace = "}";
    const string lparen = "(";
    const string rparen = ")";
    const string comma = ",";
    const string semicolon = ";";
    const string return_sym = "return";
    const string exp_pro = "[&](){" + return_sym + ws;
    const string exp_epi = ";}()";

public:

    Compiler(Syntaxer::Node::Ptr tree, std::ostream *os) {
        this->os = os;
        this->parse_tree = move(tree);
    }

private:


    bool logical_operation(Syntaxer::Node::Ptr node) {
        if (node == nullptr || node->next.size() != 2) return false;

        os_temp << node->next[0]->value();
        return expression(move(node->next[1]));
    }

    bool condition(Syntaxer::Node::Ptr node) {
        bool res = true;
        if (node == nullptr) return false;
        if (node->next.empty()) return false;

        int neg = 0;

        if (node->next[0]->token.type == Lexer::negation) {
            os_temp << "!";
            neg++;
        }

        os_temp << lparen;

        res &= expression(move(node->next[0 + neg]));

        if (node->next.size() == 2)
            res &= logical_operation(move(node->next[1 + neg]));

        os_temp << rparen;

        return res;
    }

    bool conditional_expression(Syntaxer::Node::Ptr node) {
        bool res = true;
        if (node == nullptr) return false;
        if (node->next.size() != 6) return false;

        os_temp << lparen;
        os_temp << lparen;
        res &= expression(move(node->next[2]));
        os_temp << rparen;
        os_temp << "?";
        res &= body(move(node->next[4]));
        os_temp << ":";
        res &= body(move(node->next[5]));
        os_temp << rparen;

        return res;
    }

    bool binary_operator(Syntaxer::Node::Ptr node) {
        if (node == nullptr || node->next.size() != 2) return false;

        os_temp << node->next[0]->value();
        return expression(move(node->next[1]));
    }

    bool value(Syntaxer::Node::Ptr node) {
        if (node == nullptr) return false;

        if (node->token.type == Lexer::Type::ident) {
            auto val = node->value();
            auto ret = var_idents.find(val);
            if (ret == var_idents.end()) return false;
            os_temp << val;
            return true;
        } else if (node->token.type == Lexer::Type::number) {
            os_temp << lparen << number << rparen << node->value();
            return true;
        }
        return false;
    }

    bool params_call(Syntaxer::Node::Ptr node, int &num_params) {
        bool res = true;
        if (node == nullptr) return false;

        num_params = 0;

        os_temp << lparen;
        for (int i = 0; i < node->next.size(); i = i + 2) {
            res &= expression(move(node->next[i]));
            if (i < node->next.size() - 1)
                os_temp << comma;
            num_params++;
        }
        os_temp << rparen;

        return res;
    }

    bool function_ident_call(Syntaxer::Node::Ptr node, int &num_params) {
        bool res = true;
        if (node == nullptr || !node->next.empty()) return false;

        auto val = node->value();
        auto ret = func_idents.find(val);
        if (ret == func_idents.end()) return false;
        os_temp << val;

        num_params = ret->second;

        return true;

    }

    bool function_call(Syntaxer::Node::Ptr node) {
        bool res = true;
        if (node == nullptr || node->next.size() != 4) return false;
        int num_params_want;
        res &= function_ident_call(move(node->next[0]), num_params_want);
        int num_params_got;
        res &= params_call(move(node->next[2]), num_params_got); // TODO - check number of parameters is correct

        res &= num_params_got == num_params_want;

        return res;
    }

    bool group(Syntaxer::Node::Ptr node) {
        bool res = true;
        if (node == nullptr || node->next.size() != 3) return false;

        os_temp << lparen;
        res &= expression(move(node->next[1]));
        os_temp << rparen;

        return res;
    }

    bool expression(Syntaxer::Node::Ptr node) {
        bool res = true;
        if (node == nullptr) return false;

        if (node->next.size() != 1 && node->next.size() != 2) return false;
        if (node->next[0] == nullptr) return false;

        if (false) {}
        else if (node->next[0]->type == Syntaxer::Node::body) res &= body(move(node->next[0]));
        else if (node->next[0]->type == Syntaxer::Node::group) res &= group(move(node->next[0]));
        else if (node->next[0]->type == Syntaxer::Node::function_call) res &= function_call(move(node->next[0]));
        else if (node->type == Syntaxer::Node::condition) { res &= condition(move(node)); node = nullptr; }
        else if (node->next[0]->type == Syntaxer::Node::conditional_expression)
            res &= conditional_expression(move(node->next[0]));
        else if (node->next[0]->type == Syntaxer::Node::basic_value) res &= value(move(node->next[0]));
        else return false;

        if (node != nullptr && node->next.size() == 2)
            res &= binary_operator(move(node->next[1]));

        return res;
    }

    bool body(Syntaxer::Node::Ptr node) {
        bool res = true;
        if (node == nullptr || node->next.size() != 3) return false;
        node = move(node->next[1]);
        if (node == nullptr) return false;

        os_temp << lparen;
        os_temp << exp_pro;
        for (int i = 0; i < node->next.size(); i = i + 2) {
            res &= expression(move(node->next[i]));
            if (i < node->next.size() - 1)
                os_temp << comma;
        }
        os_temp << exp_epi;
        os_temp << rparen;

        return res;
    }

    bool var_ident(Syntaxer::Node::Ptr node) {
        bool res = true;
        if (node == nullptr) return false;

        if (node->is_terminal() && node->token.type == Lexer::ident) {
            string v = node->value();
            auto func_ret = func_idents.find(v);
            if (v == number || func_ret != func_idents.end()) return false;
            auto ret = var_idents.insert(v);
            res &= ret.second;
            os_temp << v;
        }

        return res;
    }

    bool func_ident(Syntaxer::Node::Ptr node, int num_params) {
        bool res = true;
        if (node == nullptr) return false;

        if (node->is_terminal() && node->token.type == Lexer::ident) {
            string v = node->value();
            auto ret = func_idents.emplace(v, num_params);
            res &= ret.second;
            os_temp << v;
        }

        return res;
    }


    bool function(Syntaxer::Node::Ptr node) {
        bool res = true;
        if (node == nullptr) return false;
        if (node->next.size() != 3) return false;
        var_idents = {};

        bool main = false;
        if (node->next[0] != nullptr && node->next[0]->is_terminal() && node->next[0]->token.type == Lexer::ident &&
            node->next[0]->value() == "main") {
            main = true;
            os_temp << "int" << ws;
        } else
            os_temp << number << ws;

        res &= func_ident(move(node->next[0]), (int) node->next[1]->next.size());

        os_temp << lparen;
        for (int i = 0; i < node->next[1]->next.size(); ++i) {
            os_temp << number << ws;
            res &= var_ident(move(node->next[1]->next[i]));
            if (i < node->next[1]->next.size() - 1)
                os_temp << comma;
        }
        os_temp << rparen;

        os_temp << lbrace << return_sym << ws;
        res &= body(move(node->next[2])); // TODO
        if (main) os_temp << comma << "0";
        os_temp << semicolon << rbrace << std::endl;

        var_idents = {};
        return res;
    }

public:

    bool compile() {
        bool res = true;

        os_temp << "#include <iostream>" << std::endl;
        os_temp << std::endl;
        os_temp << "typedef uint64_t number;" << std::endl;
        os_temp << std::endl;
        os_temp << "number read(){number x; std::cin >> x;return x;}" << std::endl;
        func_idents.emplace("read", 0);
        os_temp << "number write(number x){std::cout << x << std::endl;return x;}" << std::endl;
        func_idents.emplace("write", 1);

        for (auto &i : parse_tree->next)
            res &= function(move(i));

        string file_name = "test";
        std::fstream of;
        of.open(file_name + ".cpp", std::ios::out);

        os_temp.clear();
        os_temp.seekg(0, std::ios::beg);

        if (res) {
            of << os_temp.str();
            of.flush();
        } else { std::cerr << os_temp.str(); }

        of.close();

        return res;
    }
};

int main(int argc, char **argv) {
    std::stringstream is;

    string file_name = "test";

    std::ifstream f;
    f.open(file_name);

    char c;
    while (f.get(c)) is.put(c);

    auto lexer = std::make_unique<Lexer>(is);
    std::vector<Lexer::Token> tokens;
    if (lexer->analyse(tokens))
        lexer.reset();
    else{
        std::cerr << "Lexical analysis failed" << std::endl;
        return 30;
    }

    Syntaxer syntaxer(move(tokens));
    Syntaxer::Node::Ptr tree = syntaxer.analyse();
    if (tree == nullptr)
    {
        std::cerr << "Syntax analysis failed" << std::endl;
        return 20;
    }

    std::fstream of;
    of.open(file_name + ".cpp", std::ios::out);
    if (!of) { std::cerr << "Open output failed"; return 100; }

    Compiler compiler(move(tree), &of);
    of.close();

    bool res = compiler.compile();
    if (!res) { std::cerr << "Compilation failed" << std::endl; return 10; }

    return 0;
}