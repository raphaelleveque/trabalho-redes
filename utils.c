#include "utils.h"

int clear_icanon()
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

void str_trim_lf (char *arr){
    for(int i = 0; i < strlen(arr); i++){
        if(arr[i == '\n']){
            arr[i] = '\0';
            //arr[i] = '\0';
            break;
        }
    }
}

int check(int exp, const char *msg)
{
    if (exp == SOCKETERROR)
    {
        perror(msg);
        exit(1);
    }
    return exp;
}

void str_overwrite_stdout()
{
    printf("\r%s", "> ");
    fflush(stdout);
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

void str_free_tokens(char **tokens)
{
    for (int i = 0; tokens[i]; i++)
        free(tokens[i]);
    free(tokens);
}
