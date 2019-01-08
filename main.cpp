#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <set>

using std::string; using std::vector; using std::istream; using std::move;

/*
 * Lexer - manages tokenization for Syntaxer
 */
class Lexer
{
public:
    typedef enum {null, lparen, rparen, lbrace, rbrace, times, slash, plus,
        minus, mod, andsym, orsym, eql, neq, lss, gtr, semicolon, comma, ifsym, ident, number } Type;

    struct Token
    {
        string value;
        Type type;
        Token() { type = null; value = ""; };
        Token(string value, Type type) { this->value = value; this->type = type; }
    };

    std::istream* buf;

    std::vector<Token> tokens;

    Lexer(std::istream& s);

private:
    void skipws();

    bool match(Token&);

    bool next(Token&);

public:

    bool eof();

    bool analyse(std::vector<Token>&);
};

Lexer::Lexer(std::istream& s) {
    buf = &s;
    tokens.emplace_back("(", lparen);   tokens.emplace_back(")", rparen);
    tokens.emplace_back("{", lbrace);   tokens.emplace_back("}", rbrace);
    tokens.emplace_back("*", times);    tokens.emplace_back("/", slash);
    tokens.emplace_back("+", plus);     tokens.emplace_back("-", minus);
    tokens.emplace_back("%", mod);
    tokens.emplace_back("&&", andsym);  tokens.emplace_back("||", orsym);
    tokens.emplace_back("==", eql);     tokens.emplace_back("!=", neq);
    tokens.emplace_back("<", lss);      tokens.emplace_back(">", gtr);
    tokens.emplace_back(";", semicolon);
    tokens.emplace_back(",", comma);
    tokens.emplace_back("if", ifsym);
    tokens.emplace_back("", ident);
    tokens.emplace_back("", number);
}

void Lexer::skipws() { while (isspace(buf->peek())) buf->get(); }

bool Lexer::match(Lexer::Token& token)
{
    skipws();
    auto pos = buf->tellg();

    string s;
    if (token.type == ident || token.type == number) {
        while ((token.type == ident ? isalpha(buf->peek()) : isdigit(buf->peek())))
            s += (char)buf->get(); // TODO - inefficient
        if (!s.empty()) { token.value = s; return true; }
    }
    else {
        while (buf->peek() == token.value[buf->tellg() - pos]) {
            s += string(1, buf->get());
            if (s.size() == token.value.size()) return true;
        }
    }

    buf->seekg(pos); return false;
}

bool Lexer::next(Token& token)
{
    for (int i = 0; i < tokens.size(); ++i) {
        token = tokens[i];
        if (match(token))
            return true;
    }
    return false;
}

//bool Lexer::peek()
//{
//
//}

bool Lexer::eof()
{
    skipws(); return buf->eof() || buf->peek() == EOF;
}

bool Lexer::analyse(std::vector<Lexer::Token>& tokens)
{
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
class Syntaxer
{
public:

    std::unique_ptr<Lexer> lexer;

    struct Node
    {
        typedef std::unique_ptr<Node> Ptr;
        typedef std::vector<Ptr> PtrVec;
        typedef enum {generic, function, function_call, params, params_call, expression, conditional_expression, terminal} Type;

        Type type;
        PtrVec next;
        Lexer::Token token;

        Node() { token = Lexer::Token(); type = generic; };
        explicit Node(Lexer::Token token) { this->token = move(token); type = terminal; }
        
        static Ptr MakePtr() { return move(std::make_unique<Node>()); }
        static Ptr MakePtr(Lexer::Token t) { return move(std::make_unique<Node>(t)); }

        bool is_terminal() { return token.type != Lexer::null; }
        string value() { return token.value; }
        PtrVec flatten() { return move(next); }
        void append(PtrVec other) {
            next.insert(next.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
        }
    };

    typedef Syntaxer::Node::Ptr (Syntaxer::*FuncPtr)();

    std::vector<Lexer::Token> tokens;
    std::vector<Lexer::Token>::iterator pos;

    explicit Syntaxer(std::istream& is) { lexer = move(std::make_unique<Lexer>(is)); }

    Node::Ptr terminal(Lexer::Type type) {
        Node::Ptr node = Node::MakePtr();

        if (eof() || pos->type != type) { node = nullptr; }
        else {
            node = Node::MakePtr(*pos);
            pos++;
        }

        return move(node);
    }

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
    TERMINAL(semicolon)
    TERMINAL(ident)
    TERMINAL(number)

	Node::Ptr conjunction(std::vector<FuncPtr> funcs) {
		Node::Ptr node = Node::MakePtr();
		auto orig_pos = pos;

		for (auto& func : funcs) {
			if (Node::Ptr child = (this->*func)())
                node->next.push_back(move(child));
			else { pos = orig_pos; node = nullptr; break; }
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

	BEGIN_RULE(function)
        node = conjunction({FUNC(ident), FUNC(params), FUNC(body)});
	    if(node != nullptr) node->type = Node::function;
    END_RULE

    BEGIN_RULE(body)
        node = conjunction({FUNC(lbrace), FUNC(body_inner), FUNC(rbrace)});
    END_RULE

    BEGIN_RULE(body_inner)  // TODO - use same format as params_call
        while(true) {
            if (Node::Ptr child = expression())
                node->next.push_back(move(child));
            else { node = nullptr; break; }
            if(Node::Ptr child = semicolon())
                node->next.push_back(move(child));
            else break;
        }
        if (node != nullptr && node->next.empty()) node = nullptr;
    END_RULE

    BEGIN_RULE(params)
        node->type = Node::params;
        while(true) {
            if (Node::Ptr child = terminal(Lexer::ident))
                node->next.push_back(move(child));
            else break;
        }
    END_RULE

    BEGIN_RULE(expression)
        node = disjunction({FUNC(binary_op), FUNC(body), FUNC(group), FUNC(function_call), FUNC(number)});
        if(node != nullptr) node->type = Node::expression;
    END_RULE

    BEGIN_RULE(binary_op)
        if (false) {}
        else if (node = conjunction({FUNC(body), FUNC(arithmetic_operator), FUNC(expression)})) {}
        else if (node = conjunction({FUNC(group), FUNC(arithmetic_operator), FUNC(expression)})) {}
        else if (node = conjunction({FUNC(function_call), FUNC(arithmetic_operator), FUNC(expression)})) {}
        else if (node = conjunction({FUNC(number), FUNC(arithmetic_operator), FUNC(expression)})) {}
//        else if (node = FUNC(condition_expression)) {}
    END_RULE

    BEGIN_RULE(group)
        node = conjunction({FUNC(lparen), FUNC(expression), FUNC(rparen)});
    END_RULE

    BEGIN_RULE(function_call)
        node = conjunction({FUNC(ident), FUNC(lparen), FUNC(params_call), FUNC(rparen)});
    END_RULE

    BEGIN_RULE(params_call)
        node->type = Node::params_call;
        while(true) {
            if (Node::Ptr child = expression())
                node->next.push_back(move(child));
            else break;
            if(Node::Ptr child = comma())
                node->append(params_call()->flatten());
            else break;
        }
    END_RULE

    BEGIN_RULE(arithmetic_operator)
        node = disjunction({FUNC(times), FUNC(slash), FUNC(plus), FUNC(minus), FUNC(mod)});
    END_RULE

//    BEGIN_RULE(condition_expression)
//        node = conjunction({FUNC(ifsym), FUNC(lparen), FUNC(condition), FUNC(rparen), FUNC(body), FUNC(body)})
//    END_RULE
//
//    BEGIN_RULE(condition)
//        node;
//    END_RULE

	BEGIN_RULE(program)
		while (!eof()) {
			if (Node::Ptr child = function())
				node->next.push_back(move(child));
			else { node = nullptr; break; }
		}

		if (node != nullptr && node->next.empty()) node = nullptr;
	END_RULE

    Node::Ptr analyse();

    bool eof();
};

bool Syntaxer::eof() { return (pos == tokens.end()); }

Syntaxer::Node::Ptr Syntaxer::analyse()
{
    if (lexer->analyse(tokens)) {
        lexer.reset();
        std::cout << "Lexical analysis ok" << std::endl;
    } // TODO - remove debug & check lexer.reset();
    else return nullptr;

    pos = tokens.begin();
	return move(program());
}

class Compiler
{

    std::stringstream os_temp;
    std::ostream* os;
    Syntaxer::Node::Ptr parse_tree;
    std::set<string> func_idents = {};
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
    const string exp_pro = "[](){" + return_sym + ws;
    const string exp_epi = ";}()";

public:

    Compiler(Syntaxer::Node::Ptr tree, std::ostream* os)
        //: os(os.rdbuf())
    {
        this->os = os;
        this->parse_tree = move(tree); ;
    }

private:

    bool expression(Syntaxer::Node::Ptr node)
    {
        bool res = true;
        if (node == nullptr) return false;

        os_temp << exp_pro;



        os_temp << exp_epi;

        return res;
    }

    bool body(Syntaxer::Node::Ptr node)
    {
        bool res = true;
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

    bool ident(Syntaxer::Node::Ptr node, std::set<string>& set)
    {
        bool res = true;
        if (node == nullptr) return false;

        if (node->is_terminal() && node->token.type == Lexer::ident)
        {
            string v = node->value();
            std::pair<std::set<string>::iterator,bool> ret = set.insert(v);
            res &= ret.second;
            os_temp << v;
        }

        return res;
    }


    bool function(Syntaxer::Node::Ptr node)
    {
        bool res = true;
        if (node == nullptr) return false;
        if (node->next.size() != 3) return false;
        var_idents = {};

        bool main;
        if (node->next[0] != nullptr && node->next[0]->is_terminal() && node->next[0]->token.type == Lexer::ident && node->next[0]->value() == "main") {
            main = true;
            os_temp << "int" << ws;
        }
        else
            os_temp << number << ws;

        res &= ident(move(node->next[0]), func_idents);

        os_temp << lparen;
        for (int i = 0; i < node->next[1]->next.size(); ++i)
        {
            os_temp << number << ws;
            res &= ident(move(node->next[1]->next[i]), var_idents);
            if (i < node->next[1]->next.size() - 1)
                os_temp << comma;
        }
        os_temp << rparen;

        os_temp << lbrace << return_sym << ws;
        res &= body(move(node->next[2]));
        if (main) os_temp << comma << "0";
        os_temp << semicolon << rbrace << std::endl;

        var_idents = {};
        return res;
    }

public:

    bool compile()
    {
        bool res = true;

        os_temp << "#include <iostream>" << std::endl;
        os_temp << std::endl;
        os_temp << "typedef uint64_t number;" << std::endl;
        os_temp << std::endl;
        os_temp << "number read(){number x; std::cin >> x;return x;}" << std::endl;
        func_idents.insert("read");
        os_temp << "number write(number x){std::cout << x;return x;}" << std::endl;
        func_idents.insert("write");

        for (int i = 0; i < parse_tree->next.size(); i++)
            res &= function(move(parse_tree->next[i]));

        string file_name = "test";
        std::fstream of;
        of.open(file_name + ".cpp", std::ios::out);

        if (res) { os_temp.clear(); os_temp.seekg(0, std::ios::beg); of << os_temp.str(); of.flush(); }

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

    Syntaxer syntaxer(is);
    Syntaxer::Node::Ptr tree = syntaxer.analyse();
    std::cout << (tree == nullptr ? "Syntax error" : "Syntax analysis ok") << std::endl; // TODO - only print to stderr

    std::fstream of;
    of.open(file_name + ".cpp", std::ios::out);
    if (!of) std::cout << "Open output failed";

    Compiler compiler(move(tree), &of);
    of.close();

    bool res = compiler.compile();
    if (res)
        std::cout << "Compiled successfully" << std::endl;
    else
        std::cout << "Compilation Error" << std::endl;
    typedef uint64_t number;
//    auto x = (number[] () { return ([] () { return 3; }()); }()), (number[] () { return ([] () { return 3; }()); }());
    number x = []() {
        return ((number)4, ([](){return (number)5;}() + 1));
    }();

    std::cout << x;

    return 0;
}