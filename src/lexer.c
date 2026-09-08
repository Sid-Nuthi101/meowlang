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

    tokenizer->indentStack = (char **)malloc(sizeof(char *));
    tokenizer->indentStack[0] = strdup("");
    tokenizer->indentStackSize = 1;
    tokenizer->pendingDedentCount = 0;
    tokenizer->atLineStart = true;

    return tokenizer;
}

int isStopLineCharacter(char c) {
    return isspace(c) || c == '\0' || c == ',';
}

int isBracket(char c) {
    return c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']';
}

// Returns true if `prefix` is a strict (shorter, byte-for-byte) prefix of `s`.
bool isStrictPrefix(char *prefix, char *s) {
    int prefixLen = strlen(prefix);
    int sLen = strlen(s);
    if (prefixLen >= sLen) return false;
    return strncmp(prefix, s, prefixLen) == 0;
}

Token *makeToken(enum TokenType type, char *value) {
    Token *token = (Token *)malloc(sizeof(Token));
    token->type = type;
    token->value = value;
    return token;
}

// Takes ownership of whitespace.
void pushIndent(Tokenizer *tokenizer, char *whitespace) {
    tokenizer->indentStackSize++;
    tokenizer->indentStack = (char **)realloc(tokenizer->indentStack, tokenizer->indentStackSize * sizeof(char *));
    tokenizer->indentStack[tokenizer->indentStackSize - 1] = whitespace;
}

// Pops the indent stack down to size newSize, freeing the discarded levels
// and queuing one TOKEN_DEDENT per level popped.
void popIndentStackTo(Tokenizer *tokenizer, int newSize) {
    for (int i = newSize; i < tokenizer->indentStackSize; i++) {
        free(tokenizer->indentStack[i]);
    }
    tokenizer->pendingDedentCount = tokenizer->indentStackSize - newSize;
    tokenizer->indentStackSize = newSize;
}

// Measures this line's leading whitespace (skipping any blank lines first,
// which don't affect indentation) and compares it against the indent stack
// byte-for-byte - tabs and spaces are never treated as interchangeable, so
// ambiguous mixing is rejected instead of guessed at. Pushes or pops the
// stack as needed and returns a TOKEN_INDENT if one was opened; a dedent
// instead queues into pendingDedentCount, since nextToken() can only
// return one token per call.
Token *updateIndentation(Tokenizer *tokenizer) {
    char *program = tokenizer->codeSource->program;
    int idx = tokenizer->currentIndex;
    int wsStart = idx;
    while (1) {
        wsStart = idx;
        while (program[idx] == ' ' || program[idx] == '\t') {
            idx++;
        }
        if (program[idx] != '\n') {
            break;
        }
        idx++;
    }

    if (program[idx] == '\0') {
        tokenizer->currentIndex = idx;
        return NULL;
    }

    int wsLen = idx - wsStart;
    char *top = tokenizer->indentStack[tokenizer->indentStackSize - 1];
    bool sameAsTop = (int)strlen(top) == wsLen && memcmp(top, program + wsStart, wsLen) == 0;
    tokenizer->currentIndex = idx;

    if (sameAsTop) {
        return NULL;
    }

    char *lineIndent = (char *)malloc(wsLen + 1);
    memcpy(lineIndent, program + wsStart, wsLen);
    lineIndent[wsLen] = '\0';

    if (isStrictPrefix(top, lineIndent)) {
        pushIndent(tokenizer, lineIndent);
        return makeToken(TOKEN_INDENT, "");
    }

    if (!isStrictPrefix(lineIndent, top)) {
        fprintf(stderr, "Lexer error: inconsistent use of tabs and spaces in indentation\n");
        exit(1);
    }

    int k = tokenizer->indentStackSize - 2;
    while (k >= 0 && strcmp(tokenizer->indentStack[k], lineIndent) != 0) {
        k--;
    }
    if (k < 0) {
        fprintf(stderr, "Lexer error: unindent does not match any outer indentation level\n");
        exit(1);
    }
    popIndentStackTo(tokenizer, k + 1);
    free(lineIndent);
    return NULL;
}

// Flushes any indentation left open at EOF and drains the resulting
// TOKEN_DEDENTs before finally returning TOKEN_EOF.
Token *emitEOFOrDedent(Tokenizer *tokenizer, int atIndex) {
    if (tokenizer->indentStackSize > 1) {
        popIndentStackTo(tokenizer, 1);
    }
    tokenizer->currentIndex = atIndex;

    if (tokenizer->pendingDedentCount > 0) {
        tokenizer->pendingDedentCount--;
        return makeToken(TOKEN_DEDENT, "");
    }

    tokenizer->doneReading = true;
    tokenizer->currentIndex = atIndex + 1;
    return makeToken(TOKEN_EOF, "");
}

Token *nextToken(Tokenizer *tokenizer) {
    // Drain any queued TOKEN_DEDENTs before TOKEN_INDENT, one per call.
    if (tokenizer->pendingDedentCount > 0) {
        tokenizer->pendingDedentCount--;
        return makeToken(TOKEN_DEDENT, "");
    }
    if (tokenizer->atLineStart) {
        tokenizer->atLineStart = false;
        Token *indentToken = updateIndentation(tokenizer);
        return indentToken != NULL ? indentToken : nextToken(tokenizer);
    }

    int currentIndex = tokenizer->currentIndex;
    char *program = tokenizer->codeSource->program;

    // Find nearest non-whitespace character
    while (isStopLineCharacter(program[currentIndex])) {
        if (program[currentIndex] == '\n') {
            Token *token = (Token *)malloc(sizeof(Token));
            token->type = TOKEN_NEWLINE;
            token->value = "\n";
            tokenizer->currentIndex = currentIndex + 1;
            tokenizer->atLineStart = true;
            return token;
        } else if (program[currentIndex] == '\0') {
            return emitEOFOrDedent(tokenizer, currentIndex);
        } else if(program[currentIndex] == ',') {
            Token *token = (Token *)malloc(sizeof(Token));
            token->type = TOKEN_COMMA;
            tokenizer->currentIndex = currentIndex + 1;
            token->value = ",";
            return token;
        }
        currentIndex++;
    }

    // A leading '"' starts a quoted string literal. Consume up to the matching
    // closing '"', ignoring the usual whitespace/comma/bracket stop rules, and
    // interpret the \n and \" escape sequences along the way.
    if (program[currentIndex] == '"') {
        int strStart = currentIndex + 1;
        int scanIndex = strStart;
        int unescapedLength = 0;

        // First pass: find the closing quote and compute the unescaped length.
        while (program[scanIndex] != '"') {
            if (program[scanIndex] == '\0') {
                fprintf(stderr, "Lexer error: unterminated string literal\n");
                exit(1);
            }
            if (program[scanIndex] == '\\' && (program[scanIndex + 1] == 'n' || program[scanIndex + 1] == '"')) {
                scanIndex += 2;
            } else {
                scanIndex += 1;
            }
            unescapedLength++;
        }
        int closingQuoteIndex = scanIndex;

        Token *token = (Token *)malloc(sizeof(Token));
        token->type = TOKEN_STRING;
        token->value = (char *)malloc(unescapedLength + 1);

        // Second pass: copy characters, resolving \n and \" escapes.
        int readIndex = strStart;
        int writeIndex = 0;
        while (readIndex < closingQuoteIndex) {
            if (program[readIndex] == '\\' && program[readIndex + 1] == 'n') {
                token->value[writeIndex++] = '\n';
                readIndex += 2;
            } else if (program[readIndex] == '\\' && program[readIndex + 1] == '"') {
                token->value[writeIndex++] = '"';
                readIndex += 2;
            } else {
                token->value[writeIndex++] = program[readIndex];
                readIndex += 1;
            }
        }
        token->value[writeIndex] = '\0';

        tokenizer->currentIndex = closingQuoteIndex + 1;
        return token;
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
    } else if (token->value[0] == ':') {
        token->type = TOKEN_COLON;
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