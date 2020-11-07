#include "fw_converter.h"
using namespace std;


void fwConvertPic16F1xxx(char* inFilename, char* outFilename){
	char inFilenameAbsolutePath[64] = {};
	char outFilenameAbsolutePath[64] = {};
	string str;
	char hex_buffer[43+1] = {};
	char tmp_buffer[4+1] = {};
	int lineBytesNum = 0;
	int lineType = 0;
	int lineAddr = 0;
	int highAddr = 0;
	int lastPointer = 0;
	char lineData[32] = {};		

	int summaryBytesNum = 0;
	int summaryAddr = 0;
	char summaryLine[128+1] = {};

	ifstream inputFile;
	ofstream outFile;

	realpath(inFilename, inFilenameAbsolutePath);
	printf("Path in %s\n", inFilenameAbsolutePath);
	inputFile.open(inFilenameAbsolutePath);

	realpath(outFilename, outFilenameAbsolutePath);
	printf("Path out %s\n", outFilenameAbsolutePath);
	outFile.open(outFilenameAbsolutePath);


  	bool newLine = false;

  while (getline(inputFile, str)) {
	memset (hex_buffer, 0, sizeof(hex_buffer));
 	strcpy(hex_buffer, str.c_str()); //char * strcpy( char * destptr, const char * srcptr );



	memset (tmp_buffer, 0, sizeof(tmp_buffer));
  	strncpy(tmp_buffer, hex_buffer + 1, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
	lineBytesNum = stoi (tmp_buffer, nullptr, 16); //int stoi( const std::string& str, std::size_t* pos = 0, int base = 10 );

	memset (tmp_buffer, 0, sizeof(tmp_buffer));
  	strncpy(tmp_buffer, hex_buffer + 3, 4); //char * strncpy( char * destptr, const char * srcptr, size_t num );
	lineAddr = stol (tmp_buffer, nullptr, 16); 

	memset (tmp_buffer, 0, sizeof(tmp_buffer));
  	strncpy(tmp_buffer, hex_buffer + 7, 2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
	lineType = stoi (tmp_buffer, nullptr, 16); //int stoi( const std::string& str, std::size_t* pos = 0, int base = 10 );

	memset (lineData, 0, sizeof(lineData));
  	strncpy(lineData, hex_buffer + 9, lineBytesNum*2); //char * strncpy( char * destptr, const char * srcptr, size_t num );

  	switch (lineType){
  		case 4:
  			highAddr = stol (lineData, nullptr, 16);
  			printf("highAddr %i \n", highAddr);
  			newLine = true;
  			break;
  		case 0:
  			if (highAddr == 0){
 				if ( ((lastPointer >= 128) | (summaryAddr*2 + lastPointer/2 != lineAddr)) & (!newLine) ) {
 					printf("====== Write %i %i %s \n",lastPointer/2, summaryAddr, summaryLine);
  					lastPointer = 0;
  					summaryAddr = lineAddr/2;
  					memset (summaryLine, 0, sizeof(summaryLine));
 				}
	 				strncpy(summaryLine + lastPointer, lineData, lineBytesNum*2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
	 				lastPointer += lineBytesNum*2;
	 				newLine = false;
  			}
  			break;
  		case 1:
			printf("====== Write %i %i %s \n",lastPointer/2, summaryAddr, summaryLine);
  			break;
  	}


printf("%s \n", hex_buffer);
printf("     ");



//    printf("Num %i ", lineBytesNum);     
//    printf("Addr %i ", lineAddr);
//    printf("Type %i ", lineType);
//    printf("Data %s \n", lineData);




  }

}

