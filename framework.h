// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <strsafe.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <iostream>

#include "resource.h"

bool crash(const char* file, int line)
{
	std::cout << "Assertion failed in " << file << ":" << line << '\n';
	exit(-1);
}

// Yes, this leaves all file handles open
#define nassert(cond) (!!(cond) || crash(__FILE__, __LINE__))