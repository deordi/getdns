
#include <yaml.h>

#include <stdlib.h>
#include <stdio.h>

#include <assert.h>

static int process_yaml(FILE*, FILE*);

static int process_yaml_stream(yaml_parser_t*, yaml_event_t*, FILE*);

static int process_yaml_document(yaml_parser_t*, yaml_event_t*, FILE*);

static int process_yaml_sequence(yaml_parser_t*, yaml_event_t*, FILE*);

static int process_yaml_mapping(yaml_parser_t*, yaml_event_t*, FILE*);

static int process_yaml_value(yaml_parser_t*, yaml_event_t*, FILE*);

static int output_scalar(yaml_event_t*, FILE*);

static void report_parser_error(yaml_parser_t*);

static char* event_type_string(yaml_event_type_t);

/* TODO - improve this */
char*
event_type_string(yaml_event_type_t type) {
    
    switch (type)
    {
        case YAML_STREAM_START_EVENT:
            return "YAML_STREAM_START_EVENT";
            break;
            
        case YAML_STREAM_END_EVENT:
            return "YAML_STREAM_END_EVENT";
            break;
            
        case YAML_DOCUMENT_START_EVENT:
            return "YAML_DOCUMENT_START_EVENT";
            break;

        case YAML_DOCUMENT_END_EVENT:
            return "YAML_DOCUMENT_END_EVENT";
            break;

        case YAML_ALIAS_EVENT:
            return "YAML_ALIAS_EVENT";
            break;

        case YAML_SCALAR_EVENT:
            return "YAML_SCALAR_EVENT";
            break;

        case YAML_SEQUENCE_START_EVENT:
            return "YAML_SEQUENCE_START_EVENT";
            break;

        case YAML_SEQUENCE_END_EVENT:
            return "YAML_SEQUENCE_END_EVENT";
            break;

        case YAML_MAPPING_START_EVENT:
            return "YAML_MAPPING_START_EVENT";
            break;

        case YAML_MAPPING_END_EVENT:
            return "YAML_MAPPING_END_EVENT";
            break;

        default:
            /* It couldn't really happen. */
            return "";
            break;
    }
    return "";
}

int
process_yaml(FILE* instream, FILE* outstream) {

    assert(instream);
    assert(outstream);
    
    yaml_parser_t parser;
    yaml_event_t event;

    /* Clear the objects. */
    memset(&parser, 0, sizeof(parser));
    memset(&event, 0, sizeof(event));

    /* Initialize the parser object. */
    if (!yaml_parser_initialize(&parser)) {
        fprintf(stderr, "Could not initialize the parser object\n");
        goto return_error;
    }

    /* Set the parser parameters. */
    yaml_parser_set_input_file(&parser, instream);

    /* Get the first event. */
    if (!yaml_parser_parse(&parser, &event)) {
        report_parser_error(&parser);
        goto return_error;
    }

    /* Check if this is the stream start. */
    if (event.type != YAML_STREAM_START_EVENT) {
        fprintf(stderr, "Event error: wrong type of event: %d\n", event.type);
        goto return_error;
    }

    if (!process_yaml_stream(&parser, &event, outstream)) {
        goto return_error;
    }
    
    /* Delete the event object and the parser. */
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
    
    return 1;

return_error:

    yaml_event_delete(&event);
    yaml_parser_delete(&parser);

    return 0;

}

int
process_yaml_stream(yaml_parser_t* parser, yaml_event_t* event, FILE* outstream) {

    assert(parser);
    assert(event);
    assert(outstream);

    int done = 0;

    while (!done)
    {
        /* Delete the event that brought us here */
        yaml_event_delete(event);

        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return 0;
        }

        /* Analyze the event. */
        switch (event->type)
        {
            case YAML_STREAM_END_EVENT:

                done = 1;
                break;

            case YAML_DOCUMENT_START_EVENT:

                if (!process_yaml_document(parser, event, outstream)) {
                    return 0;   
                }
                break;

            case YAML_STREAM_START_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_ALIAS_EVENT:
            case YAML_SCALAR_EVENT:
            case YAML_SEQUENCE_START_EVENT:
            case YAML_SEQUENCE_END_EVENT:
            case YAML_MAPPING_START_EVENT:
            case YAML_MAPPING_END_EVENT:

                fprintf(stderr, "Event error: %s. Expected YAML_DOCUMENT_START_EVENT or YAML_STREAM_END_EVENT.\n",
                        event_type_string(event->type));
                return 0;

            default:
                /* It couldn't really happen. */
                break;
        }

    }

    return 1;

}

int
process_yaml_document(yaml_parser_t* parser, yaml_event_t* event, FILE* outstream) {

    assert(parser);
    assert(event);
    assert(outstream);

    int done = 0;

    while (!done)
    {
        /* Delete the event that brought us here */
        yaml_event_delete(event);

        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return 0;
        }

        /* Analyze the event. */
        switch (event->type)
        {
            case YAML_DOCUMENT_END_EVENT:

                done = 1;
                break;

                /* TODO: revisit if json doesn't like sequqnce-only objects */
            case YAML_SEQUENCE_START_EVENT:

                if (!process_yaml_sequence(parser, event, outstream)) {
                    return 0;   
                }
                break;

            case YAML_MAPPING_START_EVENT:

                if (!process_yaml_mapping(parser, event, outstream)) {
                    return 0;   
                }
                break;

            case YAML_STREAM_START_EVENT:
            case YAML_STREAM_END_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_ALIAS_EVENT:
            case YAML_SCALAR_EVENT:
            case YAML_SEQUENCE_END_EVENT:
            case YAML_MAPPING_END_EVENT:

                fprintf(stderr, "Event error: %s. Expected YAML_MAPPING_START_EVENT, YAML_SEQUENCE_START_EVENT or YAML_DOCUMENT_END_EVENT.\n",
                        event_type_string(event->type));
                return 0;
                break;

            default:
                /* It couldn't really happen. */
                break;
        }

    }

    return 1;
    
}

int
process_yaml_sequence(yaml_parser_t* parser, yaml_event_t* event, FILE* outstream) {
    
    assert(parser);
    assert(event);
    assert(outstream);
    
    int done = 0;
    int elements = 0;

    fprintf(outstream, "[ ");
    
    while (!done)
    {
        /* Delete the event that brought us here */
        yaml_event_delete(event);
        
        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return 0;
        }

        /* Analyze the event. */
        switch (event->type)
        {
            case YAML_SCALAR_EVENT:
            case YAML_SEQUENCE_START_EVENT:
            case YAML_MAPPING_START_EVENT:

                if (elements) fprintf(outstream, ", ");
                if (!process_yaml_value(parser, event, outstream)) {
                    return 0;   
                }
                elements = 1;
                break;

            case YAML_SEQUENCE_END_EVENT:

                fprintf(outstream, " ]");
                done = 1;
                break;

            case YAML_STREAM_START_EVENT:
            case YAML_STREAM_END_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_ALIAS_EVENT:
            case YAML_MAPPING_END_EVENT:

                fprintf(stderr, "Event error: %s. Expected YAML_MAPPING_START_EVENT, YAML_SEQUENCE_START_EVENT, YAML_SCALAR_EVENT or YAML_SEQUENCE_END_EVENT.\n",
                        event_type_string(event->type));
                return 0;
                break;

            default:
                /* It couldn't really happen. */
                break;
        }

    }

    return 1;

}

int
process_yaml_mapping(yaml_parser_t* parser, yaml_event_t* event, FILE* outstream) {

    assert(parser);
    assert(event);
    assert(outstream);
    
    int done = 0;
    int members = 0;

    fprintf(outstream, "{ ");
    
    while (!done)
    {
        /* Delete the event that brought us here */
        yaml_event_delete(event);
        
        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return 0;
        }

        /* Analyze the event. */
        if (event->type == YAML_SCALAR_EVENT) {

            if (members) fprintf(outstream, ", ");
            if (!output_scalar(event, outstream)) {
                fprintf(stderr, "Mapping error: Error outputting key\n");
                return 0;
            }

            fprintf(outstream, ": ");
            members = 1;

        } else if (event->type == YAML_MAPPING_END_EVENT) {

            fprintf(outstream, " }");
            done = 1;
            continue;

        } else {

            fprintf(stderr, "Event error: %s. Expected YAML_SCALAR_EVENT or YAML_MAPPING_END_EVENT.\n",
                    event_type_string(event->type));
            return 0;

        }
        
        /* Delete the event that brought us here */
        yaml_event_delete(event);

        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return 0;
        }

        /* Analyze the event. */
        switch (event->type)
        {
            case YAML_SCALAR_EVENT:
            case YAML_SEQUENCE_START_EVENT:
            case YAML_MAPPING_START_EVENT:

                if (!process_yaml_value(parser, event, outstream)) {
                    return 0;   
                }
                break;

            case YAML_STREAM_START_EVENT:
            case YAML_STREAM_END_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_ALIAS_EVENT:
            case YAML_SEQUENCE_END_EVENT:
            case YAML_MAPPING_END_EVENT:

                fprintf(stderr, "Event error: %s. Expected YAML_MAPPING_START_EVENT, YAML_SEQUENCE_START_EVENT or YAML_SCALAR_EVENT.\n",
                        event_type_string(event->type));
                return 0;

            default:
                /* It couldn't really happen. */
                break;
        }

    }
    
    return 1;
    
}

int
process_yaml_value(yaml_parser_t* parser, yaml_event_t* event, FILE* outstream){
    
    assert(parser);
    assert(event);
    assert(outstream);
    
    switch (event->type)
    {
        case YAML_SCALAR_EVENT:

            if (!output_scalar(event, outstream)) {
                fprintf(stderr, "Value error: Error outputting scalar\n");
                return 0;
            }
            break;

        case YAML_SEQUENCE_START_EVENT:

            if (!process_yaml_sequence(parser, event, outstream)) {
                return 0;   
            }
            break;

        case YAML_MAPPING_START_EVENT:

            if (!process_yaml_mapping(parser, event, outstream)) {
                return 0;   
            }
            break;
            
        default:
            fprintf(stderr, "Bug: calling process_yaml_value() in the wrong context");
            return 0;
    }
    
    return 1;
   
}

int
output_scalar(yaml_event_t* event, FILE* outstream) {
    
    assert(event);
    assert(outstream);
    assert(event->data.scalar.length > 0);

    if (event->data.scalar.style != YAML_PLAIN_SCALAR_STYLE) fprintf(outstream, "\"");
    
    for (int k = 0; k < event->data.scalar.length; k++) {
        fprintf(outstream, "%c", event->data.scalar.value[k]);
    }

    if (event->data.scalar.style != YAML_PLAIN_SCALAR_STYLE) fprintf(outstream, "\"");

    return 1;
}

void report_parser_error(yaml_parser_t* parser) {

    assert(parser);

    /* Display a parser error message. */
    switch (parser->error)
    {
        case YAML_MEMORY_ERROR:
            fprintf(stderr, "Memory error: Not enough memory for parsing\n");
            break;

        case YAML_READER_ERROR:
            if (parser->problem_value != -1) {
                fprintf(stderr, "Reader error: %s: #%X at %zu\n", parser->problem,
                        parser->problem_value, parser->problem_offset);
            }
            else {
                fprintf(stderr, "Reader error: %s at %zu\n", parser->problem,
                        parser->problem_offset);
            }
            break;

        case YAML_SCANNER_ERROR:
            if (parser->context) {
                fprintf(stderr, "Scanner error: %s at line %lu, column %lu\n"
                        "%s at line %lu, column %lu\n", parser->context,
                        parser->context_mark.line+1, parser->context_mark.column+1,
                        parser->problem, parser->problem_mark.line+1,
                        parser->problem_mark.column+1);
            }
            else {
                fprintf(stderr, "Scanner error: %s at line %lu, column %lu\n",
                        parser->problem, parser->problem_mark.line+1,
                        parser->problem_mark.column+1);
            }
            break;

        case YAML_PARSER_ERROR:
            if (parser->context) {
                fprintf(stderr, "Parser error: %s at line %lu, column %lu\n"
                        "%s at line %lu, column %lu\n", parser->context,
                        parser->context_mark.line+1, parser->context_mark.column+1,
                        parser->problem, parser->problem_mark.line+1,
                        parser->problem_mark.column+1);
            }
            else {
                fprintf(stderr, "Parser error: %s at line %lu, column %lu\n",
                        parser->problem, parser->problem_mark.line+1,
                        parser->problem_mark.column+1);
            }
            break;

        default:
            /* Couldn't happen. */
            fprintf(stderr, "Internal error\n");
            break;
    }
    return;
}

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

        else if (strcmp(argv[k], "-c") == 0
                || strcmp(argv[k], "--canonical") == 0) {
            canonical = 1;
        }

        else if (strcmp(argv[k], "-u") == 0
                || strcmp(argv[k], "--unicode") == 0) {
            unicode = 1;
        }

        else {
            fprintf(stderr, "Unrecognized option: %s\n"
                    "Try `%s --help` for more information.\n",
                    argv[k], argv[0]);
            return 1;
        }
    }

    /* Display the help string. */

    if (help)
    {
        printf("%s <input\n"
                "or\n%s -h | --help\nDeconstruct a YAML stream\n\nOptions:\n"
                "-h, --help\t\tdisplay this help and exit\n"
                "-c, --canonical\t\toutput in the canonical YAML format\n"
                "-u, --unicode\t\toutput unescaped non-ASCII characters\n",
                argv[0], argv[0]);
        return 0;
    }
    
    /* Convert the input from yaml to json */
    return process_yaml(stdin, stdout);

}

