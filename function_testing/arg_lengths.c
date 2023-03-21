#include <stdio.h>
#include <stdlib.h>

int main()
{
    char buffer[256] = "one two three";
    int arg_lengths[3] = {0};
    int count = 0;

    printf("%s\n", buffer);

    for (int i = 0; i < 256; i++)
    {
        if (buffer[i] == '\0')
        {
            break;
        }
        if (buffer[i] == ' ')
        {
            count++;
            continue;
        }
            arg_lengths[count]++;
    }

    printf("arg one: %i, arg two: %i, arg three: %i\n", arg_lengths[0], arg_lengths[1], arg_lengths[2]);
    return 0;
}