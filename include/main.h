#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

typedef struct{
	FILE *handle;
	string name;
	string path;
} file_t;

extern FILE *output_file;
extern vector<file_t> input_files;
extern vector<file_t> input_directories;
extern uint64_t partition_start;
extern uint64_t partition_size;
extern uint64_t fsheader_lba;
extern uint64_t dataregion_lba;
extern string volume_label;
