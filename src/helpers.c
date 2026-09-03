#import "enums.c"

void intToStr(int N, char *str) {
    int i = 0;
  
    // Save the copy of the number for sign
    int sign = N;

    // If the number is negative, make it positive
    if (N < 0)
        N = -N;

    // Extract digits from the number and add them to the
    // string
    while (N > 0) {
      
        // Convert integer digit to character and store
      	// it in the str
        str[i++] = N % 10 + '0';
      	N /= 10;
    } 

    // If the number was negative, add a minus sign to the
    // string
    if (sign < 0) {
        str[i++] = '-';
    }

    // Null-terminate the string
    str[i] = '\0';

    // Reverse the string to get the correct order
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

int precedence(Token *operator) {
    switch (operator->type) {
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
            return 2;

        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 1;

        case TOKEN_GREATER_THAN:
        case TOKEN_LESS_THAN:
        case TOKEN_GREATER_THAN_EQUAL:
        case TOKEN_LESS_THAN_EQUAL:
            return 0;

        default:
            return -1;
    }
}