//
//  AST-fooling-around
//

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

const std::string testInput = "number myFirstTest = 5 * 2 - 1 + 7 / 5 * 1 + 10 / ( 3 + 1 ) / 1; number mySecondTest = 3*(2+10)*5/1+1+2+(3+(4+(5)))+6*(7+8+(9+10+11+12+(13*14)));";

bool isInteger(std::string s) {
    if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;
    char* p;
    strtol(s.c_str(), &p, 10);
    return *p == 0;
} 

enum class NodeType {
    ROOT_DOCUMENT,
    VARIABLE_DECLARATION,
    VARIABLE_ASSIGNMENT,
    VARIABLE_REFERENCE,
    EXPRESSION,
    LITERAL,
    OPERATION,
    FUNCTION_CALL
};

std::string getNodeTypeName(const NodeType& t) {
    switch(t) {
        case NodeType::ROOT_DOCUMENT:
            return "ROOT_DOCUMENT";
        case NodeType::VARIABLE_DECLARATION:
            return "VARIABLE_DECLARATION";
        case NodeType::VARIABLE_ASSIGNMENT:
            return "VARAIBLE_ASSIGNMENT";
        case NodeType::EXPRESSION:
            return "EXPRESSION";
        case NodeType::LITERAL:
            return "LITERAL";
        case NodeType::OPERATION:
            return "OPERATION";
    }
    return "UNKNOWN";
}

class NodeContext {
public:
    std::string key;
    std::string value;
    NodeContext( std::string _key, std::string _value ) : key(_key), value(_value) {}
};

int globalId = 0;

class AST {
private:
    AST* parent;
    std::vector<AST*> childNodes;
    std::vector<NodeContext> context;
    NodeType type;
    int id;
public:
    AST(NodeType _type) : parent(NULL), type(_type), id(globalId++) {}
    int getId() const {
        return id;
    }
    AST* getParent() {
        return parent;
    }
    void addNode(AST* node) {
        node->parent = this;
        childNodes.push_back(node);
    }
    void replaceNode(AST* oldNode, AST* newNode) {
        for( size_t i = 0; i < childNodes.size(); i++ ) {
            if(childNodes[i]->id == oldNode->id) {
                childNodes[i] = newNode;
                newNode->parent = this;
                oldNode->parent = NULL;
                return;
            }
        }
    }
    AST* getNextSibling(AST* node) {
        for( size_t i = 0; i < childNodes.size(); i++ ) {
            if(childNodes[i]->id == node->id) {
                if(i+1 < childNodes.size()) {
                    return childNodes[i+1];
                }
                return NULL;
            }
        }
        return NULL;
    }
    void addContext(std::string key, std::string value) {
        context.push_back(NodeContext(key,value));
    }
    void setType(NodeType _type) {
        type = _type;
    }
    NodeType getType() {
        return type;
    }
    AST* getChild(size_t i) {
        if(i < 0 || i >= childNodes.size()) {
            return NULL;
        }
        return childNodes[i];
    }
    size_t getChildCount() const {
        return childNodes.size();
    }
    const NodeContext* getContext(std::string key) {
        for(auto it=std::begin(context); it != std::end(context); it++) {
            if((*it).key == key) {
                return &(*it);
            }
        }
        return NULL;
    }
    void printAll() const {
        std::cout << "\n==AST NODE==\n";
        std::cout << "Id: " << id << "\n";
        std::cout << "Type: " << getNodeTypeName(type) << "\n";
        std::cout << "Node Count: " << childNodes.size();
        for(size_t i = 0; i < context.size(); i++) {
            std::cout << "\nField " << i << ": (" << context[i].key << ", " << context[i].value << ")";
        }
        std::cout << "\n==END AST NODE==\n";
        for(auto node: childNodes) {
            node->printAll();
        }
    }
    virtual ~AST() {
        for(auto node: childNodes) {
            delete node;
        }
        childNodes.clear();
    }
};

void pullTokens(const std::string& input, std::vector<std::string>* tokens) {
    std::string token = "";
    for(int i = 0; i < input.length(); i++) {
        char c = input[i];
        bool isSpace = c == ' ';
        bool isPunctuation = c == '+' || c == '-' || c == '*' || c == '/' || c == ';';
        bool isParanthesis = c == '(' || c == ')';
        if(!isSpace && !isPunctuation && !isParanthesis) {
            token += c;
        }
        if(isParanthesis || isPunctuation || isSpace || i == input.length()-1) {
            if(token.size()) {
                (*tokens).push_back(token);
            }
            if(isParanthesis || isPunctuation) {
                (*tokens).push_back(std::string(1,c));
            }
            token.clear();
        }
    }
}

void interpretTokens(const std::vector<std::string>& tokens, AST* rootAst) {
    AST* ast = NULL;
    AST* exprAst = NULL;
    AST* prevAst = NULL;
    for(auto it=std::begin(tokens); it != std::end(tokens); it++) {
        const std::string token = (*it);
        const std::string* next = it+1 == std::end(tokens)? NULL : &(*(it+1));
        
        if(token == "number") {
            if(!ast) {
                ast = new AST(NodeType::VARIABLE_DECLARATION);
                ast->addContext("TYPE", "number");
                ast->addContext("NAME", *next);
                it++;
                prevAst = ast;
                rootAst->addNode(ast);
            } else {
                throw "Unexpected variable declaration";
            }
        } else if(token == ";") {
            if(exprAst) {
                if(exprAst->getParent()) {
                    throw "Unexpected end of expression";
                }
                ast->addNode(exprAst);
                ast = NULL;
            }
        } else if(token == "+" || token == "-" || token == "*" || token == "/") {
            if(exprAst) {
                if(exprAst->getChildCount() == 1) {
                    AST* exprChild = new AST(NodeType::OPERATION);
                    exprChild->addContext("TYPE", token);
                    exprAst->addNode(exprChild);
                } else if(exprAst->getChildCount() == 3) {
                    std::string prevToken = exprAst->getChild(1)->getContext("TYPE")->value;
                    if((token == "*" || token == "/") && (prevToken == "+" || prevToken == "-")) {
                        AST* newLh = exprAst->getChild(2);
                        AST* newExpr = new AST(NodeType::EXPRESSION);
                        AST* op = new AST(NodeType::OPERATION);
                        op->addContext("TYPE", token);
                        exprAst->replaceNode(newLh, newExpr);
                        newExpr->addNode(newLh);
                        newExpr->addNode(op);
                        newExpr->addContext("IMPLICIT_PAREN", "1");
                        exprAst = newExpr;
                    } else {
                        AST* oldParent = exprAst->getParent();
                        AST* newParent = new AST(NodeType::EXPRESSION);
                        if(oldParent) {
                            oldParent->replaceNode(exprAst, newParent);
                        }
                        newParent->addNode(exprAst);
                        AST* exprChild = new AST(NodeType::OPERATION);
                        exprChild->addContext("TYPE", token);
                        newParent->addNode(exprChild);
                        exprAst = newParent;
                    }
                } else {
                    throw "Invalid placement of operator";
                }
            }
        } else if(token == "=") {
            if(prevAst) {
                if(prevAst->getType() == NodeType::VARIABLE_DECLARATION) {
                    ast = new AST(NodeType::VARIABLE_ASSIGNMENT);
                    ast->addContext("TARGET", prevAst->getContext("NAME")->value);
                    rootAst->addNode(ast);
                    exprAst = new AST(NodeType::EXPRESSION);
                }
            }
        } else if(token == "(") {
            if( exprAst ) {
                AST* exprChild = new AST(NodeType::EXPRESSION);
                exprAst->addNode(exprChild);
                exprAst = exprChild;
            } else {
                throw "Unexpected expression";
            }
        } else if(token == ")") {
            if(exprAst->getParent()) {
                if(exprAst->getParent()->getContext("IMPLICIT_PAREN")) {
                    exprAst = exprAst->getParent();
                }
                exprAst = exprAst->getParent();
            }
        } else {
            if(!ast) {
                if(next) {
                    if(*next == "(") {
                        ast = new AST(NodeType::FUNCTION_CALL);
                        ast->addContext("TARGET", token);
                        it++;
                    } else if(*next == "=") {
                        ast = new AST(NodeType::VARIABLE_ASSIGNMENT);
                        ast->addContext("TARGET", token);
                        rootAst->addNode(ast);
                        exprAst = new AST(NodeType::EXPRESSION);
                        it++;
                    } else {
                        throw "Invalid statement";
                    }
                } else {
                    throw "Invalid statement";
                }
            } else {
                if(exprAst) {
                    if(isInteger(token)) {
                        AST* exprChild = new AST(NodeType::LITERAL);
                        exprChild->addContext("VALUE", token);
                        exprAst->addNode(exprChild);
                        if(exprAst->getContext("IMPLICIT_PAREN")) {
                            exprAst = exprAst->getParent();
                        }
                    } else {
                        AST* exprChild = new AST(NodeType::VARIABLE_REFERENCE);
                        exprChild->addContext("TARGET", token);
                        exprAst->addNode(exprChild);
                    }
                }
            }
        }
    }
}

void outputResults(std::vector<std::string>* tokens, AST* rootAst) {
    for(auto v: *tokens) {
        std::cout << v << " ";
    }
    std::cout << "\n\n";
    rootAst->printAll();
}

int evaluateExpression(AST* exprAst) {
    AST* lh = exprAst->getChild(0);
    AST* op = exprAst->getChild(1);
    AST* rh = exprAst->getChild(2);
    int lValue = 0, rValue = 0;
    
    if(lh) {
        if(lh->getType() == NodeType::EXPRESSION) {
            lValue = evaluateExpression(lh);
        } else if(lh->getType() == NodeType::LITERAL) {
            lValue = atoi(lh->getContext("VALUE")->value.c_str());
        }
    }
    
    if(op && rh) {
        if(rh->getType() == NodeType::EXPRESSION) {
            rValue = evaluateExpression(rh);
        } else if(rh->getType() == NodeType::LITERAL) {
            rValue = atoi(rh->getContext("VALUE")->value.c_str());
        }
        switch(op->getContext("TYPE")->value[0]) {
            case '+': {
                return lValue+rValue;
            }
            case '-': {
                return lValue-rValue;
            }
            case '*': {
                return lValue*rValue;
            }
            case '/': {
                return lValue/rValue;
            }
            default: {
                break;
            }
        }
    }
    return lValue;
}

void runAst(AST* rootAst) {
    AST* curAst = rootAst;
    std::vector<int> traversed;
    while(curAst) {
        AST* parent = curAst->getParent();
        AST* nextSibling = parent? parent->getNextSibling(curAst) : NULL;
        AST* next = nextSibling;
        if(std::find(traversed.begin(), traversed.end(),curAst->getId()) == traversed.end()) {
            traversed.push_back(curAst->getId());
            switch(curAst->getType()) {
                case NodeType::ROOT_DOCUMENT: {
                    std::cout << "Entered root of AST\n";
                    next = curAst->getChild(0);
                    break;
                }
                case NodeType::VARIABLE_DECLARATION: {
                    std::cout << "Creating variable named " << curAst->getContext("NAME")->value << " of type: " << curAst->getContext("TYPE")->value << "\n";
                    break;
                }
                case NodeType::VARIABLE_ASSIGNMENT: {
                    const int value = evaluateExpression(curAst->getChild(0));
                    std::cout << "Assigned value of " << value << " to " << curAst->getContext("TARGET")->value << "\n";
                    break;
                }
                default: {
                   
                    break;
                }
            }
        }
        if(!next) {
            if(nextSibling) {
                next = nextSibling;
            } else if(parent) {
                next = parent;
            }
        }
        curAst = next;
    }
}

void cleanup(AST* rootAst) {
    delete rootAst;
}

void parseCode(const std::string& input) {
    AST* rootAst = new AST(NodeType::ROOT_DOCUMENT);
    try {
        std::vector<std::string> tokens;
        pullTokens(input, &tokens);
        interpretTokens(tokens, rootAst);
        outputResults(&tokens, rootAst);
        runAst(rootAst);
        cleanup(rootAst);
    } catch( const char* e ) {
        std::cout << "Exception: " << e;
    }
}

int main(int argc, const char * argv[]) {
    parseCode(testInput);
    return 0;
}
