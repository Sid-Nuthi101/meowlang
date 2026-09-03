#import "enums.c"

void printTree(ParseTreeNode *node, const char *prefix, bool isLast);
void printNode(ParseTreeNode *node);

void printNodeValue(ParseTreeNode *node) {
    if (node == NULL) {
        printf("NULL");
        return;
    }
    switch (node->type) {
        case AssignNode:
            printf("ASSIGN %s", node->assignNode.identifier);
            break;

        case BinaryOpNode:
            printf("%s", node->binaryOpNode.operator->value);
            break;

        case NumberNode:
            printf("%d", node->numberNode.value);
            break;

        case IdentifierNode:
            printf("%s", node->identifierNode.identifier);
            break;

        case FunctionCallNode:
            printf("CALL %s", node->functionCallNode.functionName);
            break;

        case FunctionDefinitionNode:
            printf("DEF %s", node->functionDefinitionNode.functionName);
            break;

        case ReturnNode:
            printf("MEOW");
            break;

        default:
            printf("???");
            break;
    }
}

void printParent(ParseTreeNode *node) {
    printf(" parent=");

    if (node == NULL || node->parent == NULL) {
        printf("NULL");
        return;
    }

    printNodeValue(node->parent);
}

void printTree(ParseTreeNode *node, const char *prefix, bool isLast) {
    printf("%s", prefix);
    printf("%s", isLast ? "└── " : "├── ");

    if (node == NULL) {
        printf("NULL\n");
        return;
    }

    printNodeValue(node);

    printf("  [depth=%d,", node->depth);
    printParent(node);
    printf("]\n");

    char childPrefix[1024];

    snprintf(
        childPrefix,
        sizeof(childPrefix),
        "%s%s",
        prefix,
        isLast ? "    " : "│   "
    );

    switch (node->type) {
        case AssignNode:
            printf("%s├── target\n", childPrefix);

            char targetPrefix[1024];
            snprintf(
                targetPrefix,
                sizeof(targetPrefix),
                "%s│   ",
                childPrefix
            );

            printTree(
                node->assignNode.assignTo,
                targetPrefix,
                true
            );

            printf("%s└── value\n", childPrefix);

            char valuePrefix[1024];
            snprintf(
                valuePrefix,
                sizeof(valuePrefix),
                "%s    ",
                childPrefix
            );

            printTree(
                node->assignNode.value,
                valuePrefix,
                true
            );

            break;

        case BinaryOpNode:
            printTree(
                node->binaryOpNode.left,
                childPrefix,
                false
            );

            printTree(
                node->binaryOpNode.right,
                childPrefix,
                true
            );

            break;

        case FunctionCallNode:
            if (node->functionCallNode.argumentCount > 0) {
                printf("%s└── arguments\n", childPrefix);

                char argPrefix[1024];
                snprintf(
                    argPrefix,
                    sizeof(argPrefix),
                    "%s    ",
                    childPrefix
                );

                for (int i = 0;
                     i < node->functionCallNode.argumentCount;
                     i++) {

                    printTree(
                        node->functionCallNode.arguments[i],
                        argPrefix,
                        i == node->functionCallNode.argumentCount - 1
                    );
                }
            }

            break;

        case FunctionDefinitionNode: {
            int bodyCount = 0;
            while (node->functionDefinitionNode.functionCode[bodyCount] != NULL) {
                bodyCount++;
            }

            bool hasArguments = node->functionDefinitionNode.argumentCount > 0;
            bool hasBody = bodyCount > 0;

            if (hasArguments) {
                printf("%s%s arguments\n", childPrefix, hasBody ? "├──" : "└──");

                char argPrefix[1024];
                snprintf(
                    argPrefix,
                    sizeof(argPrefix),
                    "%s%s",
                    childPrefix,
                    hasBody ? "│   " : "    "
                );

                for (int i = 0;
                     i < node->functionDefinitionNode.argumentCount;
                     i++) {

                    printTree(
                        node->functionDefinitionNode.arguments[i],
                        argPrefix,
                        i == node->functionDefinitionNode.argumentCount - 1
                    );
                }
            }

            if (hasBody) {
                printf("%s└── body\n", childPrefix);

                char bodyPrefix[1024];
                snprintf(
                    bodyPrefix,
                    sizeof(bodyPrefix),
                    "%s    ",
                    childPrefix
                );

                for (int i = 0; i < bodyCount; i++) {
                    printTree(
                        node->functionDefinitionNode.functionCode[i],
                        bodyPrefix,
                        i == bodyCount - 1
                    );
                }
            }

            break;
        }

        case ReturnNode: {
            printf("%s└── value\n", childPrefix);

            char valuePrefix[1024];
            snprintf(
                valuePrefix,
                sizeof(valuePrefix),
                "%s    ",
                childPrefix
            );

            printTree(
                node->returnNode.value,
                valuePrefix,
                true
            );

            break;
        }

        default:
            break;
    }
}

void printNode(ParseTreeNode *node) {
    if (node == NULL) {
        printf("NULL\n");
        return;
    }

    // Print root without └──
    printNodeValue(node);
    printf("  [depth=%d,", node->depth);
    printParent(node);
    printf("]\n");

    char prefix[1] = "";

    switch (node->type) {
        case AssignNode:
            printf("├── target\n");
            printTree(node->assignNode.assignTo, "│   ", true);

            printf("└── value\n");
            printTree(node->assignNode.value, "    ", true);
            break;

        case BinaryOpNode:
            printTree(node->binaryOpNode.left, "", false);
            printTree(node->binaryOpNode.right, "", true);
            break;

        case FunctionCallNode:
            for (int i = 0;
                 i < node->functionCallNode.argumentCount;
                 i++) {

                printTree(
                    node->functionCallNode.arguments[i],
                    "",
                    i == node->functionCallNode.argumentCount - 1
                );
            }
            break;

        case FunctionDefinitionNode: {
            int bodyCount = 0;
            while (node->functionDefinitionNode.functionCode[bodyCount] != NULL) {
                bodyCount++;
            }

            for (int i = 0;
                 i < node->functionDefinitionNode.argumentCount;
                 i++) {

                printTree(
                    node->functionDefinitionNode.arguments[i],
                    "",
                    bodyCount == 0 &&
                        i == node->functionDefinitionNode.argumentCount - 1
                );
            }

            for (int i = 0; i < bodyCount; i++) {
                printTree(
                    node->functionDefinitionNode.functionCode[i],
                    "",
                    i == bodyCount - 1
                );
            }

            break;
        }

        case ReturnNode:
            printTree(node->returnNode.value, "", true);
            break;

        default:
            break;
    }
}