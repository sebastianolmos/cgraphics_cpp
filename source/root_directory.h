/*
	This field was automatically created with CMake please don't modify it
*/
#pragma once

#include <filesystem>

static const char * const source_directory = "D:/projects/cgraphics_cpp/";

static std::filesystem::path getPath(const std::string &relative_path){
	return source_directory + relative_path;
}
	
