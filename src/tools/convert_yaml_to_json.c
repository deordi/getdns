/*
 * Copyright (c) 2017, NLNet Labs, Verisign, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the names of the copyright holders nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <yaml.h>

#include "config.h"
#include "../gldns/gbuffer.h"
#include "convert_yaml_to_json.h"

static int process_yaml_stream(yaml_parser_t *, yaml_event_t *, gldns_buffer *);

static int process_yaml_document(yaml_parser_t *, yaml_event_t *, gldns_buffer *);

static int process_yaml_mapping(yaml_parser_t *, yaml_event_t *, gldns_buffer *);

static int process_yaml_sequence(yaml_parser_t *, yaml_event_t *, gldns_buffer *);

static int process_yaml_value(yaml_parser_t *, yaml_event_t *, gldns_buffer *);

static int output_scalar(yaml_event_t *, gldns_buffer *);

static void report_parser_error(yaml_parser_t *);

static char* event_type_string(yaml_event_type_t);

static int gbuffer_write(gldns_buffer *buf, void *data, size_t length);

/* public functions */

int
yaml_stream_to_json_stream(FILE *instream, FILE *outstream) {

	assert(instream);
	assert(outstream);
	
	char* json = NULL;
	
	if ((json = yaml_stream_to_json_string(instream)) == NULL) {
		return -1;
	}

	fprintf(outstream, "%s\n", json);
	
	free(json);
	
	return 0;
}

char *
yaml_stream_to_json_string(FILE *instream) {

	assert(instream);

	gldns_buffer *buf;
	char* json = NULL;

	buf = gldns_buffer_new(8192);
	if (!buf) {
		fprintf(stderr, "Could not assign buffer for json output");
		return NULL;
	}
	
	yaml_parser_t parser;
	yaml_event_t event;

	memset(&parser, 0, sizeof(parser));
	memset(&event, 0, sizeof(event));

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

	/* First event should be stream start. */
	if (event.type != YAML_STREAM_START_EVENT) {
		fprintf(stderr, "Event error: wrong type of event: %d\n", event.type);
		goto return_error;
	}

	if (process_yaml_stream(&parser, &event, buf) != 0) {
		goto return_error;
	}

	yaml_event_delete(&event);
	yaml_parser_delete(&parser);
	
	/* The string-terminating null byte has not been written to the buf yet. */
	/* We need to terminate the string before returning it. */
	char zero = 0;
	gldns_buffer_write(buf, &zero, 1);
	//fprintf(stderr, "Debug - buf contents, now zero terminated:  %s\n", buf->_data);

	/* gldns_buffer_export() returns a pointer to the data and 
		 sets a flag to prevent it from being deleted by gldns_buffer_free() */
	json = (char *) gldns_buffer_export(buf);
	gldns_buffer_free(buf);
	//fprintf(stderr, "Debug - json string:  %s\n", json);

	return json;

return_error:

	yaml_event_delete(&event);
	yaml_parser_delete(&parser);
	gldns_buffer_free(buf);

	return NULL;
}

/* local functions */

int
process_yaml_stream(yaml_parser_t *parser, yaml_event_t *event, gldns_buffer *buf) {

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

		switch (event->type) {
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
			/* NOTREACHED */
			break;
		}
	}
	return 0;
}

int
process_yaml_document(yaml_parser_t *parser, yaml_event_t *event, gldns_buffer *buf) {

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

		switch (event->type) {
		case YAML_DOCUMENT_END_EVENT:

			done = 1;
			break;

		case YAML_MAPPING_START_EVENT:

			/* getdns config data is a dictionary (ie yaml mapping) */
			/* so the document must start with a mapping; scalar or sequence would be wrong. */
			if (process_yaml_mapping(parser, event, buf) != 0) {
				return -1;   
			}
			break;

		case YAML_STREAM_START_EVENT:
		case YAML_STREAM_END_EVENT:
		case YAML_DOCUMENT_START_EVENT:
		case YAML_ALIAS_EVENT:
		case YAML_SCALAR_EVENT:
		case YAML_SEQUENCE_START_EVENT:
		case YAML_SEQUENCE_END_EVENT:
		case YAML_MAPPING_END_EVENT:

			fprintf(stderr, "Event error: %s. Expected YAML_MAPPING_START_EVENT or YAML_DOCUMENT_END_EVENT.\n",
					event_type_string(event->type));
			return -1;

		default:
			/* NOTREACHED */
			break;
		}
	}
	return 0;
}

int
process_yaml_mapping(yaml_parser_t *parser, yaml_event_t *event, gldns_buffer *buf) {

	assert(parser);
	assert(event);
	assert(buf);
	
	int done = 0;
	int members = 0;

	char *mapstart = "{ ";
	char *mapend = " }";
	char *colon = ": ";
	char *comma = ", ";
	
	if (gbuffer_write(buf, mapstart, strlen(mapstart)) != 0) {
		return -1;
	}
	
	while (!done)
	{
		/* Delete the event that brought us here */
		yaml_event_delete(event);
		
		/* Get the next event. */
		if (!yaml_parser_parse(parser, event)) {
			report_parser_error(parser);
			return -1;
		}

		if (event->type == YAML_SCALAR_EVENT) {
			if (members){
				if (gbuffer_write(buf, comma, strlen(comma)) != 0) {
					return -1;
				}
			}
			if (output_scalar(event, buf) != 0) {
				fprintf(stderr, "Mapping error: Error outputting key\n");
				return -1;
			}
			if (gbuffer_write(buf, colon, strlen(colon)) != 0) {
				return -1;
			}
			members = 1;
			
		} else if (event->type == YAML_MAPPING_END_EVENT) {
			if (gbuffer_write(buf, mapend, strlen(mapend)) != 0) {
				return -1;
			}
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

		switch (event->type) {
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
			return -1;

		default:
			/* NOTREACHED */
			break;
		}
	}
	return 0;
}

int
process_yaml_sequence(yaml_parser_t *parser, yaml_event_t *event, gldns_buffer *buf) {
	
	assert(parser);
	assert(event);
	assert(buf);
	
	int done = 0;
	int elements = 0;

	char *seqstart = "[ ";
	char *seqend = " ]";
	char *comma = ", ";

	if (gbuffer_write(buf, seqstart, strlen(seqstart)) != 0) {
		return -1;
	}
   
	while (!done)
	{
		/* Delete the event that brought us here */
		yaml_event_delete(event);
		
		/* Get the next event. */
		if (!yaml_parser_parse(parser, event)) {
			report_parser_error(parser);
			return -1;
		}

		switch (event->type) {
		case YAML_SCALAR_EVENT:
		case YAML_SEQUENCE_START_EVENT:
		case YAML_MAPPING_START_EVENT:

			if (elements) {
				if (gbuffer_write(buf, comma, strlen(comma)) != 0) {
					return -1;
				}
			}

			if (process_yaml_value(parser, event, buf) != 0) {
				return -1;   
			}
			elements = 1;
			break;

		case YAML_SEQUENCE_END_EVENT:

			if (gbuffer_write(buf, seqend, strlen(seqend)) != 0) {
				return -1;
			}
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

		default:
			/* NOTREACHED */
			break;
		}
	}
	return 0;
}

int
process_yaml_value(yaml_parser_t *parser, yaml_event_t *event, gldns_buffer *buf){
	
	assert(parser);
	assert(event);
	assert(buf);
	
	switch (event->type) {
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
output_scalar(yaml_event_t *event, gldns_buffer *buf) {
	
	assert(event);
	assert(buf);
	assert(event->data.scalar.length > 0);
	
	if (event->data.scalar.style != YAML_PLAIN_SCALAR_STYLE) {

		if (gbuffer_write(buf, "\"", 1) != 0) {
			return -1;
		}
		if (gbuffer_write(buf, event->data.scalar.value, event->data.scalar.length) != 0) {
			return -1;
		}
		if (gbuffer_write(buf, "\"", 1) != 0) {
			return -1;
		}
	} else {
		if (gbuffer_write(buf, event->data.scalar.value, event->data.scalar.length) != 0) {
			return -1;
		}
	}
	return 0;
}

void report_parser_error(yaml_parser_t *parser) {

	assert(parser);

	/* Display a parser error message. */
	switch (parser->error) {
	case YAML_MEMORY_ERROR:
		fprintf(stderr, "Memory error: Not enough memory for parsing\n");
		break;

	case YAML_READER_ERROR:
		if (parser->problem_value != -1) {
			fprintf(stderr, "Reader error: %s: #%X at %zu\n", parser->problem,
					parser->problem_value, parser->problem_offset);
		} else {
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
		} else {
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
		} else {
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
	
	switch (type) {
	case YAML_STREAM_START_EVENT:
		return "YAML_STREAM_START_EVENT";
		
	case YAML_STREAM_END_EVENT:
		return "YAML_STREAM_END_EVENT";
		
	case YAML_DOCUMENT_START_EVENT:
		return "YAML_DOCUMENT_START_EVENT";

	case YAML_DOCUMENT_END_EVENT:
		return "YAML_DOCUMENT_END_EVENT";

	case YAML_ALIAS_EVENT:
		return "YAML_ALIAS_EVENT";

	case YAML_SCALAR_EVENT:
		return "YAML_SCALAR_EVENT";

	case YAML_SEQUENCE_START_EVENT:
		return "YAML_SEQUENCE_START_EVENT";

	case YAML_SEQUENCE_END_EVENT:
		return "YAML_SEQUENCE_END_EVENT";

	case YAML_MAPPING_START_EVENT:
		return "YAML_MAPPING_START_EVENT";

	case YAML_MAPPING_END_EVENT:
		return "YAML_MAPPING_END_EVENT";

	default:
		/* NOTREACHED */
		return NULL;
	}
	return NULL;
}

int
gbuffer_write(gldns_buffer *buf, void *data, size_t length) {
	if (gldns_buffer_remaining(buf) <= length) {
		fprintf(stderr, "json buffer is full: only %lu bytes remaining", gldns_buffer_remaining(buf));
		return -1;
	}
	gldns_buffer_write(buf, data, length);
	//fprintf(stderr, "Debug - buf contents:  %s\n", buf->_data);
	return 0;
}
