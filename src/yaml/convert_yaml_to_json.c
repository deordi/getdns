/*
 * 
 *
 *
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <yaml.h>
#include "convert_yaml_to_json.h"

#include "../../build/src/config.h"
#include "../gldns/gbuffer.h"

static int yaml_stream_to_json_string(FILE*, char**);

static int process_yaml_stream(yaml_parser_t*, yaml_event_t*, gldns_buffer*);

static int process_yaml_document(yaml_parser_t*, yaml_event_t*, gldns_buffer*);

static int process_yaml_sequence(yaml_parser_t*, yaml_event_t*, gldns_buffer*);

static int process_yaml_mapping(yaml_parser_t*, yaml_event_t*, gldns_buffer*);

static int process_yaml_value(yaml_parser_t*, yaml_event_t*, gldns_buffer*);

static int output_scalar(yaml_event_t*, gldns_buffer* buf);

static void report_parser_error(yaml_parser_t*);

static char* event_type_string(yaml_event_type_t);

/* public functions */

int
yaml_stream_to_json_stream(FILE* instream, FILE* outstream) {

    assert(instream);
    assert(outstream);
    
    char* json = NULL;
    
    if (yaml_stream_to_json_string(instream, &json) != 0) {

        return -1;
    }

    fprintf(outstream, "%s\n", json);
    
    free(json);
    
    return 0;

}

int
yaml_stream_to_json_string(FILE* instream, char** json) {

    assert(instream);

    gldns_buffer *buf;
    
    buf = gldns_buffer_new(8192);
    if (!buf) {
        fprintf(stderr, "Could not assign buffer for json output");
        return -1;
    }
    
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

    if (process_yaml_stream(&parser, &event, buf) != 0) {
        goto return_error;
    }

    /* Delete the event object and the parser. */
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
    
    /* gldns_buffer_write_string() copies the characters to the buffer, but not the terminating null byte */
    /* so we need to terminate the string before returning it. */

    char zero = 0;
    gldns_buffer_write(buf, &zero, 1);
    //fprintf(stderr, "Debug - buf contents, now zero terminated:  %s\n", buf->_data);

    /* gldns_buffer_export() returns a pointer to the data and 
         sets a flag to prevent it from being deleted by gldns_buffer_free() */
    *json = (char *) gldns_buffer_export(buf);
    gldns_buffer_free(buf);
    fprintf(stderr, "Debug - json string:  %s\n", *json);

    //*json = (char *) malloc(strlen(json2));
    //strcpy(*json, json2);
    //free(json2);
    
    return 0;

return_error:

    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
    gldns_buffer_free(buf);

    return -1;

}

/* local functions */

int
process_yaml_stream(yaml_parser_t* parser, yaml_event_t* event, gldns_buffer* buf) {

    assert(parser);
    assert(event);
    assert(buf);

    int done = 0;

    while (!done)
    {
        /* Delete the event that brought us here */
        yaml_event_delete(event);

        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return -1;
        }

        /* Analyze the event. */
        switch (event->type)
        {
            case YAML_STREAM_END_EVENT:

                done = 1;
                break;

            case YAML_DOCUMENT_START_EVENT:

                if (process_yaml_document(parser, event, buf) != 0) {
                    return -1;   
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
                return -1;

            default:
                /* It couldn't really happen. */
                break;
        }

    }

    return 0;

}

int
process_yaml_document(yaml_parser_t* parser, yaml_event_t* event, gldns_buffer* buf) {

    assert(parser);
    assert(event);
    assert(buf);

    int done = 0;

    while (!done)
    {
        /* Delete the event that brought us here */
        yaml_event_delete(event);

        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return -1;
        }

        /* Analyze the event. */
        switch (event->type)
        {
            case YAML_DOCUMENT_END_EVENT:

                done = 1;
                break;

            case YAML_MAPPING_START_EVENT:

                /* getdns config data is a dictionary (ie yaml mapping) */
                /* so the document must start with a mapping. scalar or sequence would be wrong. */
                if (process_yaml_mapping(parser, event, buf) != 0) {
                    return -1;   
                }
                break;

            case YAML_STREAM_START_EVENT:
            case YAML_STREAM_END_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_ALIAS_EVENT:
            case YAML_SCALAR_EVENT:
            case YAML_SEQUENCE_START_EVENT: /* getdns config data is a dictionary (ie yaml mapping) */
            case YAML_SEQUENCE_END_EVENT:
            case YAML_MAPPING_END_EVENT:

                fprintf(stderr, "Event error: %s. Expected YAML_MAPPING_START_EVENT or YAML_DOCUMENT_END_EVENT.\n",
                        event_type_string(event->type));
                return -1;
                break;

            default:
                /* It couldn't really happen. */
                break;
        }

    }

    return 0;
    
}

int
process_yaml_sequence(yaml_parser_t* parser, yaml_event_t* event, gldns_buffer* buf) {
    
    assert(parser);
    assert(event);
    assert(buf);
    
    int done = 0;
    int elements = 0;

    char *seqstart = "[ ";
    char *seqend = " ]";
    char *comma = ", ";
    
    if (gldns_buffer_remaining(buf) <= strlen(seqstart)) {
        fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
        return -1;
    }
    gldns_buffer_write_string(buf, seqstart);
    //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);
   
    while (!done)
    {
        /* Delete the event that brought us here */
        yaml_event_delete(event);
        
        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return -1;
        }

        /* Analyze the event. */
        switch (event->type)
        {
            case YAML_SCALAR_EVENT:
            case YAML_SEQUENCE_START_EVENT:
            case YAML_MAPPING_START_EVENT:

                if (elements) {
                    if (gldns_buffer_remaining(buf) <= strlen(comma)) {
                        fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
                        return -1;
                    }
                    gldns_buffer_write_string(buf, comma);
                    //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);
                }

                if (process_yaml_value(parser, event, buf) != 0) {
                    return -1;   
                }
                elements = 1;
                break;

            case YAML_SEQUENCE_END_EVENT:

                if (gldns_buffer_remaining(buf) <= strlen(seqend)) {
                    fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
                    return -1;
                }
                gldns_buffer_write_string(buf, seqend);
                //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);
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
                return -1;
                break;

            default:
                /* It couldn't really happen. */
                break;
        }

    }

    return 0;

}

int
process_yaml_mapping(yaml_parser_t* parser, yaml_event_t* event, gldns_buffer* buf) {

    assert(parser);
    assert(event);
    assert(buf);
    
    int done = 0;
    int members = 0;

    char *mapstart = "{ ";
    char *mapend = " }";
    char *colon = ": ";
    char *comma = ", ";
    
    if (gldns_buffer_remaining(buf) <= strlen(mapstart)) {
        fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
        return -1;
    }
    gldns_buffer_write_string(buf, mapstart);
    //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);
    
    while (!done)
    {
        /* Delete the event that brought us here */
        yaml_event_delete(event);
        
        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return -1;
        }

        /* Analyze the event. */
        if (event->type == YAML_SCALAR_EVENT) {

            if (members){
                if (gldns_buffer_remaining(buf) <= strlen(comma)) {
                    fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
                    return -1;
                }
                gldns_buffer_write_string(buf, comma);
                //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);
            }
            if (output_scalar(event, buf) != 0) {
                fprintf(stderr, "Mapping error: Error outputting key\n");
                return -1;
            }

            if (gldns_buffer_remaining(buf) <= strlen(colon)) {
                fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
                return -1;
            }
            gldns_buffer_write_string(buf, colon);
            //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);
            members = 1;

        } else if (event->type == YAML_MAPPING_END_EVENT) {

            if (gldns_buffer_remaining(buf) <= strlen(mapend)) {
                fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
                return -1;
            }
            gldns_buffer_write_string(buf, mapend);
            //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);
            done = 1;
            continue;

        } else {

            fprintf(stderr, "Event error: %s. Expected YAML_SCALAR_EVENT or YAML_MAPPING_END_EVENT.\n",
                    event_type_string(event->type));
            return -1;

        }
        
        /* Delete the event that brought us here */
        yaml_event_delete(event);

        /* Get the next event. */
        if (!yaml_parser_parse(parser, event)) {
            report_parser_error(parser);
            return -1;
        }

        /* Analyze the event. */
        switch (event->type)
        {
            case YAML_SCALAR_EVENT:
            case YAML_SEQUENCE_START_EVENT:
            case YAML_MAPPING_START_EVENT:

                if (process_yaml_value(parser, event, buf) != 0) {
                    return -1;   
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
    
    return 0;
    
}

int
process_yaml_value(yaml_parser_t* parser, yaml_event_t* event, gldns_buffer* buf){
    
    assert(parser);
    assert(event);
    assert(buf);
    
    switch (event->type)
    {
        case YAML_SCALAR_EVENT:

            if (output_scalar(event, buf) != 0) {
                fprintf(stderr, "Value error: Error outputting scalar\n");
                return -1;
            }
            break;

        case YAML_SEQUENCE_START_EVENT:

            if (process_yaml_sequence(parser, event, buf) != 0) {
                return -1;   
            }
            break;

        case YAML_MAPPING_START_EVENT:

            if (process_yaml_mapping(parser, event, buf) != 0) {
                return -1;   
            }
            break;
            
        default:
            fprintf(stderr, "Bug: calling process_yaml_value() in the wrong context");
            return -1;
    }
    
    return 0;
   
}

int
output_scalar(yaml_event_t* event, gldns_buffer* buf) {
    
    assert(event);
    assert(buf);
    assert(event->data.scalar.length > 0);
    
    if (event->data.scalar.style != YAML_PLAIN_SCALAR_STYLE) {

        if (gldns_buffer_remaining(buf) <= (event->data.scalar.length + 2)) {
            fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
            return -1;
        }
        gldns_buffer_write_string(buf, "\"");
        gldns_buffer_write(buf, event->data.scalar.value, event->data.scalar.length);
        gldns_buffer_write_string(buf, "\"");
        //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);

    } else {

        if (gldns_buffer_remaining(buf) <= event->data.scalar.length) {
            fprintf(stderr, "json buffer is full: only %lu remaining", gldns_buffer_remaining(buf));
            return -1;
        }
        gldns_buffer_write(buf, event->data.scalar.value, event->data.scalar.length);
        //fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);

    }

    return 0;
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
