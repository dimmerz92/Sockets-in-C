#include <stdio.h>
#include <stdlib.h>

int main()
{
    char buffer[256];
    int word_count = 0;

    printf("Enter a word: ");
    fgets(buffer, 255, stdin);

    if (buffer[0] == '\n')
    {
        printf("ERROR\n");
        return 1;
    }

    for (int i = 0; i < 256; i++)
    {
        if (buffer[i] == ' ')
        {
            word_count++;
        }
        else if (buffer[i] == '\n')
        {
            word_count++;
            break;
        }
    }

    printf("Word count: %i\n", word_count);
    return 0;
}