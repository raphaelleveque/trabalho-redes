#include "utils.h"

// Função para desabilitar o modo canônico do terminal
int disable_canonical_mode()
{
    struct termios terminal_settings;
    int result;

    result = tcgetattr(STDIN_FILENO, &terminal_settings);
    if (result < 0)
    {
        perror("Error in tcgetattr");
        return 0;
    }

    terminal_settings.c_lflag &= ~ICANON;
    result = tcsetattr(STDIN_FILENO, TCSANOW, &terminal_settings);
    if (result < 0)
    {
        perror("Error in tcsetattr");
        return 0;
    }

    return 1;
}

// Função para validar uma expressão e lidar com erros relacionados a sockets
int validate_expression(int expression, const char *error_message)
{
    if (expression == SOCKETERROR)
    {
        perror(error_message);
        exit(1);
    }
    else
    {
        return expression;
    }
}

// Função auxiliar para construir um token
char *_build_token(char **token, int token_size, char c)
{
    *token = realloc(*token, sizeof(char) * (token_size + 1));
    (*token)[token_size] = c;
    return *token;
}

// Função para obter tokens de uma string com base em um delimitador
char **get_tokens_from_string(char *string, const char delimiter)
{
    if (string == NULL)
        return NULL;

    char **tokens = calloc(BUFF_LEN / 2, sizeof(*tokens));
    int i = 0;
    int token_size = 0;

    while (*string != '\0')
    {
        if (*string == delimiter)
        {
            tokens[i] = _build_token(&tokens[i], token_size, '\0');
            token_size = 0;
            i++;
        }
        else
        {
            tokens[i] = _build_token(&tokens[i], token_size++, *string);
        }
        string++;
    }

    tokens[i] = _build_token(&tokens[i], token_size, '\0');
    return tokens;
}
