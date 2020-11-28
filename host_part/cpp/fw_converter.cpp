#include "fw_converter.h"
using namespace std;

void swapBytesInLine(char *LineToProceed, int LineSize){
  	char tmpByte[2] = {};
  			for (int i = 0; i < LineSize; i += 4){
  				strncpy(tmpByte, LineToProceed + i, 2);
  				strncpy(LineToProceed + i, LineToProceed + i + 2, 2);
  				strncpy(LineToProceed + i + 2, tmpByte, 2);
  			}
}

void WriteLineToFile(ofstream &outFile_fd, int lineSize, int Addr, char *LineToProceed){
	char hhex[4] = {};

	// Add pream to send to MCU
//	sprintf(hhex, "%02X", PREAM_TO_DEVICE);
//	outFile_fd.write(hhex, 2);

	// Add length of data + 4
	sprintf(hhex, "%02X", lineSize/2 + 4);
	outFile_fd.write(hhex, 2);

	// Add Command code
	sprintf(hhex, "%02X", WRITE_TO_MEM);
	outFile_fd.write(hhex, 2);

	// Add addr
	char ahex[8] = {};
	sprintf(ahex, "%04X", Addr);
	outFile_fd.write(ahex, 4);

	// Add data	
	strcpy(LineToProceed + lineSize, "\n");
	outFile_fd.write(LineToProceed, lineSize + 1);
}

void fwConvertPic16F1xxx(char* inFilename, char* outFilename){
	char inFilenameAbsolutePath[64] = {};
	char outFilenameAbsolutePath[64] = {};
	string str;
	char hex_buffer[43] = {};
	char tmp_buffer[4+1] = {};
	int lineBytesNum = 0;
	int lineType = 0;
	int lineAddr = 0;
	int highAddr = 0;
	int linePointer = 0;
	char lineData[32] = {};		

	int summaryAddr = 0;
	char summaryLine[127+2] = {};

	ifstream inputFile;
	ofstream outFile;

	realpath(inFilename, inFilenameAbsolutePath);
	printf("Path in %s\n", inFilenameAbsolutePath);
	inputFile.open(inFilenameAbsolutePath);

	realpath(outFilename, outFilenameAbsolutePath);
	printf("Path out %s\n", outFilenameAbsolutePath);
	outFile.open(outFilenameAbsolutePath, ios_base::trunc);



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
  			//printf("highAddr %i \n", highAddr);
  			newLine = true;
  			break;
  		case 0:
  			if (highAddr == 0){
 				if ( ((linePointer > 127) | (summaryAddr*2 + linePointer/2 != lineAddr)) & (!newLine) ) {
					swapBytesInLine(summaryLine, linePointer);
					WriteLineToFile(outFile, linePointer, summaryAddr, summaryLine);
 					//printf("=====case 4= Write %i %i %s\n",linePointer/2, summaryAddr, summaryLine);
  					linePointer = 0;
  					summaryAddr = lineAddr/2;
  					memset (summaryLine, 0, sizeof(summaryLine));
 				}
	 				strncpy(summaryLine + linePointer, lineData, lineBytesNum*2); //char * strncpy( char * destptr, const char * srcptr, size_t num );
	 				linePointer += lineBytesNum*2;
	 				newLine = false;
  			}
  			break;
  		case 1:
			swapBytesInLine(summaryLine, linePointer);
			WriteLineToFile(outFile, linePointer, summaryAddr, summaryLine);
			//printf("=====case 1= Write %i %i %s\n",linePointer/2, summaryAddr, summaryLine);
  			break;
  		default:
  			break;
  	}


printf("%s \n", hex_buffer);
printf("     ");



//    printf("Num %i ", lineBytesNum);     
//    printf("Addr %i ", lineAddr);
//    printf("Type %i ", lineType);
//    printf("Data %s \n", lineData);




  }
inputFile.close();
outFile.close();
}

