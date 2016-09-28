#ifndef HSLICE_TESTS_H
#define HSLICE_TESTS_H

#include <unistd.h>
#include <stdbool.h>
#include "libhslice.h"

// This test covers only defined behavior. Any kind of weird errors like ENOMEM aren't tested. Sorry about that (no).

char fname[] = "hslice_test_XXXXXX"; // do not apply const keyword. XXXXXX will be changed during mkstemp();

static void xxx_me_please(char *filename_template) { // should be used to set last 6 characters to XXXXXX
	const size_t xxx_length = 6; // read man mkstemp
	size_t length = strlen(filename_template);
	if (length < xxx_length) {
		fprintf(stderr, "Filename template for temporary file should contain at least 6 characters");
		exit(EXIT_FAILURE);
	}
	filename_template += length - xxx_length;
	memcpy(filename_template, "XXXXXX", xxx_length);
}

static void prepare_file_with_data(char *filename, const char *data, size_t datalen) {
	int fd;
	if (chdir("/tmp") < 0) {
		perror("Are you under UNIX-like OS?  /tmp");
		exit(EXIT_FAILURE);
	}
	fd = mkstemp(filename);
	if (fd < 0) {
		perror(filename);
		exit(EXIT_FAILURE);
	}
	write(fd, data, datalen);
	fsync(fd);
	close(fd);
}

bool hslice_open_test() {
	bool return_value = false;
	const char testdata[] = "Get the roll{Z_ROLL_} of the troll{Z_TROLL_}";
	prepare_file_with_data(fname, testdata, sizeof(testdata) - 1);
	hslice_obj test_object = hslice_open(fname);
	if (strcmp(test_object.filemem, testdata) == 0) return_value = true;
	hslice_close(&test_object);
	remove(fname);
	xxx_me_please(fname);
	return return_value;
}

bool is_behind_exist_test() {
	const char *test_str = "SAMEP ON THE ROAD BEHIN"; // Do not ask me about contents of this string. There is absolutely NO SENSE.
	if (is_behind_exist(test_str + 6, "EP ", test_str) and is_behind_exist(test_str + 6, "MEP ", test_str) and
		is_behind_exist(test_str + 6, "AMEP ", test_str) and is_behind_exist(test_str + 6, "SAMEP ", test_str) and
		!is_behind_exist(test_str + 6, "SAMEP ", test_str + 1) and is_behind_exist(test_str + 6, " ", test_str) )
		// TODO: I should write specified test cases and explain all of them in code instead of "Let's test random shit!"
		return true;
	return false;
}

bool ftag_test() {
	if (ftag("ABCDEFG", "EF", 2) != NULL and ftag("{WHAT_TAG_HERE_SA}" + 6, "_SA}", 4) != NULL and ftag("{TEST_TAG}" + 5, "}", 1) != NULL
		and ftag("JHD REAL EA" + 4, " EA", 3) != NULL) return true;
	return false;
}

bool modify_seeks_to_pointers_test() {
	char test_data[] = "a\0bbbbb\0cccccc\0ddddddddddd";
	tag_and_data test_dataset[2];
	test_dataset[0].tag.seek = 0; // test_dataset[0] = {.tag.seek = 2, .data.seek = 0};
	test_dataset[0].data.seek = 2; // why I cant initialize like this?
	test_dataset[1].tag.seek = 8; // And what's the reason that compiler throws an error about expected expression before "{"?
	test_dataset[1].data.seek = 15; // Why? Because fuck you, that's why.
	hslice_obj test_obj = {.table = test_dataset, .parsed_strings = 2, .filemem = test_data};
	modify_seeks_to_pointers(&test_obj);
	if (strcmp(test_obj.table[0].tag.ptr, "a") == 0 and strcmp(test_obj.table[0].data.ptr, "bbbbb") == 0 and
		strcmp(test_obj.table[1].tag.ptr, "cccccc") == 0 and strcmp(test_obj.table[1].data.ptr, "ddddddddddd") == 0)
		return true;
	return false;
}

bool add_to_table_test() {
	bool test_result = false;
	hslice_obj test_obj;
	test_obj.parsed_strings = 0;
	test_obj.tablesize = 3; // we gonna use realloc() at least once without crushing own face
	test_obj.table = calloc(3, sizeof(tag_and_data));
	test_obj.filemem = NULL; // check what's going on inside add_to_table() to understand why It's necessary. GOD SAVE THE QU^VALGRIND!

	add_to_table(&test_obj, "a", "AAAAAA"); // 0
	add_to_table(&test_obj, "b", "BBBBBBBB"); // 1
	add_to_table(&test_obj, "c", "CCCCCCCCCC"); // 2
	add_to_table(&test_obj, "d", "DDDDDDDDDDDDD"); // 3
	add_to_table(&test_obj, "e", "EEEEEEEEEEEEEEEEE"); // 4
	add_to_table(&test_obj, "f", "FFFFFFFFFFFFFFFFFFF"); // 5
	add_to_table(&test_obj, "g", "GGGGGGGGGGGGGGGGGGGGGGG"); // 6
	add_to_table(&test_obj, "h", "HHHHHHHHHHHHHHHHHHHHHHHHHHH"); // 7
	add_to_table(&test_obj, "i", "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII"); // 8
	add_to_table(&test_obj, "j", "JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ"); // 9

	modify_seeks_to_pointers(&test_obj);
	if (strcmp(test_obj.table[0].tag.ptr, "a") == 0 and strcmp(test_obj.table[0].data.ptr, "AAAAAA") == 0 and
		strcmp(test_obj.table[8].tag.ptr, "i") == 0 and strcmp(test_obj.table[8].data.ptr, "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII") == 0) test_result = true;
	free(test_obj.table);
	return test_result;
}

bool hslice_parse_moved_mem_test() {
	const char testdata[] = "Get the roll{Z_ROLL_} of the troll{Z_TROLL_}";
	prepare_file_with_data(fname, testdata, sizeof(testdata) - 1);
	hslice_obj test_object = hslice_open(fname);
	const char parsed[] = "Get the roll\0ROLL\0 of the troll\0TROLL\0";
	hslice_parse(&test_object, "{Z_", "_}");
	bool test_result = false;
	if (memcmp(test_object.filemem, parsed, sizeof(parsed) - 1) == 0 and // checking how data should be modified (in-place modification)
		strcmp(test_object.table[0].data.ptr, "Get the roll") == 0 and strcmp(test_object.table[0].tag.ptr, "ROLL") == 0 and // and how data is recorded to table
		strcmp(test_object.table[1].data.ptr, " of the troll") == 0 and strcmp(test_object.table[1].tag.ptr, "TROLL") == 0) test_result = true;

	hslice_close(&test_object);
	remove(fname);
	xxx_me_please(fname);
	return test_result;
}

static bool check_string_existance(char **list, size_t count, char *string) {
	size_t i = 0;
	while (i < count) {
		if (strcmp(list[i], string) == 0) return true;
		i++;
	}
	return false;
}

bool hslice_tags_test() {
	const char testdata[] = "Every cloud beyond the sky(A_bfg)"
			"Every place that I hide(A_lf)"
			"Tell me that I, I was wrong to let u go(A_lmnt)"
			"Every sound that I hear(A_nnn)"
			"Every force that I feel(A_AoE)"
			"Tell me that I, I was wrong, I was wrong to let u go(A_qwerty)";
	hslice_obj test_object = {.filemem = strdup(testdata), .fmemsize = sizeof(testdata)};
	hslice_parse(&test_object, "(A_", ")");
	bool retval = false;
	if (check_string_existance(test_object.tags, (size_t) test_object.parsed_strings, "bfg") == true and
		check_string_existance(test_object.tags, (size_t) test_object.parsed_strings, "lf") == true and
		check_string_existance(test_object.tags, (size_t) test_object.parsed_strings, "lmnt") == true and
		check_string_existance(test_object.tags, (size_t) test_object.parsed_strings, "nnn") == true and
		check_string_existance(test_object.tags, (size_t) test_object.parsed_strings, "AoE") == true) retval = true;
	hslice_close(&test_object);

	return retval;
}

bool hslice_parse_sorted_test() { // Tags should exist and be sorted alphabetically
	const char testdata[] = "What is love{p_love}\nBaby don't hurt me{p_baby}\nDon't hurt me{p_Hurt}\nNo more{p_more}\n";
	prepare_file_with_data(fname, testdata, sizeof(testdata) - 1);
	hslice_obj test_object = hslice_open(fname);
	hslice_parse(&test_object, "{p_", "}");

	bool test_result = false;
	if (strcmp(test_object.table[0].tag.ptr, "baby") == 0 and strcmp(test_object.table[1].tag.ptr, "Hurt") == 0 and
		strcmp(test_object.table[2].tag.ptr, "love") == 0 and strcmp(test_object.table[3].tag.ptr, "more") == 0)
		test_result = true;

	hslice_close(&test_object);
	remove(fname);
	xxx_me_please(fname);
	return test_result;
}

bool hslice_count_test() { // is this test really needed?
	hslice_obj test_object = {.parsed_strings = 777};
	if (hslice_count(&test_object) == 777) return true;
	return false;
}

bool hslice_return_test() {
	const char testdata[] = "I don't know why you're not fair{zv_zzzz}"
			"I give you my love, but you don't care{zv_cccc}"
			"So what is right and what is wrong?{zv_bbbb}"
			"Gimme a sign{zv_aaaa}";
	prepare_file_with_data(fname, testdata, sizeof(testdata) - 1);
	hslice_obj test_object = hslice_open(fname);
	hslice_parse(&test_object, "{zv_", "}");

	bool test_result = false;

	if (strcmp(hslice_return(&test_object, "aaaa"), "Gimme a sign") == 0 and
		strcmp(hslice_return(&test_object, "bbbb"), "So what is right and what is wrong?") == 0 and
		strcmp(hslice_return(&test_object, "cccc"), "I give you my love, but you don't care") == 0 and
		hslice_return(&test_object, "qwerty") == NULL and
		strcmp(hslice_return_e(&test_object, "dsa"), "") == 0)
		test_result = true;

	hslice_close(&test_object);
	remove(fname);
	xxx_me_please(fname);
	return test_result;
}

bool hslice_return_full_test() {
	const char testdata[] = "What da heck{q_W}is dat{q_e}";
	prepare_file_with_data(fname, testdata, sizeof(testdata) - 1);
	hslice_obj test_object = hslice_open(fname);
	hslice_parse(&test_object, "{q_", "}");

	bool test_result = true;

	struct {
		size_t datalength;
		char *tag;
		char *data;
	} anon;

	anon.data = test_object.filemem + 0;
	anon.tag = test_object.filemem + 13;
	anon.datalength = 12;
	if (memcmp(&anon, hslice_return_full(&test_object, "W"), sizeof(anon)) != 0) test_result = false;

	anon.data = test_object.filemem + 15;
	anon.tag = test_object.filemem + 22;
	anon.datalength = 6;
	if (memcmp(&anon, hslice_return_full(&test_object, "e"), sizeof(anon)) != 0) test_result = false;

	hslice_close(&test_object);
	remove(fname);
	xxx_me_please(fname);
	return test_result;
}

bool perform_test(char *test_name, bool(testfunc)()) {
	static bool test_result = false;
	if (test_name == NULL) return test_result;
	static size_t test_number = 0;
	printf("Test #%lu: %s - ", ++test_number, test_name);
	fflush(stdout);
	static const char *test_result_str = "Passed.";
	test_result = testfunc();
	if (test_result == false) test_result_str = "NOT passed. Stopped.";
	printf("%s\n", test_result_str);
	return test_result;
}

void run_tests() {
	do { // we should NOT execute further tests if one of them fails
		if (!perform_test("hslice_open", hslice_open_test)) break;
		if (!perform_test("is_behind_exist", is_behind_exist_test)) break;
		if (!perform_test("ftag", ftag_test)) break;
		if (!perform_test("hslice_count", hslice_count_test)) break;
		if (!perform_test("modify_seeks_to_pointers", modify_seeks_to_pointers_test)) break;
		if (!perform_test("add_to_table", add_to_table_test)) break;
		if (!perform_test("hslice_parse (moved mem)", hslice_parse_moved_mem_test)) break;
		if (!perform_test("hslice_parse (sorted tags)", hslice_parse_sorted_test)) break;
		if (!perform_test("hslice_tags_test", hslice_tags_test)) break;
		if (!perform_test("hslice_return", hslice_return_test)) break;
		if (!perform_test("hslice_return_full_test", hslice_return_full_test)) break;
	} while (0);
	if (perform_test(NULL, NULL) == true) exit(EXIT_SUCCESS); // get last test result
	exit(EXIT_FAILURE);
}

#endif
