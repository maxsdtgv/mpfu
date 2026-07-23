#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <cstring>
#include "uart_procedures.h"

void fwConvertPic16F1xxx(char*, char*);
int  buildEepromImage(const char* inFilename, const char* outFilename);