#ifndef LEXER_H
#define LEXER_H


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


typedef enum {
    // Constans and Literals
    TOKEN_INT_LITERAL,      // 10, 450, 1000 etc.
    TOKEN_FLOAT_LITERAL,    // 20.5, 3.14, 0.001 etc.
    TOKEN_STRING_LITERAL,   // "Hello", "World" etc.
    TOKEN_IDENTIFIER,  // x, number, sum, result etc.
    
    // Keywords
    TOKEN_INT,         // int
    TOKEN_FLOAT,       // float
    TOKEN_STRING,      // string
    TOKEN_BOOL,        // bool
    TOKEN_VOID,        // void
    TOKEN_LIST,        // list
    TOKEN_TRUE,        // true
    TOKEN_FALSE,       // false
    TOKEN_IF,          // if
    TOKEN_ELSE,        // else
    TOKEN_WHILE,       // while
    TOKEN_RETURN,      // return

    // Operators & Sembols
    TOKEN_PLUS,        // +
    TOKEN_MINUS,       // -
    TOKEN_STAR,        // *
    TOKEN_SLASH,       // /
    TOKEN_PERCENT,     // %
    TOKEN_ASSIGN,      // =
    TOKEN_SEMICOLON,   // ;
    TOKEN_COMMA,       // ,
    TOKEN_LPAREN,      // (
    TOKEN_RPAREN,      // )
    TOKEN_LBRACE,      // {
    TOKEN_RBRACE,      // }
    TOKEN_LBRACKET,    // [
    TOKEN_RBRACKET,    // ]
    TOKEN_DOT,         // .
    TOKEN_GT,          // >
    TOKEN_LT,          // <
    TOKEN_GTE,         // >=
    TOKEN_LTE,         // <=
    TOKEN_EQ,          // ==
    TOKEN_BANG_EQ,     // !=
    TOKEN_BANG,        // !
    TOKEN_ANDAND,      // &&
    TOKEN_OROR,        // ||

    // Special Tokens
    TOKEN_EOF,        
    TOKEN_ERROR        
} TokenType;


// Token Struct
typedef struct {
    TokenType type;
    char* value;    // text representation of the token
    int line;       // Line number for error handling
} Token;

// The Lexer Struct that holds the state of the lexer
typedef struct {
    const char* source; // source code to be tokenized
    int cursor;         // Current position in the source code
    int line;           // Current line number
} Lexer;


Lexer create_lexer(const char* source);
char peek_char(Lexer* lexer);
char advance_char(Lexer* lexer);
Token make_token(TokenType type, const char* text, int line);
TokenType check_keyword(const char* text);
Token next_token(Lexer* lexer);

void print_token(Token token);
const char* token_type_to_string(TokenType type);
void free_token(Token* token);

#endif
