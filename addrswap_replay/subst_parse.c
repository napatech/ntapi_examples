/*
 * Copyright 2022 Napatech A/S. All rights reserved.
 * 
 * 1. Copying, modification, and distribution of this file, or executable
 * versions of this file, is governed by the terms of the Napatech Software
 * license agreement under which this file was made available. If you do not
 * agree to the terms of the license do not install, copy, access or
 * otherwise use this file.
 * 
 * 2. Under the Napatech Software license agreement you are granted a
 * limited, non-exclusive, non-assignable, copyright license to copy, modify
 * and distribute this file in conjunction with Napatech SmartNIC's and
 * similar hardware manufactured or supplied by Napatech A/S.
 * 
 * 3. The full Napatech Software license agreement is included in this
 * distribution, please see "NP-0405 Napatech Software license
 * agreement.pdf"
 * 
 * 4. Redistributions of source code must retain this copyright notice,
 * list of conditions and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES, EXPRESS OR
 * IMPLIED, AND NAPATECH DISCLAIMS ALL IMPLIED WARRANTIES INCLUDING ANY
 * IMPLIED WARRANTY OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, OR OF
 * FITNESS FOR A PARTICULAR PURPOSE. TO THE EXTENT NOT PROHIBITED BY
 * APPLICABLE LAW, IN NO EVENT SHALL NAPATECH BE LIABLE FOR PERSONAL INJURY,
 * OR ANY INCIDENTAL, SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES WHATSOEVER,
 * INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS, CORRUPTION OR
 * LOSS OF DATA, FAILURE TO TRANSMIT OR RECEIVE ANY DATA OR INFORMATION,
 * BUSINESS INTERRUPTION OR ANY OTHER COMMERCIAL DAMAGES OR LOSSES, ARISING
 * OUT OF OR RELATED TO YOUR USE OR INABILITY TO USE NAPATECH SOFTWARE OR
 * SERVICES OR ANY THIRD PARTY SOFTWARE OR APPLICATIONS IN CONJUNCTION WITH
 * THE NAPATECH SOFTWARE OR SERVICES, HOWEVER CAUSED, REGARDLESS OF THE THEORY
 * OF LIABILITY (CONTRACT, TORT OR OTHERWISE) AND EVEN IF NAPATECH HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGES. SOME JURISDICTIONS DO NOT ALLOW
 * THE EXCLUSION OR LIMITATION OF LIABILITY FOR PERSONAL INJURY, OR OF
 * INCIDENTAL OR CONSEQUENTIAL DAMAGES, SO THIS LIMITATION MAY NOT APPLY TO YOU.
 */

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <subst_parse.h>

#include <yaml.h>

#include "include/btree.h"

#define TRUE  1
#define FALSE  0




//=====================================================================
//
#if 1
void PrintIP(uint32_t address) {
	char temp[32];
	snprintf(temp, 32, "%i.%i.%i.%i",
			(address & 0xff),
			(address >> 8) & 0xff,
			(address >> 16) & 0xff,
			(address >> 24) & 0xff
	);
	int pad = 16 - strlen(temp);
	for (int i = 0; i < pad/2; ++i) printf(" ");
	printf("%s", temp);
	for (int i = strlen(temp)+pad/2; i < 16; ++i) printf(" ");
//	return len;

//	printf("%i.%i.%i.%i",
//			(address & 0xff),
//			(address >> 8) & 0xff,
//			(address >> 16) & 0xff,
//			(address >> 24) & 0xff
//	);
}
#endif

static uint32_t PackIP(char *address) {
	uint32_t packed_address = 0;
	const char dot = '.';
	char *byte_str[4];
	short byte[4];
	char temp[33];

	strncpy(temp, address, 32);

	byte_str[0] = temp;

	byte_str[1] = strchr(byte_str[0], dot);
	*byte_str[1] = 0;
	++byte_str[1];

	byte_str[2] = strchr(byte_str[1], dot);
	*byte_str[2] = 0;
	++byte_str[2];

	byte_str[3] = strchr(byte_str[2], dot);
	*byte_str[3] = 0;
	++byte_str[3];

	byte[0] = (short) strtol(byte_str[0], NULL, 10);
	byte[1] = (short) strtol(byte_str[1], NULL, 10);
	byte[2] = (short) strtol(byte_str[2], NULL, 10);
	byte[3] = (short) strtol(byte_str[3], NULL, 10);

	packed_address = byte[0] + (byte[1] << 8) + (byte[2] << 16) + (byte[3] << 24);

	return packed_address;
}

//=====================================================================
//
static node *root;

uint32_t load_substitute_addr_map(char *sub_filename) {
	static yaml_parser_t parser;
	FILE *fh = fopen(sub_filename, "r");
	yaml_token_t  token;   /* new variable */

	 uint32_t line_number = 0;

	 root = NULL;

	/* Initialize parser */
	if(!yaml_parser_initialize(&parser))
		fputs("Failed to initialize parser!\n", stderr);
	if(fh == NULL)
		fputs("Failed to open file!\n", stderr);

	/* Set input file */
	yaml_parser_set_input_file(&parser, fh);

	// BEGIN new code
	do {
		uint32_t initial = 0;
		uint32_t replace_with = 0;

		yaml_parser_scan(&parser, &token);
		//show_token_type(token);

		if (token.type == 0) {
			return 0;
		}
		++line_number;

		if ((token.type == YAML_STREAM_START_TOKEN) ||
				(token.type == YAML_BLOCK_MAPPING_START_TOKEN) ||
				(token.type == YAML_BLOCK_END_TOKEN)) {
			continue;
		}

		if(token.type == YAML_STREAM_END_TOKEN) {
			break;
		}

		if (token.type == YAML_KEY_TOKEN) {
			yaml_parser_scan(&parser, &token);

			if (token.type == YAML_SCALAR_TOKEN) {
				initial = PackIP((char *)token.data.scalar.value);
			} else {
				//*line_number = token.line;
				return FALSE;
			}

			yaml_parser_scan(&parser, &token);

			if (token.type == YAML_VALUE_TOKEN) {
				yaml_parser_scan(&parser, &token);

				if (token.type == YAML_SCALAR_TOKEN) {
					replace_with = PackIP((char *)token.data.scalar.value);
				} else {
					return FALSE;
				}
			} else {
				return FALSE;
			}

			node *duplicate = search(&root, initial);
			if (!duplicate) {
				printf("Replace ");
				PrintIP(initial);
				printf(" with ");
				PrintIP(replace_with);
				printf("\n");
				insert(&root, initial, replace_with);
			} else {
				printf("--- WARNING: Duplicate Entry found for: ");
				PrintIP(initial);
				printf(" - ignoring ");
				PrintIP(initial);
				printf(": ");
				PrintIP(replace_with);
				printf("\n");
			}
		}

	} while(token.type != YAML_STREAM_END_TOKEN);

	yaml_token_delete(&token);
	// END new code

	/* Cleanup */
	yaml_parser_delete(&parser);
	fclose(fh);

#if 0
	printf("--------------\n");
	print_inorder(root);
	printf("--------------\n");
#endif

	return TRUE;
}

int search_addr_map(uint32_t val) {

	node *found_node = search(&root, val);

	if (found_node) {
//		printf("\n    - SAM FOUND -- val: 0x%x - key: 0x%x - data: 0x%x  - - ", val, found_node->key, found_node->data );
		return found_node->data;
	} else {
//		printf("\n    - NOT Found - val: 0x%x", val);
		return -1;
	}
}

void delete_addr_map(void) {
	deltree(root);
}

