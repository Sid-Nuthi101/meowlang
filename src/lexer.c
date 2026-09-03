#include <ctype.h>
#include <string.h>
#include <stdbool.h>

CodeSource *createCodeSource(char *filePath) {
    FILE *f = fopen(filePath, "r");
    if (!f) { /* handle error */ return NULL; }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *program = malloc(len + 1);
    fread(program, 1, len, f);
    program[len] = '\0';
    fclose(f);

    CodeSource *codeSource = (CodeSource *)malloc(sizeof(CodeSource));
    codeSource->program = program;
    codeSource->filePath = filePath;
    return codeSource;
}

void printCodeSource(CodeSource *codeSource) {
    printf("Program: %s\n", codeSource->program);
    printf("File Path: %s\n", codeSource->filePath);
}

Tokenizer *createTokenizer(CodeSource *codeSource) {
    Tokenizer *tokenizer = (Tokenizer *)malloc(sizeof(Tokenizer));
    tokenizer->codeSource = codeSource;
    tokenizer->currentIndex = 0;
    tokenizer->doneReading = false;
    return tokenizer;
}

int isStopLineCharacter(char c) {
    return isspace(c) || c == '\0' || c == ',';
}

int isBracket(char c) {
    return c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']';
}

Token *nextToken(Tokenizer *tokenizer) {
    int currentIndex = tokenizer->currentIndex;
    char *program = tokenizer->codeSource->program;

    // Find nearest non-whitespace character
    while (isStopLineCharacter(program[currentIndex])) {
        if (program[currentIndex] == '\n') { // If the current character is a newline, return a newline token
            Token *token = (Token *)malloc(sizeof(Token));
            token->type = TOKEN_NEWLINE;
            token->value = "\n";
            tokenizer->currentIndex = currentIndex + 1;
            return token;
        } else if (program[currentIndex] == '\0') {
            Token *token = (Token *)malloc(sizeof(Token));
            token->type = TOKEN_EOF;
            token->value = "";
            tokenizer->doneReading = true; // Mark that we are done reading the program
            tokenizer->currentIndex = currentIndex + 1;
            return token;
        } else if(program[currentIndex] == ',') {
            Token *token = (Token *)malloc(sizeof(Token));
            token->type = TOKEN_COMMA;
            tokenizer->currentIndex = currentIndex + 1;
            token->value = ",";
            return token;
        }
        currentIndex++;
    }

    int startIndex = currentIndex;
    
    // Calculate length of token
    int tokenLength = 0;
    while (!isStopLineCharacter(program[currentIndex])) {
        tokenLength++;
        currentIndex++;
        // if the current character or the previous character is a bracket, break the loop
        if (isBracket(program[currentIndex]) || isBracket(program[currentIndex - 1])) {
            break;
        }
    }
    
    // Create a new token
    Token *token = (Token *)malloc(sizeof(Token));
    token->value = (char *)malloc(tokenLength + 1);

    // Copy the token value from the program string to length
    strncpy(token->value, program + startIndex, tokenLength);
    token->value[tokenLength] = '\0';

    if (isalpha(token->value[0])) {
        if (token->value[0] == ',') {
            token->type = TOKEN_COMMA;
        } else if (strcmp(token->value, "purr") == 0) {
            token->type = TOKEN_PURR;
        } else if (strcmp(token->value, "meow") == 0) {
            token->type = TOKEN_MEOW;
        } else{
            token->type = TOKEN_IDENTIFIER;
        }
    } else if (isdigit(token->value[0])) {
        token->type = TOKEN_NUMBER;
    } else if (token->value[0] == '"') {
        token->type = TOKEN_STRING;
    } else if (token->value[0] == '-') {
        token->type = TOKEN_MINUS;
    } else if (token->value[0] == '+') {
        token->type = TOKEN_PLUS;
    } else if (token->value[0] == '*') {
        token->type = TOKEN_MULTIPLY;
    } else if (token->value[0] == '/') {
        token->type = TOKEN_DIVIDE;
    } else if (token->value[0] == '=') {
        token->type = TOKEN_ASSIGN;
    } else if (token->value[0] == '&') {
        token->type = TOKEN_AND;
    } else if (token->value[0] == '|') {
        token->type = TOKEN_OR;
    } else if (token->value[0] == '!') {
        token->type = TOKEN_NOT;
    } else if (token->value[0] == '<' && token->value[1] == '=') {
        token->type = TOKEN_LESS_THAN_EQUAL;
    } else if (token->value[0] == '>' && token->value[1] == '=') {
        token->type = TOKEN_GREATER_THAN_EQUAL;
    } else if (token->value[0] == '<') {
        token->type = TOKEN_LESS_THAN;
    } else if (token->value[0] == '>') {
        token->type = TOKEN_GREATER_THAN;
    } else if(isBracket(token->value[0])) {
        if (token->value[0] == '(' || token->value[0] == '[' || token->value[0] == '{') {
            token->type = TOKEN_BRACKET_OPEN;
        } else {
            token->type = TOKEN_BRACKET_CLOSE;
        }
    }
    else {
        token->type = TOKEN_UNKNOWN;
    }

    tokenizer->currentIndex = currentIndex;
    return token;
}

void printToken(Token *token) {
    if (token == NULL) {
        printf("NULL TOKEN");
    } else{
        printf("Token Type: %d, Token Value: %s\n", token->type, token->value);
    }
}