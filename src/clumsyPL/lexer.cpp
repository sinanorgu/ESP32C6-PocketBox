#include "clumsyPL/lexer.hpp"

Lexer create_lexer(const char* source) {
    Lexer lexer;
    lexer.source = source;
    lexer.cursor = 0;
    lexer.line = 1;
    return lexer;
}

char peek_char(Lexer* lexer) {
    return lexer->source[lexer->cursor];
}

char advance_char(Lexer* lexer) {
    char c = lexer->source[lexer->cursor];
    lexer->cursor++;
    return c;
}

Token make_token(TokenType type, const char* text, int line) {
    Token t;
    t.type = type;
    t.value = strdup(text); 
    t.line = line;
    return t;
}

// Keyword checking function
TokenType check_keyword(const char* text) {
    if (strcmp(text, "int") == 0) return TOKEN_INT;
    if (strcmp(text, "float") == 0) return TOKEN_FLOAT;
    if (strcmp(text, "bool") == 0) return TOKEN_BOOL;
    if (strcmp(text, "void") == 0) return TOKEN_VOID;
    if (strcmp(text, "list") == 0) return TOKEN_LIST;
    if (strcmp(text, "true") == 0) return TOKEN_TRUE;
    if (strcmp(text, "false") == 0) return TOKEN_FALSE;
    if (strcmp(text, "if") == 0) return TOKEN_IF;
    if (strcmp(text, "else") == 0) return TOKEN_ELSE;
    if (strcmp(text, "while") == 0) return TOKEN_WHILE;
    if (strcmp(text, "return") == 0) return TOKEN_RETURN;
    if (strcmp(text, "string") == 0) return TOKEN_STRING;
    
    // If it's not a known keyword, it's a variable or function name!
    return TOKEN_IDENTIFIER; 
}

Token next_token(Lexer* lexer) {

    for (;;) {
        char c = peek_char(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance_char(lexer);
        } else if (c == '\n') {
            lexer->line++;
            advance_char(lexer);
        } else if (c == '/' && lexer->source[lexer->cursor + 1] == '/') {
            while (peek_char(lexer) != '\n' && peek_char(lexer) != '\0') advance_char(lexer);
        } else if (c == '/' && lexer->source[lexer->cursor + 1] == '*') {
            int start_line = lexer->line;
            advance_char(lexer);
            advance_char(lexer);
            while (peek_char(lexer) != '\0' &&
                   !(peek_char(lexer) == '*' && lexer->source[lexer->cursor + 1] == '/')) {
                if (advance_char(lexer) == '\n') lexer->line++;
            }
            if (peek_char(lexer) == '\0') return make_token(TOKEN_ERROR, "Unterminated block comment", start_line);
            advance_char(lexer);
            advance_char(lexer);
        } else {
            break;
        }
    }

    // end of source check
    if (peek_char(lexer) == '\0') {
        return make_token(TOKEN_EOF, "EOF", lexer->line);
    }

    char c = advance_char(lexer);

    // single character tokens
    switch (c) {
        case '+': return make_token(TOKEN_PLUS, "+", lexer->line);
        case '-': return make_token(TOKEN_MINUS, "-", lexer->line);
        case '*': return make_token(TOKEN_STAR, "*", lexer->line);
        case '/': return make_token(TOKEN_SLASH, "/", lexer->line);
        case '%': return make_token(TOKEN_PERCENT, "%", lexer->line);
        case '(': return make_token(TOKEN_LPAREN, "(", lexer->line);
        case ')': return make_token(TOKEN_RPAREN, ")", lexer->line);
        case '{': return make_token(TOKEN_LBRACE, "{", lexer->line);
        case '}': return make_token(TOKEN_RBRACE, "}", lexer->line);
        case '[': return make_token(TOKEN_LBRACKET, "[", lexer->line);
        case ']': return make_token(TOKEN_RBRACKET, "]", lexer->line);
        case '.': return make_token(TOKEN_DOT, ".", lexer->line);
        case ';': return make_token(TOKEN_SEMICOLON, ";", lexer->line);
        case ',': return make_token(TOKEN_COMMA, ",", lexer->line);
        case '>': 
            if (peek_char(lexer) == '=') {
                advance_char(lexer);
                return make_token(TOKEN_GTE, ">=", lexer->line);
            } else {
                return make_token(TOKEN_GT, ">", lexer->line);
            }

        case '<':
            if (peek_char(lexer) == '=') {
                advance_char(lexer);
                return make_token(TOKEN_LTE, "<=", lexer->line);
            } else {
                return make_token(TOKEN_LT, "<", lexer->line);
            }

        case '!':
            if (peek_char(lexer) == '=') {
                advance_char(lexer);
                return make_token(TOKEN_BANG_EQ, "!=", lexer->line);
            } else {
                return make_token(TOKEN_BANG, "!", lexer->line);
            }
            
        case '=':
            if (peek_char(lexer) == '=') {
                advance_char(lexer);
                return make_token(TOKEN_EQ, "==", lexer->line);
            } else {
                return make_token(TOKEN_ASSIGN, "=", lexer->line);
            }
        
        case '&':
            if (peek_char(lexer) == '&') {
                advance_char(lexer);
                return make_token(TOKEN_ANDAND, "&&", lexer->line);
            } else {
                return make_token(TOKEN_ERROR, "Unexpected character: &", lexer->line);
            }
        case '|':
            if (peek_char(lexer) == '|') {
                advance_char(lexer);
                return make_token(TOKEN_OROR, "||", lexer->line);
            } else {
                return make_token(TOKEN_ERROR, "Unexpected character: |", lexer->line);
            }
        
    }

    // The words starting with a letter or underscore are either keywords or identifiers
    if (isalpha(c) || c == '_') {
        int start = lexer->cursor - 1;

        // iterate through the identifier, allowing for letters, digits, and underscores
        while (isalnum(peek_char(lexer)) || peek_char(lexer) == '_') {
            advance_char(lexer);
        }

        int length = lexer->cursor - start;
        char* text = (char*)malloc(length + 1);
        strncpy(text, &lexer->source[start], length);
        text[length] = '\0';

        // Check if the identifier is a keyword or just a regular identifier
        TokenType type = check_keyword(text);

        Token t = make_token(type, text, lexer->line);
        free(text);
        return t;
    }

    // multi character numbers
    if (isdigit(c)) {
        bool is_float = false;
        int start = lexer->cursor - 1; // where the number starts
        
        // iterate through the number, allowing for one decimal point
        while (isdigit(peek_char(lexer)) || peek_char(lexer) == '.') {
            if (peek_char(lexer) == '.') {
                if(!is_float) {
                    is_float = true;
                } else {
                    return make_token(TOKEN_ERROR, "Invalid number format", lexer->line);
                }
            }
            advance_char(lexer);
        }

        int length = lexer->cursor - start;
        
        
        // Sayı string'ini ayır ve token yap
        char* num_str = (char*)malloc(length + 1);
        strncpy(num_str, &lexer->source[start], length);
        num_str[length] = '\0';
        Token t;

        if(is_float){
            t = make_token(TOKEN_FLOAT_LITERAL, num_str, lexer->line);
        }
        else{    
            t = make_token(TOKEN_INT_LITERAL, num_str, lexer->line);
        }
        
        free(num_str); 
        return t;
        
    }

    //string literals
    if (c == '"') {
        int start = lexer->cursor; // where the string starts
        while (peek_char(lexer) != '"' && peek_char(lexer) != '\0') {
            advance_char(lexer);
        }

        if (peek_char(lexer) == '\0') {
            return make_token(TOKEN_ERROR, "Unterminated string literal", lexer->line);
        }

        // Skip the closing quote
        advance_char(lexer);

        int length = lexer->cursor - start - 1; // exclude the quotes
        char* str_value = (char*)malloc(length + 1);
        strncpy(str_value, &lexer->source[start], length);
        str_value[length] = '\0';

        Token t = make_token(TOKEN_STRING_LITERAL, str_value, lexer->line);
        free(str_value); 
        return t;
    }
    
    // Unknown character
    char err_str[2] = {c, '\0'};
    return make_token(TOKEN_ERROR, err_str, lexer->line);

}




void print_token(Token token) {
    printf("Token Type: %s, Value: %s, Line: %d\n", token_type_to_string(token.type), token.value, token.line);
}

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_INT_LITERAL: return "TOKEN_INT_LITERAL";
        case TOKEN_FLOAT_LITERAL: return "TOKEN_FLOAT_LITERAL";
        case TOKEN_STRING_LITERAL: return "TOKEN_STRING_LITERAL";
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";   
        case TOKEN_INT: return "TOKEN_INT";
        case TOKEN_FLOAT: return "TOKEN_FLOAT";
        case TOKEN_STRING: return "TOKEN_STRING";
        case TOKEN_BOOL: return "TOKEN_BOOL";
        case TOKEN_VOID: return "TOKEN_VOID";
        case TOKEN_LIST: return "TOKEN_LIST";
        case TOKEN_TRUE: return "TOKEN_TRUE";
        case TOKEN_FALSE: return "TOKEN_FALSE";
        case TOKEN_IF: return "TOKEN_IF";
        case TOKEN_ELSE: return "TOKEN_ELSE";
        case TOKEN_WHILE: return "TOKEN_WHILE";
        case TOKEN_RETURN: return "TOKEN_RETURN";
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_STAR: return "TOKEN_STAR";
        case TOKEN_SLASH: return "TOKEN_SLASH";
        case TOKEN_PERCENT: return "TOKEN_PERCENT";
        case TOKEN_ASSIGN: return "TOKEN_ASSIGN";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_ANDAND: return "TOKEN_ANDAND";
        case TOKEN_OROR: return "TOKEN_OROR";
        case TOKEN_LPAREN: return "TOKEN_LPAREN";
        case TOKEN_RPAREN: return "TOKEN_RPAREN";
        case TOKEN_LBRACE: return "TOKEN_LBRACE";
        case TOKEN_RBRACE: return "TOKEN_RBRACE";
        case TOKEN_LBRACKET: return "TOKEN_LBRACKET";
        case TOKEN_RBRACKET: return "TOKEN_RBRACKET";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_ERROR: return "TOKEN_ERROR";
        case TOKEN_GT: return "TOKEN_GT";
        case TOKEN_LT: return "TOKEN_LT";
        case TOKEN_GTE: return "TOKEN_GTE";
        case TOKEN_LTE: return "TOKEN_LTE";
        case TOKEN_EQ: return "TOKEN_EQ";
        case TOKEN_BANG_EQ: return "TOKEN_BANG_EQ";
        case TOKEN_BANG: return "TOKEN_BANG";
        default: return "UNKNOWN_TOKEN";
    }
}


void free_token(Token* token)
{
    if (token == NULL) {
        return;
    }

    free(token->value);
    token->value = NULL;
}
