#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#import "enums.c"

char *normalSpacing = "    ";

#define MAX_VARIABLES 64
#define NUM_SCRATCH_REGISTERS 7

typedef struct {
    char *name;
    int offset;
} Variable;

static Variable variables[MAX_VARIABLES];
static int variableCount = 0;

// Set per function by generateFunctionBody so ReturnNode can emit its own epilogue.
static int currentFrameSize = 0;
static int scratchSpillBaseOffset = 0;
static int savedLrOffset = 0; // `bl` clobbers x30, so it's saved/restored around every call

static char *scratchRegisters[NUM_SCRATCH_REGISTERS] = {
    "x9", "x10", "x11", "x12", "x13", "x14", "x15"
};
static bool scratchRegisterUsed[NUM_SCRATCH_REGISTERS] = { false };

char *allocateRegister(void) {
    for (int i = 0; i < NUM_SCRATCH_REGISTERS; i++) {
        if (!scratchRegisterUsed[i]) {
            scratchRegisterUsed[i] = true;
            return scratchRegisters[i];
        }
    }
    fprintf(stderr, "allocateRegister: out of scratch registers (expression too complex)\n");
    exit(1);
}

void freeUsedRegisters() {
    for (int i = 0; i < NUM_SCRATCH_REGISTERS; i++) {
        scratchRegisterUsed[i] = false;
    }
    return;
}


void freeRegister(char *right) {
    for (int i = 0; i < NUM_SCRATCH_REGISTERS; i++) {
        if (strcmp(right,scratchRegisters[i]) == 0) {
            scratchRegisterUsed[i] = false;
            return;
        }
    }
    fprintf(stderr, "allocateRegister: register does not exist\n");
    exit(1);
}

int getVariableOffset(char *name) {
    for (int i = 0; i < variableCount; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].offset;
        }
    }

    if (variableCount >= MAX_VARIABLES) {
        fprintf(stderr, "getVariableOffset: too many variables (max %d)\n", MAX_VARIABLES);
        exit(1);
    }

    int offset = variableCount * 8;
    variables[variableCount].name = name;
    variables[variableCount].offset = offset;
    variableCount++;
    return offset;
}

void collectVariables(ParseTreeNode *node) {
    if (node == NULL) {
        return;
    }

    switch (node->type) {
        case AssignNode:
            collectVariables(node->assignNode.assignTo);
            collectVariables(node->assignNode.value);
            break;

        case BinaryOpNode:
            collectVariables(node->binaryOpNode.left);
            collectVariables(node->binaryOpNode.right);
            break;

        case IdentifierNode:
            getVariableOffset(node->identifierNode.identifier);
            break;

        case FunctionCallNode:
            for (int i = 0; i < node->functionCallNode.argumentCount; i++) {
                collectVariables(node->functionCallNode.arguments[i]);
            }
            break;

        case ReturnNode:
            collectVariables(node->returnNode.value);
            break;

        default:
            break;
    }
}

char *arithmeticInstruction(Token *operator) {
    switch (operator->type) {
        case TOKEN_PLUS: return "add";
        case TOKEN_MINUS: return "sub";
        case TOKEN_MULTIPLY: return "mul";
        case TOKEN_DIVIDE: return "sdiv";
        default:
            fprintf(stderr, "arithmeticInstruction: unsupported operator %s\n", operator->value);
            exit(1);
    }
}

char *comparisonCondition(Token *operator) {
    switch (operator->type) {
        case TOKEN_GREATER_THAN: return "gt";
        case TOKEN_LESS_THAN: return "lt";
        case TOKEN_GREATER_THAN_EQUAL: return "ge";
        case TOKEN_LESS_THAN_EQUAL: return "le";
        default:
            fprintf(stderr, "comparisonCondition: not a comparison operator\n");
            exit(1);
    }
}

bool isComparison(Token *operator) {
    return operator->type == TOKEN_GREATER_THAN || operator->type == TOKEN_LESS_THAN ||
           operator->type == TOKEN_GREATER_THAN_EQUAL || operator->type == TOKEN_LESS_THAN_EQUAL;
}

bool isSimpleOp(ParseTreeNode *node) {
    return node->type == IdentifierNode || node->type == NumberNode;
}

// Returns the scratch register holding currentNode's result; caller frees it.
char *generateProgram(FILE *out, ParseTreeNode *currentNode, ParseTreeNode *parentNode) {
    if (currentNode == NULL) {
        fprintf(stderr, "generateProgram: NULL node under parent type %d\n",
                parentNode == NULL ? -1 : parentNode->type);
        exit(1);
    }

    switch (currentNode->type) {
        case NumberNode: {
            char *reg = allocateRegister();
            fprintf(out, "%smov %s, #%d\n", normalSpacing, reg, currentNode->numberNode.value);
            return reg;
        }

        case IdentifierNode: {
            char *reg = allocateRegister();
            int offset = getVariableOffset(currentNode->identifierNode.identifier);
            fprintf(out, "%sldr %s, [sp, #%d]\n", normalSpacing, reg, offset);
            return reg;
        }

        case BinaryOpNode: {
            char *left;
            char *right;

            if (isSimpleOp(currentNode->binaryOpNode.left) && !isSimpleOp(currentNode->binaryOpNode.right)) {
                right = generateProgram(out, currentNode->binaryOpNode.right, currentNode);
                left = generateProgram(out, currentNode->binaryOpNode.left, currentNode);
            } else {
                left = generateProgram(out, currentNode->binaryOpNode.left, currentNode);
                right = generateProgram(out, currentNode->binaryOpNode.right, currentNode);
            }

            Token *operator = currentNode->binaryOpNode.operator;

            if (isComparison(operator)) {
                fprintf(out, "%scmp %s, %s\n", normalSpacing, left, right);
                fprintf(out, "%scset %s, %s\n", normalSpacing, left, comparisonCondition(operator));
            } else {
                fprintf(out, "%s%s %s, %s, %s\n", normalSpacing, arithmeticInstruction(operator), left, left, right);
            }

            freeRegister(right); // right's value has been folded into left
            return left;
        }

        case AssignNode: {
            char *value = generateProgram(out, currentNode->assignNode.value, currentNode);
            char *name = currentNode->assignNode.assignTo->identifierNode.identifier;
            int offset = getVariableOffset(name);
            fprintf(out, "%sstr %s, [sp, #%d]\n", normalSpacing, value, offset);
            return value;
        }

        case FunctionCallNode: {
            int argCount = currentNode->functionCallNode.argumentCount;
            if (argCount > 8) {
                fprintf(stderr,
                        "generateProgram: function calls with more than 8 arguments are not supported (call to %s)\n",
                        currentNode->functionCallNode.functionName);
                exit(1);
            }

            char *argRegs[8];
            for (int i = 0; i < argCount; i++) {
                argRegs[i] = generateProgram(out, currentNode->functionCallNode.arguments[i], currentNode);
            }

            for (int i = 0; i < argCount; i++) {
                fprintf(out, "%smov x%d, %s\n", normalSpacing, i, argRegs[i]);
                freeRegister(argRegs[i]);
            }

            // x9-x15 are caller-saved; spill any still holding a live value.
            bool spilled[NUM_SCRATCH_REGISTERS];
            for (int i = 0; i < NUM_SCRATCH_REGISTERS; i++) {
                spilled[i] = scratchRegisterUsed[i];
                if (spilled[i]) {
                    fprintf(out, "%sstr %s, [sp, #%d]\n", normalSpacing,
                            scratchRegisters[i], scratchSpillBaseOffset + i * 8);
                }
            }

            fprintf(out, "%sbl _meowfn_%s\n", normalSpacing, currentNode->functionCallNode.functionName);

            for (int i = 0; i < NUM_SCRATCH_REGISTERS; i++) {
                if (spilled[i]) {
                    fprintf(out, "%sldr %s, [sp, #%d]\n", normalSpacing,
                            scratchRegisters[i], scratchSpillBaseOffset + i * 8);
                }
            }

            char *resultReg = allocateRegister();
            fprintf(out, "%smov %s, x0\n", normalSpacing, resultReg);
            return resultReg;
        }

        case ReturnNode: {
            char *reg = generateProgram(out, currentNode->returnNode.value, currentNode);
            fprintf(out, "%smov x0, %s\n", normalSpacing, reg);
            fprintf(out, "%sldr x30, [sp, #%d]\n", normalSpacing, savedLrOffset);
            fprintf(out, "%sadd sp, sp, #%d\n", normalSpacing, currentFrameSize);
            fprintf(out, "%sret\n", normalSpacing);
            return reg;
        }

        default:
            fprintf(stderr, "generateProgram: unsupported node type %d\n", currentNode->type);
            exit(1);
    }
}

// requireReturn: true for a `purr` function (run() already verified its body
// ends in `meow`, whose codegen emits its own epilogue); false for main,
// which has no `meow` and instead exits with its last statement's value.
void generateFunctionBody(FILE *out, ParseTreeNode **statements, char **paramNames, int paramCount, bool requireReturn) {
    if (paramCount > 8) {
        fprintf(stderr, "generateFunctionBody: functions with more than 8 parameters are not supported\n");
        exit(1);
    }

    variableCount = 0;

    for (int i = 0; i < paramCount; i++) {
        getVariableOffset(paramNames[i]);
    }

    for (int i = 0; statements[i] != NULL; i++) {
        collectVariables(statements[i]);
    }

    int variablesSize = variableCount * 8;
    int scratchSpillBase = variablesSize;
    int lrOffset = scratchSpillBase + NUM_SCRATCH_REGISTERS * 8;
    int frameSize = lrOffset + 8; // slot for the incoming x30
    if (frameSize % 16 != 0) {
        frameSize += 8; // AArch64 requires sp to stay 16-byte aligned
    }

    currentFrameSize = frameSize;
    scratchSpillBaseOffset = scratchSpillBase;
    savedLrOffset = lrOffset;

    fprintf(out, "%ssub sp, sp, #%d\n", normalSpacing, frameSize);
    fprintf(out, "%sstr x30, [sp, #%d]\n", normalSpacing, savedLrOffset);

    for (int i = 0; i < paramCount; i++) {
        int offset = getVariableOffset(paramNames[i]);
        fprintf(out, "%sstr x%d, [sp, #%d]\n", normalSpacing, i, offset);
    }

    char *lastResult = NULL;
    freeUsedRegisters();

    for (int i = 0; statements[i] != NULL; i++) {
        lastResult = generateProgram(out, statements[i], NULL);
        if (statements[i + 1] != NULL) {
            freeUsedRegisters();
        }
    }

    if (requireReturn) {
        return;
    }

    if (lastResult != NULL) {
        fprintf(out, "%smov x0, %s\n", normalSpacing, lastResult);
    } else {
        fprintf(out, "%smov x0, #0\n", normalSpacing);
    }

    fprintf(out, "%sldr x30, [sp, #%d]\n", normalSpacing, savedLrOffset);
    fprintf(out, "%sadd sp, sp, #%d\n", normalSpacing, frameSize);
    fprintf(out, "%sret\n", normalSpacing);
}

void run(ParseTreeNode **treeNodes) {
    FILE *out = fopen("outputs/meowlang_output.s", "w");

    int totalCount = 0;
    for (int i = 0; treeNodes[i] != NULL; i++) {
        totalCount++;
    }

    ParseTreeNode **mainStatements = malloc(sizeof(ParseTreeNode *) * (totalCount + 1));
    int mainStatementCount = 0;

    for (int i = 0; i < totalCount; i++) {
        ParseTreeNode *node = treeNodes[i];

        if (node->type == FunctionDefinitionNode) {
            ParseTreeNode **body = node->functionDefinitionNode.functionCode;
            int bodyCount = 0;
            while (body[bodyCount] != NULL) {
                bodyCount++;
            }
            if (bodyCount == 0 || body[bodyCount - 1]->type != ReturnNode) {
                fprintf(stderr, "run: function '%s' must end with an explicit 'meow' return\n",
                        node->functionDefinitionNode.functionName);
                exit(1);
            }

            fprintf(out, ".align 2\n");
            fprintf(out, "_meowfn_%s:\n", node->functionDefinitionNode.functionName);

            int paramCount = node->functionDefinitionNode.argumentCount;
            char **paramNames = malloc(sizeof(char *) * (paramCount > 0 ? paramCount : 1));
            for (int p = 0; p < paramCount; p++) {
                paramNames[p] = node->functionDefinitionNode.arguments[p]->identifierNode.identifier;
            }

            generateFunctionBody(out, body, paramNames, paramCount, true);
            free(paramNames);
        } else {
            mainStatements[mainStatementCount++] = node;
        }
    }
    mainStatements[mainStatementCount] = NULL;

    fprintf(out, ".global _main\n");
    fprintf(out, ".align 2\n");
    fprintf(out, "_main:\n");

    generateFunctionBody(out, mainStatements, NULL, 0, false);

    free(mainStatements);
    fclose(out);

    int result = system(
        "clang outputs/meowlang_output.s -o outputs/hi"
    );

    if (result != 0) {
        fprintf(stderr, "Compilation failed\n");
        exit(1);
    }
}
