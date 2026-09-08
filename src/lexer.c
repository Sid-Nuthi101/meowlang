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

    // The indent stack always starts with the column-0 level "".
    tokenizer->indentStackCapacity = 16;
    tokenizer->indentStack = (char **)malloc(tokenizer->indentStackCapacity * sizeof(char *));
    tokenizer->indentStack[0] = strdup("");
    tokenizer->indentStackSize = 1;
    tokenizer->pendingDedentCount = 0;
    tokenizer->pendingIndent = false;
    tokenizer->atLineStart = true; // indentation matters at the very start of the file too

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

void pushIndent(Tokenizer *tokenizer, char *whitespace) {
    if (tokenizer->indentStackSize >= tokenizer->indentStackCapacity) {
        tokenizer->indentStackCapacity *= 2;
        tokenizer->indentStack = (char **)realloc(tokenizer->indentStack, tokenizer->indentStackCapacity * sizeof(char *));
    }
    tokenizer->indentStack[tokenizer->indentStackSize++] = whitespace; // takes ownership of whitespace
}

// Returns the index where the next non-blank line's leading whitespace
// begins, skipping over any whitespace-only lines along the way (blank
// lines don't affect indentation, matching Python).
int skipBlankLines(char *program, int idx) {
    while (1) {
        int lineStart = idx;
        while (program[idx] == ' ' || program[idx] == '\t') {
            idx++;
        }
        if (program[idx] == '\n') {
            idx++;
            continue;
        }
        return lineStart;
    }
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

// Called at the start of each logical line. Compares this line's leading
// whitespace against the indent stack byte-for-byte (never tab-expanded,
// so ambiguous tab/space mixing is rejected rather than guessed at) and
// queues TOKEN_INDENT/TOKEN_DEDENT accordingly.
void updateIndentation(Tokenizer *tokenizer) {
    char *program = tokenizer->codeSource->program;
    int wsStart = skipBlankLines(program, tokenizer->currentIndex);

    int idx = wsStart;
    while (program[idx] == ' ' || program[idx] == '\t') {
        idx++;
    }
    if (program[idx] == '\0') {
        tokenizer->currentIndex = idx;
        return;
    }

    int wsLen = idx - wsStart;
    char *top = tokenizer->indentStack[tokenizer->indentStackSize - 1];
    bool sameAsTop = (int)strlen(top) == wsLen && memcmp(top, program + wsStart, wsLen) == 0;

    if (!sameAsTop) {
        char *lineIndent = (char *)malloc(wsLen + 1);
        memcpy(lineIndent, program + wsStart, wsLen);
        lineIndent[wsLen] = '\0';

        if (isStrictPrefix(top, lineIndent)) {
            pushIndent(tokenizer, lineIndent); // stack now owns lineIndent
            tokenizer->pendingIndent = true;
        } else if (isStrictPrefix(lineIndent, top)) {
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
        } else {
            fprintf(stderr, "Lexer error: inconsistent use of tabs and spaces in indentation\n");
            exit(1);
        }
    }

    tokenizer->currentIndex = idx;
}

Token *makeToken(enum TokenType type, char *value) {
    Token *token = (Token *)malloc(sizeof(Token));
    token->type = type;
    token->value = value;
    return token;
}

// Flushes any indentation levels still open at EOF (so the parser never
// needs special "unclosed block at EOF" handling), then emits either the
// next queued TOKEN_DEDENT or, once the queue is empty, TOKEN_EOF.
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
    if (tokenizer->pendingIndent) {
        tokenizer->pendingIndent = false;
        return makeToken(TOKEN_INDENT, "");
    }
    // At the start of a logical line, measure indentation before scanning
    // the line's first real token.
    if (tokenizer->atLineStart) {
        tokenizer->atLineStart = false;
        updateIndentation(tokenizer);
        return nextToken(tokenizer);
    }

    int currentIndex = tokenizer->currentIndex;
    char *program = tokenizer->codeSource->program;

    // Find nearest non-whitespace character
    while (isStopLineCharacter(program[currentIndex])) {
        if (program[currentIndex] == '\n') { // If the current character is a newline, return a newline token
            Token *token = (Token *)malloc(sizeof(Token));
            token->type = TOKEN_NEWLINE;
            token->value = "\n";
            tokenizer->currentIndex = currentIndex + 1;
            tokenizer->atLineStart = true; // indentation of the next line matters
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