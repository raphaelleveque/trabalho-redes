#include "utils.h"

int disable_canonical_mode()
{
    struct termios settings;
    int result;
    result = tcgetattr(STDIN_FILENO, &settings);
    if (result < 0)
    {
        perror("error in tcgetattr");
        return 0;
    }
    settings.c_lflag &= -ICANON;
    result = tcsetattr(STDIN_FILENO, TCSANOW, &settings);
    if (result < 0)
    {
        perror("error in tcsetattr");
        return 0;
    }
    return 1;
}


int validate_expression(int exp, const char *msg)
{
    if (exp == SOCKETERROR)
    {
        perror(msg);
        exit(1);
    } else {
        return exp;
    }
}


char *_build_token(char **t, int t_size, char c)
{
    *t = realloc(*t, sizeof(char) * (t_size + 1));
    (*t)[t_size] = c;
    return *t;
}

char **str_get_tokens_(char *str, const char d)
{
    if (str == NULL)
        return NULL;

    char **tokens = calloc(BUFF_LEN / 2, sizeof(*tokens));
    int i = 0;
    int token_size = 0;
    while (*str != '\0')
    {
        if (*str == d)
        {
            tokens[i] = _build_token(&tokens[i], token_size, '\0');
            token_size = 0;
            i++;
        }
        else
        {
            tokens[i] = _build_token(&tokens[i], token_size++, *str);
        }
        str++;
    }

    tokens[i] = _build_token(&tokens[i], token_size, '\0');
    return tokens;
}
