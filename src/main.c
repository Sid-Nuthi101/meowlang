#include<stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "helpers.c"
#include "lexer.c"
#include "parser.c"
#include "runner.c"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("ERROR: File source must be passed as first argument");
        return 1;
    }
    else if (argc > 2) {
        printf("ERROR: Only one source file can be passed as argument");
        return 1;
    }
    else {
        char *filename = argv[1];
        // Open the file source and read its contents
        FILE *file = fopen(filename, "r");
        if (file == NULL) {
            printf("ERROR: Could not open file %s\n", filename);
            return 1;
        }
        else {
            int token_capacity = 64;
            Token *token_list = calloc(token_capacity, sizeof(Token)); // zeroed so unused slots read as TOKEN_NULL (0)
            CodeSource *codeSource = createCodeSource(filename);
            if (codeSource == NULL) {
                printf("ERROR: Could not read file %s\n", filename);
                return 1;
            }
            else {
                
                // Load the file source and read its contents
                Tokenizer *tokenizer = createTokenizer(codeSource);
                int current_pos = 0;

                // Run the lexer to tokenize the program
                while (tokenizer->doneReading == false) {
                    Token *token = nextToken(tokenizer);
                    if (current_pos >= token_capacity) {
                        int oldCapacity = token_capacity;
                        token_capacity *= 2;
                        token_list = realloc(token_list, token_capacity * sizeof(Token));
                        memset(token_list + oldCapacity, 0, (token_capacity - oldCapacity) * sizeof(Token));
                    }
                    token_list[current_pos++] = *token;
                    if (token == NULL || token->type == TOKEN_UNKNOWN) {
                        printToken(token);
                        break;
                    }
                    printToken(token);
                }


                fprintf(stderr, "Tokenization complete.\n\n RUNNING PARSER: \n\n");
                // Run the parser to parse the token list
                ParseTreeNode **parse_tree = parse(token_list, 0);

                fprintf(stderr, "Parsing complete.\n\n PRINTING PARSE TREE: \n\n");
                for (int i = 0; i < 64; i++) {
                    if (parse_tree[i] == NULL) {
                        break;
                    }
                    printNode(parse_tree[i]);
                    fprintf(stdout, "\n\n");
                }
                fprintf(stdout, "\n\n RUNNING PROGRAM \n\n");
                run(parse_tree);
            }
        }
        return 0;
    }
}