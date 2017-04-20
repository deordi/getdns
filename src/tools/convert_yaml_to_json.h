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

#ifndef _CONVERT_YAML_TO_JSON_H
#define _CONVERT_YAML_TO_JSON_H

#include <stdio.h>

/**
 * read yaml-syntax data from the stream and convert to json-syntax
 * yaml syntax resitrictions imposed for getdns:
 *    the outer-most data structure must be a yaml mapping
 *    mapping keys must be yaml scalars
 *    plain scalars are output to the json string unchanged
 *    non-plain scalars (quoted, double-quoted, wrapped) are output double-quoted
 * TODO Test on yaml data containing yaml tags (these are ignored at present)
 *      The code has only been tested on yaml data using indentation style, so it
 *        should be tested on other styles as well.
 * @param instream the stream carrying data in yaml syntax
 * @return a string of data in json syntax on success
 * @return NULL if there is a yaml syntax violation
 *                 the outer-most structure in not a mapping
 *                 a mapping key is complex (a mapping or sequence)
 */
char * yaml_stream_to_json_string(FILE *instream);

/**
 * read yaml-syntax data from the named stream and output json data
 * See char * yaml_stream_to_json_string(FILE *instream) for yaml syntax restrictions
 * @param instream the stream carrying data in yaml syntax
 * @param outstream the stream receiving data in json syntax
 * @return 0 on success
 * @return -1 if there is a yaml syntax violation
 *               the outer-most structure in not a mapping
 *               a mapping key is complex (a mapping or sequence)
 */
int yaml_stream_to_json_stream(FILE *instream, FILE *outstream);

#endif //_CONVERT_YAML_TO_JSON_H
