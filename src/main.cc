#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <main.h>
#include <fsmaker.h>
#include <argsparser.h>

using namespace std;

FILE *output_file;
vector<file_t> input_files;
vector<file_t> input_directories;
uint64_t partition_start = 0;
uint64_t partition_size;
uint64_t fsheader_lba = 1;
uint64_t dataregion_lba = 2;
string volume_label = "NO LABEL";

int main(int argc, char **argv){
	parseargs(argc, argv);
	expect(output_file != (FILE *)0, "Storage file not specified.");
	makefs();
	fclose(output_file);
	return 0;
}

