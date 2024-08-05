#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <main.h>
#include <argsparser.h>

using namespace std;

typedef struct{
	vector<string> files;
	vector<string> directories;
} lsparse_t;

string ls(string dirname){
	string ret = "";
	FILE *fpipe;
	string command = "ls -A '" + dirname + "'";
	if((fpipe = (FILE *)popen(command.c_str(), "r")) == 0){
		perror("Listing failed");
		exit(-1);
	}
	char c = 0;
	while(fread(&c, sizeof(char), 1, fpipe)){
		ret += c;
	}
	pclose(fpipe);
	return ret;
}

lsparse_t list_directory_ls(string dirname){
	lsparse_t ret;
	string result = ls(dirname);
	vector<string> toks;
	boost::split(toks, result, boost::is_any_of("\n"));
	if(toks.size() > 0){
		if(toks[0] == dirname){
			ret.files.push_back(dirname);
			return ret;
		}
	}
	ret.directories.push_back(dirname);
	for(string entry : toks){
		if(entry != ""){
			lsparse_t extra =  list_directory_ls(dirname + "/" + entry);
			for(string file : extra.files){
				ret.files.push_back(file);
			}
			for(string directory : extra.directories){
				ret.directories.push_back(directory);
			}
		}
	}
	return ret;
}

void expect(bool cond, string msg){
	if(!cond){
		cout << "ERROR: " << msg << "\n";
		exit(-1);
	}
}


/*
extern FILE *output_file;
extern vector<file_t> input_files;
extern vector<file_t> input_directories;
extern uint64_t partition_start;
extern uint64_t partition_size;
extern uint64_t fsheader_lba;
extern uint64_t dataregion_lba;
extern string volume_label;
*/

void parseargs(int argc, char **argv){
	for(int i = 1; i < argc; i++){
		if(string(argv[i]) == "-d"){
			i++;
			expect(i < argc, "At -d in arguments: expected directory name.");
			string dirname = string(argv[i]);
			lsparse_t result = list_directory_ls(string(dirname));
			for(string directory : result.directories){
				if(directory != dirname){
					file_t dir;
					dir.path = directory.substr(directory.find_first_of("/"), directory.size());
					dir.name = directory.substr(directory.find_last_of("/") + 1, directory.size());
					dir.handle = (FILE *)0;
					input_directories.push_back(dir);
				}
			}
			for(string file : result.files){
				file_t fi;
				fi.path = file.substr(file.find_first_of("/"), file.size());
				fi.name = file.substr(file.find_last_of("/") + 1, file.size());
				fi.handle = fopen(file.c_str(), "rb+");
				input_files.push_back(fi);
			}
		}
		else if(string(argv[i]) == "-s"){
			i++;
			expect(i < argc, "At -s in arguments: expected a partition size. ([size][unit { b | k | m | g }])");
			uint64_t size = stoull(string(argv[i]));
			expect(size > 0, "At -s in arguments: size must be larger than zero.");
			char unit = argv[i][strlen(argv[i]) - 1];
			expect(unit == 'b' || unit == 'k' || unit == 'm' || unit == 'g', "At -s in arguments: please use a valid unit. { b | k | m | g }");
			switch(unit){
				case 'b':
					break;
				case 'k':
					size *= 1024;
					break;
				case 'm':
					size *= 1024*1024;
					break;
				default:
					size *= 1024*1024*1024;
					break;
			}
			partition_size = size;
		}
		else if(string(argv[i]) == "-S"){
			i++;
			expect(i < argc, "At -S in arguments: expected a partition starting sector.");
			uint64_t st = stoull(string(argv[i]));
			partition_start = st;
			fsheader_lba = st + 1;
		}
		else if(string(argv[i]) == "-r"){
			i++;
			expect(i < argc, "At -r in arguments: expected a reserved sector count.");
			uint64_t count = stoull(string(argv[i]));
			expect(count >= 2, "At -r in arguments: reserved sector count must be greater than or equal to 2.");
			dataregion_lba = partition_start + count;
		}
		else if(string(argv[i]) == "-l"){
			i++;
			expect(i < argc, "At -l in arguments: expected a volume label.");
			volume_label = string(argv[i]);
		}
		else{
			output_file = fopen(argv[i], "rb+");
			expect(output_file != (FILE *)0, "Cannot open file '" + string(argv[i]) + "'.");
		}
	}
}

