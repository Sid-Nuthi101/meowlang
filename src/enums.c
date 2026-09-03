
// Loading the file source and reading its contents

typedef struct {
    char *program;
    char *filePath;
} CodeSource;


// Tokenization stuff

enum TokenType {
    TOKEN_NULL,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_ASSIGN,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_LESS_THAN,
    TOKEN_GREATER_THAN,
    TOKEN_LESS_THAN_EQUAL,
    TOKEN_GREATER_THAN_EQUAL,
    TOKEN_KEYWORD,
    TOKEN_BRACKET_OPEN,
    TOKEN_BRACKET_CLOSE,
    TOKEN_NEWLINE,
    TOKEN_COMMA,
    TOKEN_PURR,
    TOKEN_MEOW,
    TOKEN_EOF,
    TOKEN_UNKNOWN
};

typedef struct {
    enum TokenType type;
    char *value;
    int startIndex;
    int endIndex;
} Token;

typedef struct {
    CodeSource *codeSource;
    int currentIndex;
    bool doneReading;
} Tokenizer;

typedef struct {
    Token *token_list;
    int current_pos;
} Parser;

enum PARSE_TREE_TYPE {
    AssignNode,
    BinaryOpNode,
    NumberNode,
    IdentifierNode,
    FunctionCallNode,
    FunctionDefinitionNode,
    ReturnNode,
    UndefinedNode
};

typedef struct ParseTreeNode {
    enum PARSE_TREE_TYPE type;
    union {
        struct {
            char *identifier;
            struct ParseTreeNode *value;
            struct ParseTreeNode *assignTo;
        } assignNode;
        struct {
            Token *operator;
            struct ParseTreeNode *left;
            struct ParseTreeNode *right;
        } binaryOpNode;
        struct {
            int value;
        } numberNode;
        struct {
            char *identifier;
        } identifierNode;
        struct {
            char *functionName;
            struct ParseTreeNode **arguments;
            int argumentCount;
        } functionCallNode;
        struct {
            char *functionName;
            struct ParseTreeNode **arguments;
            int argumentCount;
            struct ParseTreeNode **functionCode;
        } functionDefinitionNode;
        struct {
            struct ParseTreeNode *value;
        } returnNode;
        struct {
            int unknownValue; // Placeholder for unknown node type
        } undefinedNode;
    };
    int depth;
    struct ParseTreeNode *parent;
} ParseTreeNode;

typedef struct {
    bool inRegister;
    char *reg;

    bool hasSpillSlot;
    int spillOffset;
} ScratchRegister;