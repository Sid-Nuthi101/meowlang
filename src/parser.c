#import "enums.c"
#import "lexer.c"
#import "printer.c"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

ParseTreeNode **parse(Token *token_list, int depth);

int tokenLength(Token *tokens) {
    int length = 0;
    fprintf(stdout, "tokens: [");
    while (tokens[length].type != TOKEN_NULL) {
        fprintf(stdout, "%s, ", tokens[length].value);
        length++;
    }
    fprintf(stdout, "]\n");
    return length;
}

ParseTreeNode *toRoot(ParseTreeNode *currentRoot) {
    ParseTreeNode *root = currentRoot;
    while (root != NULL && root->parent != NULL) {
        root = root->parent;
    }
    return root;
}

bool isOperand(Token *token) {
    return (token->type == TOKEN_PLUS || token->type == TOKEN_MINUS ||
                    token->type == TOKEN_MULTIPLY || token->type == TOKEN_DIVIDE ||
                    token->type == TOKEN_GREATER_THAN || token->type == TOKEN_LESS_THAN ||
                    token->type == TOKEN_GREATER_THAN_EQUAL || token->type == TOKEN_LESS_THAN_EQUAL);
}

Token *popNextExpression(Parser *parser) { // pops the next expression for + - * /
    int capacity = 64;
    Token *expressionTokens = (Token *)calloc(capacity, sizeof(Token)); // zeroed so unused slots read as TOKEN_NULL (0)
    int currentDepth = 0;
    bool firstToken = true;
    int count = 0;
    while (
        parser->token_list[parser->current_pos].type != TOKEN_EOF &&
        parser->token_list[parser->current_pos].type != TOKEN_NULL
    ) {
        Token *token = &parser->token_list[parser->current_pos];
        if (count >= capacity) {
            int oldCapacity = capacity;
            capacity *= 2;
            expressionTokens = realloc(expressionTokens, capacity * sizeof(Token));
            memset(expressionTokens + oldCapacity, 0, (capacity - oldCapacity) * sizeof(Token));
        }
        expressionTokens[count++] = *token;
        parser->current_pos++;
        if (token->type == TOKEN_NUMBER) {
            if (firstToken) {
                return expressionTokens;
            }
        }
        if (token->type == TOKEN_IDENTIFIER) {
            // A lone identifier is a complete one-token expression, UNLESS it's
            // immediately followed by '(' (a function call) - then we must keep
            // consuming through the matching ')' via the bracket-depth tracking below.
            bool isCall = parser->token_list[parser->current_pos].type == TOKEN_BRACKET_OPEN;
            if (firstToken && !isCall) {
                return expressionTokens;
            }
        }
        if (token->type == TOKEN_BRACKET_OPEN) {
            currentDepth++;
        }
        if (token->type == TOKEN_BRACKET_CLOSE) {
            currentDepth--;
            if (currentDepth == 0)
                return expressionTokens;
        }
        firstToken = false;

    }
    return expressionTokens;
}

Token *popInnerExpression(Parser *parser) { // parses ()
    int capacity = 64;
    Token *expressionTokens = (Token *)calloc(capacity, sizeof(Token)); // zeroed so unused slots read as TOKEN_NULL (0)
    int currentDepth = 1;
    bool firstToken = true;
    int count = 0;
    parser->current_pos++; // eat the opening (
    while (
        parser->token_list[parser->current_pos].type != TOKEN_EOF &&
        parser->token_list[parser->current_pos].type != TOKEN_NULL
    ) {
        Token *token = &parser->token_list[parser->current_pos];
        parser->current_pos++;
        if (token->type == TOKEN_BRACKET_OPEN) {
            currentDepth++;
        }
        if (token->type == TOKEN_BRACKET_CLOSE) {
            currentDepth--;
            if (currentDepth == 0)
                return expressionTokens;
        }
        if (count >= capacity) {
            int oldCapacity = capacity;
            capacity *= 2;
            expressionTokens = realloc(expressionTokens, capacity * sizeof(Token));
            memset(expressionTokens + oldCapacity, 0, (capacity - oldCapacity) * sizeof(Token));
        }
        expressionTokens[count++] = *token;

    }
    return expressionTokens;
}

ParseTreeNode **popArguments(Parser *parser, int initialDepth) {
    Token *expressionTokens = popInnerExpression(parser);
    int cursor = 0; // popInnerExpression already strips the opening parenthesis
    int currentDepth = initialDepth + 1;
    int nestDepth = 0; // tracks brackets nested *within* the current argument (e.g. a nested call), so their commas aren't treated as argument separators

    int argsList_capacity = 64;
    ParseTreeNode **argsList = calloc(argsList_capacity, sizeof(ParseTreeNode *)); // zeroed so callers can rely on a NULL-terminated list
    int argsList_idx = 0;
    int argument_capacity = 64;
    Token *argument = (Token *)calloc(argument_capacity, sizeof(Token)); // zeroed so unused slots read as TOKEN_NULL (0)
    int argument_idx = 0;
    while (expressionTokens[cursor].type != TOKEN_NULL) {
        if (expressionTokens[cursor].type == TOKEN_COMMA && nestDepth == 0) {
            cursor++;
            ParseTreeNode *nextNode = *parse(argument, currentDepth);
            if (argsList_idx >= argsList_capacity) {
                int oldCapacity = argsList_capacity;
                argsList_capacity *= 2;
                argsList = realloc(argsList, argsList_capacity * sizeof(ParseTreeNode *));
                memset(argsList + oldCapacity, 0, (argsList_capacity - oldCapacity) * sizeof(ParseTreeNode *));
            }
            argsList[argsList_idx++] = nextNode;
            argument_capacity = 64;
            argument = (Token *)calloc(argument_capacity, sizeof(Token));
            argument_idx = 0;
            continue;
        }
        if (expressionTokens[cursor].type == TOKEN_BRACKET_OPEN) {
            currentDepth++;
            nestDepth++;
        }
        if (expressionTokens[cursor].type == TOKEN_BRACKET_CLOSE) {
            currentDepth--;
            nestDepth--;
        }
        if (argument_idx >= argument_capacity) {
            int oldCapacity = argument_capacity;
            argument_capacity *= 2;
            argument = realloc(argument, argument_capacity * sizeof(Token));
            memset(argument + oldCapacity, 0, (argument_capacity - oldCapacity) * sizeof(Token));
        }
        argument[argument_idx++] = expressionTokens[cursor++];
    }
    // The final argument has no trailing comma to trigger its flush above,
    // and popInnerExpression already consumed the call's closing ')' before
    // we ever see it here, so it must be flushed explicitly.
    if (argument_idx > 0) {
        ParseTreeNode *nextNode = *parse(argument, currentDepth);
        if (argsList_idx >= argsList_capacity) {
            int oldCapacity = argsList_capacity;
            argsList_capacity *= 2;
            argsList = realloc(argsList, argsList_capacity * sizeof(ParseTreeNode *));
            memset(argsList + oldCapacity, 0, (argsList_capacity - oldCapacity) * sizeof(ParseTreeNode *));
        }
        argsList[argsList_idx++] = nextNode;
    }
    return argsList;
}

void insertNode(
    ParseTreeNode **inputRoot,
    ParseTreeNode *nextNode,
    int depth
) {
    ParseTreeNode *useRoot = *inputRoot;

    if (useRoot == NULL) {
        *inputRoot = nextNode;
        return;
    }
    /**
     * Get back to the correct parenthesis/depth level.
     */
    while (
        useRoot->parent != NULL &&
        useRoot->depth > depth
    ) {
        useRoot = useRoot->parent;
    }

    if (useRoot->type != AssignNode &&
        useRoot->type != BinaryOpNode) {

        nextNode->binaryOpNode.left = useRoot;
        useRoot->parent = nextNode;

        if (nextNode->binaryOpNode.right != NULL) {
            nextNode->binaryOpNode.right->parent = nextNode;
        }

        *inputRoot = nextNode;
        return;
    }

    if (useRoot->type == AssignNode) { // Assign always root
        ParseTreeNode *oldValue =
            useRoot->assignNode.value;

        nextNode->binaryOpNode.left = oldValue;

        if (oldValue != NULL) {
            oldValue->parent = nextNode;
        }

        useRoot->assignNode.value = nextNode;
        nextNode->parent = useRoot;

        if (nextNode->binaryOpNode.right != NULL) {
            nextNode->binaryOpNode.right->parent = nextNode;
        }

        *inputRoot = nextNode;
        return;
    }

    /*
     * STEP 2:
     * If we're currently sitting on a binary operator,
     * determine whether the new operator binds tighter.
     */
    if (
        useRoot->type == BinaryOpNode &&
        nextNode->type == BinaryOpNode
    ) {
        int oldPrec = precedence(useRoot->binaryOpNode.operator);
        int newPrec = precedence(nextNode->binaryOpNode.operator);

        /*
         * New operator has higher precedence:
         * Only take over RHS
         */
        if (
            newPrec > oldPrec &&
            useRoot->depth == depth
        ) {
            ParseTreeNode *oldRight = useRoot->binaryOpNode.right;

            nextNode->binaryOpNode.left = oldRight;

            if (oldRight != NULL) {
                oldRight->parent = nextNode;
            }

            useRoot->binaryOpNode.right = nextNode;
            nextNode->parent = useRoot;

            if (nextNode->binaryOpNode.right != NULL) {
                nextNode->binaryOpNode.right->parent =
                    nextNode;
            }

            *inputRoot = nextNode;
            return;
        }
    }

    /*
     * STEP 3:
     * New operator definetly has equal/lower precedence (eg. + should be higher than *).
     */
    while (
        useRoot->parent != NULL &&
        useRoot->parent->type == BinaryOpNode &&
        useRoot->parent->depth == depth
    ) {
        int parentPrec = precedence(useRoot->parent->binaryOpNode.operator);
        int newPrec =precedence(nextNode->binaryOpNode.operator);

        /*
         * Parent binds less tightly, so don't cross it.
         */
        if (parentPrec < newPrec) {
            break;
        }

        useRoot = useRoot->parent;
    }

    /*
     * STEP 4:
     * Insert nextNode ABOVE useRoot.
     * New Operation should have LHS of everything necessary to be computed before it.
     */
    ParseTreeNode *oldParent = useRoot->parent;

    nextNode->binaryOpNode.left = useRoot;
    useRoot->parent = nextNode;

    /*
     * Reconnect the old parent downward.
     */
    if (oldParent != NULL) {
        if (oldParent->type == BinaryOpNode) {
            if (oldParent->binaryOpNode.left == useRoot) {
                oldParent->binaryOpNode.left = nextNode;
            }
            else if (
                oldParent->binaryOpNode.right == useRoot
            ) {
                oldParent->binaryOpNode.right = nextNode;
            }
        }
        else if (oldParent->type == AssignNode) {
            if (oldParent->assignNode.value == useRoot) {
                oldParent->assignNode.value = nextNode;
            }
        }
    }
    nextNode->parent = oldParent;
    *inputRoot = nextNode; // Make the pointer of inputRoot (what the user passed into this function) the next node
}

ParseTreeNode **parse(Token *token_list, int depth) {
    Parser *parser = (Parser *)malloc(sizeof(Parser));
    parser->token_list = token_list;
    parser->current_pos = 0;

    // Implement the parsing logic here
    int node_list_capacity = 64;
    ParseTreeNode **node_list = calloc(node_list_capacity, sizeof(ParseTreeNode *)); // zeroed so callers can rely on a NULL-terminated list
    int node_list_count = 0;
    while (parser->token_list[parser->current_pos].type != TOKEN_EOF && parser->token_list[parser->current_pos].type != TOKEN_NULL) {
        // Parse tokens and build the parse tree
        bool eof_reached = false;
        ParseTreeNode *currentRoot = NULL;
        // Line by line parse
        while(parser->token_list[parser->current_pos].type != TOKEN_NEWLINE 
            && parser->token_list[parser->current_pos].type != TOKEN_NULL) {
            Token *token = &parser->token_list[parser->current_pos];
            ParseTreeNode *nextNode = (ParseTreeNode *)malloc(sizeof(ParseTreeNode));
            nextNode->parent = NULL;
            nextNode->depth = depth;
            if (token->type == TOKEN_NUMBER) {
                nextNode->type = NumberNode;
                nextNode->numberNode.value = atoi(token->value);
                parser->current_pos++;
            } else if (token->type == TOKEN_IDENTIFIER) {
                // two choices here - either a variable or a function call
                Token *nextToken = &parser->token_list[parser->current_pos + 1];
                if (nextToken->type == TOKEN_BRACKET_OPEN) { //function call
                    nextNode->type = FunctionCallNode;
                    nextNode->functionCallNode.functionName = strdup(token->value);
                    parser->current_pos++; // skip past the function name onto '(' so popArguments/popInnerExpression eats the right token
                    ParseTreeNode **arg = popArguments(parser, depth);
                    nextNode->functionCallNode.arguments = arg;
                    int argCount = 0;
                    while (arg[argCount] != NULL) {
                        argCount++;
                    }
                    nextNode->functionCallNode.argumentCount = argCount;
                }
                else {
                    nextNode->type = IdentifierNode;
                    nextNode->identifierNode.identifier = strdup(token->value);
                    parser->current_pos++;
                }
            } else if (token->type == TOKEN_ASSIGN) {
                nextNode->type = AssignNode;
                nextNode->assignNode.identifier = "=";
                nextNode->assignNode.assignTo = currentRoot;
                currentRoot->parent = nextNode;

                parser->current_pos++; // Skip past '='
                // Calculate the RHS node
                Token *rightTokens = popNextExpression(parser);
                nextNode->assignNode.value = parse(rightTokens, depth)[0];
                nextNode->assignNode.value->parent = nextNode;
            } else if (isOperand(token)) { // Operators +-<=>*/
                nextNode->type = BinaryOpNode;
                nextNode->binaryOpNode.operator = token;
                
                parser->current_pos++; // Skip past operator
                // Calculate the RHS node
                Token *rightTokens = popNextExpression(parser);
                ParseTreeNode *newRight = parse(rightTokens, depth)[0];
                nextNode->binaryOpNode.right = newRight;
            } else if (token->type == TOKEN_BRACKET_OPEN) {
                // Handle function calls or expressions in parentheses
                Token *rightTokens = popInnerExpression(parser);
                nextNode = parse(rightTokens, depth + 1)[0];
                nextNode->depth = depth + 1;
            }  else if (token->type == TOKEN_COMMA) {
                parser->current_pos++;
                continue;
            } else if (token->type == TOKEN_PURR) {
                nextNode->type = FunctionDefinitionNode;
                parser->current_pos++; // skip past 'purr' onto the function name identifier
                Token *nameToken = &parser->token_list[parser->current_pos];
                nextNode->functionDefinitionNode.functionName = strdup(nameToken->value);
                parser->current_pos++; // skip past the function name onto '(' so popArguments/popInnerExpression eats the right token
                ParseTreeNode **arg = popArguments(parser, depth);
                nextNode->functionDefinitionNode.arguments = arg;
                int argCount = 0;
                while (arg[argCount] != NULL) {
                    argCount++;
                }
                nextNode->functionDefinitionNode.argumentCount = argCount;
                // current_pos now sits on the opening '{' of the function body
                Token *bodyTokens = popInnerExpression(parser);
                nextNode->functionDefinitionNode.functionCode = parse(bodyTokens, depth + 1);
            } else if (token->type == TOKEN_MEOW) {
                parser->current_pos++; // skip past 'meow' onto the return expression
                Token *exprTokens = popNextExpression(parser);
                nextNode->type = ReturnNode;
                nextNode->returnNode.value = parse(exprTokens, depth)[0];
            }
            else if (token->type == TOKEN_EOF) {
                eof_reached = true;
                break;
            } else {
                fprintf(stderr, "ERROR: Unexpected token type %u, %s\n", token->type, token->value);
                break;
            }

            if (nextNode->type == BinaryOpNode) {
                insertNode(&currentRoot, nextNode, depth);
            }
            else if (nextNode->type == AssignNode) {
                currentRoot = nextNode;
            }
            else if (currentRoot == NULL) {
                currentRoot = nextNode;
            }
        }
        parser->current_pos++;
        if (currentRoot != NULL){
            ParseTreeNode *root = toRoot(currentRoot);
            if (node_list_count >= node_list_capacity) {
                int oldCapacity = node_list_capacity;
                node_list_capacity *= 2;
                node_list = realloc(node_list, node_list_capacity * sizeof(ParseTreeNode *));
                memset(node_list + oldCapacity, 0, (node_list_capacity - oldCapacity) * sizeof(ParseTreeNode *));
            }
            node_list[node_list_count++] = root;
        }

        

        if (eof_reached) {
            break;
        }
    }
    return node_list;
}

