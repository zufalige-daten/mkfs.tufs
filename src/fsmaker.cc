#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <main.h>
#include <tufs.h>
#include <fsmaker.h>
#include <string.h>

using namespace std;

void writesect(uint64_t lba, void *buffer){
	fseeko64(output_file, lba*512, SEEK_SET);
	uint8_t *char_s = (uint8_t *)buffer;
	for(int i = 0; i < 512; i++){
		fputc(char_s[i], output_file);
	}
}

void readsect(uint64_t lba, void *buffer){
	fseeko64(output_file, lba*512, SEEK_SET);
	uint8_t *char_s = (uint8_t *)buffer;
	for(int i = 0; i < 512; i++){
		char_s[i] = fgetc(output_file);
	}
}

uint64_t filename_hash(const char *filename){
	uint64_t ret = 0;
	uint64_t highorder = 0;
	for(uint64_t i = 0; i < strlen(filename); i++){
		highorder = ret & 0xf8000000;
		ret <<= 5;
		ret ^= (highorder >> 27);
		ret ^= (uint64_t)filename[i];
	}
	return ret;
}

TUFS1HEADER header;

vector<string> pathsplit(string path){
	vector<string> ret;
	string temp = "";
	for(char chr : path){
		if(chr == '/'){
			ret.push_back(temp);
			temp = "";
		}
		else{
			temp += chr;
		}
	}
	ret.push_back(temp);
	return ret;
}

uint64_t get_path_lba(string path){
	uint64_t ret;
	vector<string> segments = pathsplit(path);
	TUFS1FILEALLOC directory_entry;
	uint64_t current_lba = header.RDSect;
	if(path == "/"){
		return header.RDSect;
	}
	readsect(current_lba, &directory_entry);
	for(int seg = 1; seg < segments.size(); seg++){
		uint64_t segnamehash = filename_hash(segments[seg].c_str());
		uint64_t entrycount = directory_entry.FSIBytes;
		current_lba = directory_entry.FSSect;
		readsect(current_lba, &directory_entry);
		uint64_t entry = 0;
		while(directory_entry.FNHash != segnamehash){
			if(entry >= entrycount){
				cout << "ERROR: " << directory_entry.FNHash << "\n";
				exit(-1);
			}
			current_lba = directory_entry.NFIDSect;
			readsect(current_lba, &directory_entry);
			entry++;
		}
	}
	ret = current_lba;
	return ret;
}


uint64_t reserve_index = 0;

uint64_t get_free_sector(uint64_t from){
	TUFS1DATASECT temp;
	uint64_t ret;
	for(; from < (header.PSISect + header.PSSect); from++){
		readsect(from, &temp);
		if(temp.Used == used::unused){
			break;
		}
	}
	ret = from;
	return ret;
}

uint64_t reservesect(){
	uint64_t ret = header.LFSect;
	header.LFSect = get_free_sector(header.LFSect+1);
	return ret;
}

uint64_t alloc_file(string name, uint8_t attribs, uint8_t permit, string owner){
	uint64_t ret;
	uint64_t entry_lba = reservesect();
	TUFS1FILEALLOC file_entry;
	file_entry.Used = used::filealloc;
	file_entry.FAttributes = attribs;
	int32_t unixtime = time(NULL);
	file_entry.FLMTime = unixtime;
	file_entry.FLRTime = unixtime;
	file_entry.FCTime = unixtime;
	file_entry.FSIBytes = 0;
	file_entry.FSSect = 0;
	file_entry.UPerms = permit;
	file_entry.FNHash = filename_hash(name.c_str());
	file_entry.NFIDSect = 0;
	memcpy(file_entry.OGUsername, owner.c_str(), owner.size());
	for(int i = owner.size(); i < 16; i++){
		file_entry.OGUsername[i] = 0;
	}
	memcpy(file_entry.FName, name.c_str(), name.size());
	for(int i = name.size(); i < max_filenamesize; i++){
		file_entry.FName[i] = 0;
	}
	writesect(entry_lba, &file_entry);
	ret = entry_lba;
	return ret;
}

void add_dir_entry(string path, uint64_t file_entry_lba){
	uint64_t parent_lba = get_path_lba(path);
	TUFS1FILEALLOC parent;
	readsect(parent_lba, &parent);
	TUFS1FILEALLOC temp;
	if(parent.FSIBytes == 0){
		parent.FSIBytes = 1;
		parent.FSSect = file_entry_lba;
		writesect(parent_lba, &parent);
		return;
	}
	uint64_t old_lba = 0;
	uint64_t current_lba = parent.FSSect;
	for(uint64_t i = 0; i < parent.FSIBytes; i++){
		readsect(current_lba, &temp);
		old_lba = current_lba;
		current_lba = temp.NFIDSect;
	}
	parent.FSIBytes++;
	temp.NFIDSect = file_entry_lba;
	writesect(old_lba, &temp);
	writesect(parent_lba, &parent);
}

uint64_t get_file_size(FILE *file){
	uint64_t og = ftello64(file);
	fseeko64(file, 0, SEEK_END);
	uint64_t ret = ftello64(file);
	fseeko64(file, og, SEEK_SET);
	return ret;
}

void makefs(void){
	memcpy(header.Sig, "TUFS", 4);
	header.LFSect = dataregion_lba;
	header.PSISect = partition_size/512;
	header.PSSect = partition_start;
	memcpy(header.PName, volume_label.c_str(), volume_label.size());
	for(int i = volume_label.size(); i < 11; i++){
		header.PName[i] = 0;
	}
	header.RSCount = dataregion_lba - partition_start;
	header.VNumber = 1;
	// file alloc
	reserve_index = dataregion_lba + 1;
	header.LFSect = get_free_sector(header.LFSect);
	header.RDSect = alloc_file("", attributes::directory, permissions::canread | permissions::isoverride, "root");
	TUFS1FILEALLOC tempd;
	readsect(header.RDSect, &tempd);
	tempd.PSect = header.RDSect;
	writesect(header.RDSect, &tempd);
	uint64_t dotdotfile = alloc_file("..", attributes::directory, permissions::canread | permissions::isoverride, "root");
	uint64_t dotfile = alloc_file(".", attributes::directory, permissions::canread | permissions::isoverride, "root");
	readsect(dotfile, &tempd);
	TUFS1FILEALLOC dir_alloc;
	readsect(header.RDSect, &dir_alloc);
	tempd.PSect = header.RDSect;
	tempd.FSIBytes = dir_alloc.FSIBytes;
	tempd.FSSect = dotfile;
	writesect(dotfile, &tempd);
	readsect(dotdotfile, &tempd);
	tempd.PSect = header.RDSect;
	tempd.FSIBytes = dir_alloc.FSIBytes;
	tempd.FSSect = dotfile;
	writesect(dotdotfile, &tempd);
	add_dir_entry("/", dotdotfile);
	add_dir_entry("/", dotfile);
	for(file_t directory : input_directories){
		cout << "+ " << directory.path << "/ :: " << directory.name << "\n";
		uint64_t dir_lba = alloc_file(directory.name, attributes::directory, permissions::canread | permissions::isoverride, "root");
		add_dir_entry(directory.path.substr(0, directory.path.find_last_of("/")), dir_lba);
		uint64_t parent_lba = get_path_lba(directory.path.substr(0, directory.path.find_last_of("/")));
		readsect(dir_lba, &tempd);
		tempd.PSect = parent_lba;
		writesect(dir_lba, &tempd);
		uint64_t dotdotfile = alloc_file("..", attributes::directory, permissions::canread | permissions::isoverride, "root");
		uint64_t dotfile = alloc_file(".", attributes::directory, permissions::canread | permissions::isoverride, "root");
		readsect(dotfile, &tempd);
		readsect(dir_lba, &dir_alloc);
		tempd.PSect = dir_lba;
		tempd.FSIBytes = dir_alloc.FSIBytes;
		tempd.FSSect = dotfile;
		writesect(dotfile, &tempd);
		readsect(dotdotfile, &tempd);
		tempd.PSect = dir_lba;
		readsect(dir_alloc.PSect, &dir_alloc);
		tempd.FSIBytes = dir_alloc.FSIBytes;
		tempd.FSSect = dir_lba;
		writesect(dotdotfile, &tempd);
		add_dir_entry(directory.path, dotdotfile);
		add_dir_entry(directory.path, dotfile);
	}
	for(file_t file : input_files){
		cout << "+ " << file.path << " :: " << file.name << "\n";
		uint64_t file_lba = alloc_file(file.name, 0, permissions::canread, "root");
		add_dir_entry(file.path.substr(0, file.path.find_last_of("/")), file_lba);
		uint64_t fsiz = get_file_size(file.handle);
		uint64_t switchover = fsiz / 503;
		uint64_t extracount = fsiz % 503;
		uint64_t nosects = switchover + ((extracount > 0) ? 1 : 0);
		TUFS1FILEALLOC file_allocated;
		readsect(file_lba, &file_allocated);
		file_allocated.FSIBytes = fsiz;
		fseeko64(file.handle, 0, SEEK_SET);
		TUFS1DATASECT temp;
		vector<TUFS1DATASECT> sectors;
		vector<uint64_t> lbas;
		if(fsiz != 0){
			temp.Used = used::datasect;
			uint64_t c_lba = reservesect();
			file_allocated.FSSect = c_lba;
			lbas.push_back(c_lba);
			for(uint64_t i = 0; i < 503; i++){
				temp.FRFData[i] = fgetc(file.handle);
			}
			sectors.push_back(temp);
			for(uint64_t sect = 1; sect < nosects; sect++){
				temp.Used = used::datasect;
				if(sect != switchover){
					c_lba = reservesect();
					sectors[sect - 1].NFRSect = c_lba;
					lbas.push_back(c_lba);
					for(uint64_t i = 0; i < 503; i++){
						temp.FRFData[i] = fgetc(file.handle);
					}
					sectors.push_back(temp);
				}
				else{
					c_lba = reservesect();
					sectors[sect - 1].NFRSect = c_lba;
					lbas.push_back(c_lba);
					for(uint64_t i = 0; i < extracount; i++){
						temp.FRFData[i] = fgetc(file.handle);
					}
					for(uint64_t i = extracount - 1; i < 503; i++){
						temp.FRFData[i] = 0;
					}
					sectors.push_back(temp);
				}
			}
			for(uint64_t sect = 0; sect < nosects; sect++){
				writesect(lbas[sect], &sectors[sect]);
			}
			writesect(file_lba, &file_allocated);
		}
		fclose(file.handle);
		uint64_t parent_lba = get_path_lba(file.path.substr(0, file.path.find_last_of("/")));
		readsect(file_lba, &tempd);
		tempd.PSect = parent_lba;
		writesect(file_lba, &tempd);
	}
	// header
	writesect(fsheader_lba, &header);
}

