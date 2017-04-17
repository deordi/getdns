/* 
 *
 *example_yaml_to_json
 */
 
#include "convert_yaml_to_json.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
int
main(int argc, char *argv[])
{
    int help = 0;
    int canonical = 0;
    int unicode = 0;
    int k;

    /* Analyze command line options. */

    for (k = 1; k < argc; k ++)
    {
        if (strcmp(argv[k], "-h") == 0
                || strcmp(argv[k], "--help") == 0) {
            help = 1;
        }

        else {
            fprintf(stderr, "Unrecognized option: %s\n"
                    "Try `%s --help` for more information.\n",
                    argv[k], argv[0]);
            return -1;
        }
    }

    /* Display the help string. */

    if (help)
    {
        printf("%s <input\n"
                "or\n%s -h | --help\n\nConvert a YAML stream\n\nOptions:\n"
                "-h, --help\t\tdisplay this help and exit\n",
                argv[0], argv[0]);
        return 0;
    }
    
    /* Convert the input from yaml to json */
    return yaml_stream_to_json_stream(stdin, stdout);

}

