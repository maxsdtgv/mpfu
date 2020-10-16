#include "fw_converter.h"
using namespace std;


void fwConvert16(char* inFilename, char* outFilename){
	char absolute_path[64] = {};
	string str;
	char hex_buffer[64] = {};
	char tmp_buffer[8] = {};
	int bytes_num = 0;
	int addr = 0;

	ifstream inputfile;
	ofstream outfile;


	realpath(inFilename, absolute_path);
	printf("Path %s", absolute_path);

	inputfile.open(absolute_path);


  while (getline(inputfile, str)) {
  	strcpy(hex_buffer, str.c_str());

  	strncpy(tmp_buffer, hex_buffer + 1, 2);

	bytes_num = stoi (tmp_buffer, nullptr, 16);

	printf("Path %s \n", str.c_str());
    printf("Path %i \n", bytes_num);
  }



return;

}

