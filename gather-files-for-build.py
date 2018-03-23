# 
# Pre-build script for example programs
# 
# This script creates a "build" subdirectory, and populates it with all the demo code
# from the "examples" directory plus all the library code from the "src" directory.
# 
# Run with no arguments, with the current working directory
# set to the script's directory. For Python 2 and 3.
# 
# Copyright (c) Project Nayuki. (MIT License)
# https://www.nayuki.io/page/pervasive-displays-epaper-panel-hardware-driver
# 

import os, re, sys


def main():
	# Subdirectory names
	exampledir = "example"
	librarydir = "src"
	outputdir = "build"
	
	# Check input directories, make output directory
	if not os.path.isdir(exampledir) or not os.path.isdir(librarydir):
		raise RuntimeError("Input directory does not exist")
	if not os.path.isdir(outputdir):
		os.mkdir(outputdir)
	
	# Handle each example
	for name in os.listdir(exampledir):
		subexample = os.path.join("example", name)
		if os.path.isdir(subexample):
			process_example_dir(subexample, librarydir, os.path.join(outputdir, name))


def process_example_dir(exampledir, librarydir, outputdir):
	# Check input directories, make output directory
	if not os.path.isdir(exampledir) or not os.path.isdir(librarydir):
		raise RuntimeError("Input directory does not exist")
	if not os.path.isdir(outputdir):
		os.mkdir(outputdir)
	
	# Gather potential input file names and paths
	inputfiles = []
	inputfiles.extend((name, os.path.join(exampledir, name)) for name in os.listdir(exampledir))
	inputfiles.extend((name, os.path.join(librarydir, name)) for name in os.listdir(librarydir))
	
	# Copy each source file to the output
	for (filename, inputpath) in inputfiles:
		if os.path.isfile(inputpath) and filename.endswith((".cpp", ".hpp", ".ino")):
			outputpath = os.path.join(outputdir, filename[ : -2] if filename.endswith(".hpp") else filename)
			copy_and_patch_code_file(inputpath, outputpath)


def copy_and_patch_code_file(inputpath, outputpath):
	encoding = "UTF-8"
	
	# Read entire file's text
	if sys.version_info.major == 2:
		with open(inputpath, "r") as fin:
			text = fin.read().decode(encoding)
	else:
		with open(inputpath, "rt", encoding=encoding, newline=None) as fin:
			text = fin.read()
	
	# Change every instance of '#include "xyz.hpp"' to '#include "xyz.h"'
	text = re.sub(r'(#include ".*?\.h)pp"', r'\1"', text)
	
	# Write entire file's text
	if sys.version_info.major == 2:
		with open(outputpath, "wb") as fout:
			fout.write(text.encode(encoding))
	else:
		with open(outputpath, "wt", encoding=encoding, newline="\n") as fout:
			fout.write(text)


if __name__ == "__main__":
	main()
