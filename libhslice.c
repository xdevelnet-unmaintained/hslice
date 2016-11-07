#include "libhslice_real.h"

#include <stdio.h>
#include <stdlib.h>
#include <iso646.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>

#define forever 1
#define BASE_STRINGS_COUNT 10

typedef union { // because we gonna store array of offsets (seeks?), then transform it into array of pointers
	char *ptr;
	size_t seek;
} ptr_and_seek;

struct tag_and_data {
	size_t length;
	ptr_and_seek tag;
	ptr_and_seek data;
};

struct hslice_obj {
	char *filemem;
	size_t fmemsize;
	tag_and_data *table;
	size_t tablesize;
	ssize_t parsed_strings;
	char **tags;
};

typedef struct {
	const char *prefix;
	const char *suffix;
	size_t prefixlen;
	size_t suffixlen;
	char *prefix_and_suffix;
	char *prefix_noskip_suffix;
} parser_internal;

static void erase_hslice_obj_ptrs(hslice_obj *obj) { // because we'll perform free() at hslice_close(); Using free(NULL) is safe.
	obj->filemem = NULL;
	obj->table = NULL;
	obj->tags = NULL;
}

const char empty_string[] = "";
const char nullword[] = "NULL";
const char skipword[] = "SKIP";
const char noskipword[] = "NOSKIP";

hslice_obj hslice_open(char *filename) { // TODO: Rewrite it using only POSIX library instead of regular C file handling. This library is not designed to run under WIN
	hslice_obj obj = {0};
	if (filename == NULL) {
		fprintf(stderr, "%s\n", "No filename specified.");
		obj.parsed_strings = -1;
		return obj;
	}
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		if (errno != 0) perror(filename); else fprintf(stderr, "%s: %s.\n", filename, "can't open file");
		obj.parsed_strings = -1;
		return obj;
	}
	fseek(file, 0L, SEEK_END); // dumb, but cross platform way to determine size of file
	long int fseekval = ftell(file);
	if (fseekval < 0) {
		if (errno != 0) perror("Can't seek over the file"); else fprintf(stderr, "%s %s.\n", "Can't seek over the file", filename);
		// using stdio functions are not guarantee that errno should be set appropriately
		obj.parsed_strings = -1;
		fclose(file);
		return obj;
	}
	rewind(file);
	obj.fmemsize = (size_t) fseekval;
	erase_hslice_obj_ptrs(&obj);
	obj.filemem = calloc(obj.fmemsize + 1, sizeof(char)); // libhslice is created to work with texts, so end should be null-terminated
	if (obj.filemem == NULL) {
		fprintf(stderr, "%s\n", "Memory allocation failed.");
		obj.parsed_strings = -1;
		fclose(file);
		return obj;
	}
	if (fread(obj.filemem, sizeof(char), obj.fmemsize, file) < obj.fmemsize) { // flush whole memory with file content
		obj.parsed_strings = -1;
		fclose(file);
		return obj;
	} 
	fclose(file);
	return obj;
}

char *ftag(char *ptr, const char *suffix, size_t suffixlen) { // we gonna check tag existence. If it exist - return pointer to suffix, otherwise - NULL.
	char current_char = *ptr;
	while (isalpha(current_char) != 0 or current_char == '_') { // so, tag may be only alphabetic character or underscore. Otherwise, carry on
		if (strncmp(ptr, suffix, suffixlen) == 0) return ptr;
		current_char = *++ptr;
	}
	if (strncmp(ptr, suffix, suffixlen) == 0) return ptr; // IDK how I can avoid this. Everything is working, so just leave it for a future refactoring
	return NULL;
}

bool add_to_table(hslice_obj *obj, char *tag, char *data) { // Do I really need to write test for this procedure?
	obj->table[obj->parsed_strings].tag.seek = tag - obj->filemem;
	obj->table[obj->parsed_strings].data.seek = data - obj->filemem;
	obj->table[obj->parsed_strings].length = strlen(data);
	obj->parsed_strings++;
	if ((size_t) obj->parsed_strings >= obj->tablesize - 1) { // can't be -1 in this scope. Typecast is needed only for removing compiler warning
		obj->tablesize *= 2;
		obj->table = realloc(obj->table, obj->tablesize * sizeof(tag_and_data));
		if (obj->table == NULL) {
			obj->parsed_strings = -1;
			fprintf(stderr, "%s\n", "Unable to reallocate more memory for table.");
			return false;
		}
	}
	return true;
}

void modify_seeks_to_pointers(hslice_obj *obj) {
	ssize_t i = 0;
	while (i < obj->parsed_strings) {
		obj->table[i].data.ptr = obj->filemem + obj->table[i].data.seek;
		obj->table[i].tag.ptr = obj->filemem + obj->table[i].tag.seek;
		i++;
	}
}

bool parser_preparations(hslice_obj *obj, parser_internal *parser_req) { // http://xdevelnet.org/u/656.png
	if (obj == NULL) return NULL; // idiot protection // UPD: Should I protect myself from being an idiot?
	if (parser_req->prefix == NULL or parser_req->suffix == NULL) {
		obj->parsed_strings = -1;
		return false;
	}

	parser_req->prefixlen = strlen(parser_req->prefix); // yes, knowing length of prefix and suffix will be useful several times
	parser_req->suffixlen = strlen(parser_req->suffix);
	if (parser_req->prefixlen == 0 or parser_req->suffixlen == 0) {
		obj->parsed_strings = -1;
		return false;
	}

	parser_req->prefix_and_suffix = calloc(parser_req->prefixlen + parser_req->suffixlen + 1, sizeof(char));
	// concatenated prefix and suffix.
	if (parser_req->prefix_and_suffix == NULL) {
		fprintf(stderr, "%s\n", "Memory allocation failed.");
		obj->parsed_strings = -1;
		return false;
	}
	strcpy(parser_req->prefix_and_suffix, parser_req->prefix);
	strcat(parser_req->prefix_and_suffix, parser_req->suffix);

	obj->table = calloc(BASE_STRINGS_COUNT, sizeof(tag_and_data)); // prepare memory for sorted pointers (or offsets) array
	if (obj->table == NULL) {
		fprintf(stderr, "%s\n", "Memory allocation failed.");
		obj->parsed_strings = -1;
		free(parser_req->prefix_and_suffix);
	}
	obj->tablesize = BASE_STRINGS_COUNT; // base size. Will be expanded if needed during parsing at add_to_table()
	obj->parsed_strings = 0;

	parser_req->prefix_noskip_suffix = calloc(parser_req->prefixlen + parser_req->suffixlen + sizeof(noskipword), sizeof(char));
	if (parser_req->prefix_noskip_suffix == NULL) {
		fprintf(stderr, "%s\n", "Memory allocation failed.");
		obj->parsed_strings = -1;
		free(parser_req->prefix_and_suffix);
		free(obj->table);
		return false;
	}
	strcpy(parser_req->prefix_noskip_suffix, parser_req->prefix);
	strcat(parser_req->prefix_noskip_suffix, noskipword);
	strcat(parser_req->prefix_noskip_suffix, parser_req->suffix);
	return true;
}

static bool validate(parser_internal *parser_req, char *ptr) {
	if (strncmp(parser_req->prefix, ptr, parser_req->prefixlen) == 0) return true;
	return false;
}

bool throw_away_if_tag_is_NULL(hslice_obj *obj, char *ptr, parser_internal *parser_req, char *rollover) { // this procedure will check if tag equals NULL (check doc for details)
	if (strncmp(ptr, parser_req->prefix, parser_req->prefixlen) == 0 and
		strncmp(ptr + parser_req->prefixlen, nullword, sizeof(nullword) - 1) == 0 and
		strncmp(ptr + parser_req->prefixlen + sizeof(nullword) - 1, parser_req->suffix, parser_req->suffixlen) == 0) {
		// datadatadata otherdataotherdata{pref_NULL}usefuldatausefuldata
		//             ^^                 ^          ^
		//            / |                 |          |
		//           /  |                 |          |
		//          /   |                 |          |
		//        '0'  rollover          ptr      movepoint
		//
		char *movepoint = ptr + parser_req->prefixlen + sizeof(nullword) - 1 + parser_req->suffixlen;
		size_t len = obj->filemem + obj->fmemsize - movepoint;
		size_t distance = movepoint - rollover;
		if (len < distance) *(rollover + len) = '\0'; // when memmoving small chunk of data close to EOF
		obj->fmemsize -= distance;
		memmove(rollover, movepoint, len); // last arg is size from movepoint to end of memory
		return true;
	}
	return false;
}

signed int cut_away_if_tag_is_SKIP(hslice_obj *obj, char *ptr, parser_internal *parser_req) {
//  return values explanation:
//  1 means that we found skip-noskip pair and done what's needed;
// -1 means, that no more parsing required;
//  0 means, that current tag isn't SKIP
	if (strncmp(ptr, parser_req->prefix, parser_req->prefixlen) == 0 and
		strncmp(ptr + parser_req->prefixlen, skipword, sizeof(skipword) - 1) == 0 and
		strncmp(ptr + parser_req->prefixlen + sizeof(skipword) - 1, parser_req->suffix, parser_req->suffixlen) == 0) {
		char *movepoint = strstr(ptr + parser_req->prefixlen + parser_req->suffixlen + sizeof(noskipword) - 1, parser_req->prefix_noskip_suffix);
		// still not movepoint, just separator with NOSKIP tag

		if (movepoint == NULL or movepoint >= obj->filemem + obj->fmemsize) { // we should not parse behind end. Actually, second check can
			// be omitted because of adding null separator at end of this function body. Well, whatever, more safety == better.
			obj->fmemsize -= obj->filemem + obj->fmemsize - ptr;
			return -1;
		}
		movepoint += parser_req->prefixlen + sizeof(noskipword) - 1 + parser_req->suffixlen; // now it's really movepoint
		// datadatadata{pref_SKIP}otherdataotherdata{pref_NOSKIP}anotherdata
		//             ^                                         ^
		//             |                                         |
		//             |                                         |
		//             |                                         |
		//            ptr                                    movepoint
		//
		size_t len = obj->filemem + obj->fmemsize - movepoint;
		size_t distance = movepoint - ptr;
		if (len < distance) *(ptr + len) = '\0';
		obj->fmemsize -= distance;
		memmove(ptr, movepoint, len);
		*(ptr+len) = '\0'; // we should not parse behind end
		return 1;
	}
	return 0;
}

bool parse(hslice_obj *obj, parser_internal *parser_req) { // parse it, goddamnit!
	char *data_ptr = obj->filemem;
	char *tag_ptr;

	char *flying_ptr = obj->filemem; // this pointer will "fly" over file contents
	char *flying_suffix_after_tag;

	while (forever) {
		search_again:
		flying_ptr = strstr(flying_ptr, parser_req->prefix); // find next occurrence of prefix
		if (flying_ptr == NULL) break; // nothing more found (we found null byte?) --> stopping
		if (throw_away_if_tag_is_NULL(obj, flying_ptr, parser_req, data_ptr) == true) {
			flying_ptr = data_ptr;
			goto search_again;
		}
		signed int skip_noskip_retval = cut_away_if_tag_is_SKIP(obj, flying_ptr, parser_req);
		if (skip_noskip_retval == -1) break;
		if (skip_noskip_retval == 1) {
			flying_ptr = data_ptr;
			goto search_again;
		}
		if (strncmp(flying_ptr, parser_req->prefix_and_suffix, parser_req->prefixlen + parser_req->suffixlen) == 0 and
			validate(parser_req, flying_ptr + parser_req->prefixlen + parser_req->suffixlen) == true) {
			// prefix+suffix behind other valid delimiter
			memmove(flying_ptr, flying_ptr + parser_req->prefixlen + parser_req->suffixlen, obj->filemem + obj->fmemsize - (flying_ptr + parser_req->prefixlen + parser_req->suffixlen));
			flying_ptr += parser_req->prefixlen;
			continue;
		}
		flying_suffix_after_tag = ftag(flying_ptr + parser_req->prefixlen, parser_req->suffix, parser_req->suffixlen); // check if tag is correct
		if (flying_suffix_after_tag == NULL) {
			flying_ptr += parser_req->prefixlen;
			continue;
		}
		// now we gonna do something like that (sigh)
		// "data{PREFIX_TAG}otherdata" -> memmove and place nulls -> "data\0TAG\0otherdata"
		// then point TAG pointer to 'T' (5) after '\0' and DATA pointer to 'd' (0)
		if (parser_req->prefixlen > 1) { // nothing to move when prefix is just 1 byte
			memmove(flying_ptr + 1, flying_ptr + parser_req->prefixlen, obj->filemem + obj->fmemsize - (flying_ptr + parser_req->prefixlen));
			obj->fmemsize -= parser_req->prefixlen - 1;
			flying_suffix_after_tag -= parser_req->prefixlen - 1; // flying suffix should be updated after memmove()
		}
		*flying_ptr = '\0';
		if (parser_req->suffixlen > 1) {
			memmove(flying_suffix_after_tag + 1, flying_suffix_after_tag + parser_req->suffixlen, obj->filemem +
					obj->fmemsize - (flying_suffix_after_tag + parser_req->suffixlen));
			obj->fmemsize -= parser_req->suffixlen - 1;
		}
		tag_ptr = flying_ptr + 1; //printf("\nflying suffix after tag: %c", *flying_suffix_after_tag);
		*flying_suffix_after_tag = '\0';
		flying_ptr = flying_suffix_after_tag + 1;
		if (add_to_table(obj, tag_ptr, data_ptr) == false) return false;
		data_ptr = flying_ptr;
	}
	return true;
};

static int comparator(const void *p1, const void *p2) { // proudly copypasted, because I don't give a duck
	return strcasecmp(((tag_and_data *) p1)->tag.ptr, ((tag_and_data *) p2)->tag.ptr);
}

bool prepare_tags(hslice_obj *obj) {
	obj->tags = calloc((size_t ) obj->parsed_strings + 1, sizeof(void *));
	if (obj->tags == NULL) {
		fprintf(stderr, "%s\n", "Failed to allocate memory for tag list.");
		obj->parsed_strings = -1;
		return false;
	}
	obj->tags[obj->parsed_strings] = NULL; // NULL for last element
	ssize_t i = 0; // I don't want to typecast something below
	while (i < obj->parsed_strings) {
		obj->tags[i] = obj->table[i].tag.ptr;
		i++;
	}
	return true;
}

void hslice_parse(hslice_obj *obj, const char *prefix, const char *suffix) {
	if (obj->parsed_strings < 0) return;
	parser_internal parser_req = {.prefix = prefix, .suffix = suffix};
	if (parser_preparations(obj, &parser_req) == false) return; // goto Error not needed, because parser_req.prefix_and_suffix should not be free()'d
	if (parse(obj, &parser_req) == false) goto Error;
	obj->filemem = realloc(obj->filemem, obj->fmemsize); // Memory usage has been reduced because of memmove()'s. So, I suppose we should not care about return value too much
	if (obj->filemem == NULL) {
		fprintf(stderr, "%s\n", "Huge shit happens. ENOMEM? Exiting...\n");
		exit(EXIT_FAILURE); // that's only 1 place where we gonna exit instead of return error. If your system can't realloc even this - u're in big trouble
	}
	modify_seeks_to_pointers(obj);
	if (prepare_tags(obj) == false) goto Error; // FALLUS IN FRONTALUS — MORTE MOMENTALUS
	qsort(obj->table, (size_t) obj->parsed_strings, sizeof(tag_and_data), comparator);
	Error:
	free(parser_req.prefix_and_suffix); // prefix and suffix aren't needed anymore
	free(parser_req.prefix_noskip_suffix);
}

int hslice_count(hslice_obj *obj) {
	return (int) obj->parsed_strings; // TODO: should I just change type in structure to regular integer? I'll think about it later
}

tag_and_data *hslice_return_full(hslice_obj *obj, const char *search) {
	if (obj->parsed_strings < 1) return NULL;
	int position;
	int begin = 0;
	int end = (int) obj->parsed_strings - 1;
	int cond = 0;

	while (begin <= end) { // https://github.com/xdevelnet/weirdness/blob/master/string_sort_and_binary_search.c?ts=4#L27
		position = (begin + end) / 2;
		if ((cond = strcasecmp(obj->table[position].tag.ptr, search)) == 0) return &(obj->table[position]);
		else if (cond < 0) begin = position + 1;
		else end = position - 1;
	}
	return NULL;
}

const char *hslice_return(hslice_obj *obj, const char *search) { // const because idiot_protection_system = ON
	tag_and_data *ret = hslice_return_full(obj, search);
	if (ret == NULL) return NULL;
	return hslice_return_full(obj, search)->data.ptr;
}

const char *hslice_return_e(hslice_obj *obj, char *search) {
	const char *retval = hslice_return(obj, search);
	if (retval == NULL) return empty_string;
	return retval;
}

char **hslice_tags(hslice_obj *obj) {
	return obj->tags;
}

void hslice_close(hslice_obj *obj) {
	free(obj->filemem), free(obj->table), free(obj->tags); // AHAHAHAH, DID YOU GET IT?!
}

