#pragma once
#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <Windows.h>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <future>

#include "logger.h"


using namespace std;

class File {
	ifstream is{};
	ofstream os{};
	filesystem::path m_path{};
	Logger& m_Logger;
	vector<vector<string>> m_data;
public:
	//Constructor - fileName - file name to be opened
	//logger - reference to the logger object
	//fileName - file name to be opened
	File(Logger& logger, const string &fileName);
	
	//Function to open the input file
	//Open the input file
	bool OpenFile();
	
	//Function to get the size of the file
	//Return the size of the file
	size_t Size();
	
	//Function to check if the input file is open
	//Return true if the input file is open
	bool isOpen();
	
	//Function to read data from the file
	//Read data from the file
	//buffer - string to store the read data
	void readData(string &buffer);
	
	//Function to write data to the file
	//buffer - string to write to the file
	size_t writeData(string buffer);
	
	//Function to write data to a specific file
	//fileneame - file name to write data to
	void writeFile(const string& filename);
	
	//Function to close the input file
	//Open the input file
	void closeInputFile();
	
	
	//Function to close the output file
	//Close the output file
	void closeOutputFile();
	
	//Function to get the file name
	//Return the file name
	string getFileName();
	
	//Function to check if is end of file
	//Return true if is end of file
	bool eof();
	
	//Function to clear the data read from the file
	//Clean the data read from the file
	void cleanData();
	
	//Fundion to push data read from the file
	//Return the data read from the file
	vector<vector<string>>& getData();
	
	//Function to sort data by a specific field
	//fieldSort - field to sort by
	void sortData(const unsigned int fieldSort);
	
	//Function to sort data in parallel using threads
	//data - data in witch the sort will be performed	
	//fieldToSort - field to sort by
	void sortDataParallel(vector<vector<string>>& data, const unsigned int fieldToSort);
	
	//Function to sort data using binary sort
	//data - data to be sorted
	//fieldToSort - field to sort by
	void sortDataBinary(vector<vector<string>>& data, const unsigned int fieldToSort);
	
	//Function to compare two vectors of strings by a specific field
	//data - data to be sorted
	//vector<string> - vector of strings to be compared	
	//field - field to sort by
	bool compare(const vector<string>& begin, const vector<string>& end, const unsigned int field);
	
	//Function to perform binary insertion sort
	//data - data to be sorted
	//field - field to sort by
	void binarySort(vector<vector<string>>& data, const unsigned int field);
	
	//Function to split a string by a delimiter
	//str - string to be split
	//delimiter - delimiter to split the string
	static vector<string>splitData(const string& str, char delimiter);
	
	//Function to load data from the file
	//Load data from file
	bool loadFileData();
	
	//Function to push data to the file
	//pash - vector of strings to be pushed to the file
	//data - data to be pushed to the file
	void push(const vector<string> &data);
	
	//Function to find data in a vector of vectors of strings
	//vector<vectort<sting>> data �n witch the search it will be performed
	//matchData - data to search
	//fieldToSearch - field to search in data	
	//doneFlag - flag to stop searching
	vector<vector<string>> findData(const vector<vector<string>>& data, const string& matchData, unsigned int fieldToSearch, atomic<bool>& doneFlag);
	
	//Function to search data using find_if
	//data - data in witch the search will be performed
	//matchData - data to search
	//fieldToSearch - field to search in data
	vector<vector<string>> serachData_find_if(const vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch);
	
	//Function to search data using find_if in parallel
	//data - data in witch the search will be performed
	//matchData - data to search
	//fieldToSearch - field to search in data
	vector<vector<string>> searchDataParallel_find_if(const vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch);
	
	//Function to search data using find_if in parallel (version 1)
	//data - data in witch the search will be performed
	//matchData - data to search
	//fieldToSearch - field to search in data
	vector<vector<string>> searchDataParallel_find_if_v1(const vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch);
	
	//Function to search data using binary search
	//data - data in witch the search will be performed
	//matchData - data to search
	//fieldToSearch - field to search in data
	vector<vector<string>> searchDataBinary(const vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch);
	
	//Function to search data in parallel using threads
	//data - data in witch the search will be performed
	//matchData - data to search
	//fieldToSearch - field to search in data
	vector<vector<string>> seachDataParallel(vector<vector<string>> &data,const string& matchData, const unsigned int fieldToSearch);
	
	//Function to search data in parallel using async
	//data - data in witch the search will be performed
	//matchData - data to search
	//fieldToSearch - field to search in data
	vector<vector<string>> seachDataAsync_v1(vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch);
	
	//Function to search data in parallel using async (version 2)
	//data - data in witch the search will be performed
	//matchData - data to search
	//fieldToSearch - field to search in data
	vector<string> seachDataAsync_v2(vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch);
	
	//Function to find data in a vector of vectors of strings (version 1)
	//dataChunk - data chunk to search in
	//result - promise to return the result
	//matchData - data to search
	//fieldToSearch - field to search in data
	//doneFlag - flag to stop searching
	void findData_v1(vector<vector<string>> dataChunk, promise<vector<vector<string>>>& result, const string& matchData, unsigned int fieldToSearch, atomic<bool>* doneFlag);
	
	//Function to find data in a vector of vectors of strings (version 2)
	//dataChunk - data chunk to search in
	//matchData - data to search
	//fieldToSearch - field to search in data
	//doneFlag - flag to stop searching
	vector<string> findData_v2(const vector<vector<string>>& dataChunk, const string& matchData, unsigned int fieldToSearch, atomic<bool>* doneFlag);
};
