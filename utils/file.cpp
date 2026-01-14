#include <future>
#include <execution>
#include <algorithm>
#include <vector>
#include <string>

#include "file.h"
#include "util.h"

File::File(Logger& logger, const string &fileName): m_Logger(logger) {
	m_path.assign(fileName);
}

bool File::OpenFile() {
	try {
		is.open(m_path.string(), ios::in | ios::binary);

		if (!is.is_open()) {
			logLastError(m_Logger, string("Unable to open file:" + m_path.string()), GetLastError());
			return false;
		}
		is.seekg(0);
	}
	catch (const exception& ex) {
		cout << ex.what() << "\n";
		logLastError(m_Logger, ex.what(), GetLastError());
		return false;
	}
	return true;
}
size_t File::Size() {
	return filesystem::file_size(m_path);
}
bool File::isOpen() {
	return is.is_open();
}
bool File::eof() {
	return is.eof();
}

void File::readData(string &buffer)
{
	char ch{ 0x0 };
	while (is.get(ch)) {
		if (ch == 0x0d) {
			is.get(ch);
			break;
		}
		else {
			buffer += ch;
		}
	}
}

size_t File::writeData(string buffer)
{
	if (!os.is_open()) {
		os.open(m_path.filename(), ios::out | ios::app);
	}
	os.write(buffer.c_str(),buffer.size());
	if (!os.good()) {
		throw runtime_error("Could not write");
		return 0;
	}

	return buffer.size();
}
// Function to write the sorted data to a file
void File::writeFile(const string& filename) {
	ofstream file(filename);
	for (const auto& row : m_data) {
		for (size_t i = 0; i < row.size(); ++i) {
			if (i > 0) file << ",";
			file << row[i];
		}
		file << "\n";
	}
	file.close();
}

void File::closeInputFile()
{
	if (is.is_open())
		is.close();
}

void File::closeOutputFile()
{
	if (os.is_open())
		os.close();
}

string File::getFileName()
{
	return m_path.string();
}

void File::cleanData() {
	m_data.erase(m_data.begin(), m_data.end());
}

void File::push(const vector<string>& data) {
	this->m_data.push_back(data);
}
vector<vector<string>>& File::getData() {
	return m_data;
}
vector<string> File::splitData(const string& buffer, const char delimiter) {
	vector<string> fields;
	stringstream ss(buffer);
	string field;
	while (getline(ss, field, delimiter)) {
		fields.push_back(field);
	}
	return fields;
}

bool File::loadFileData() {
	try {
		if (!OpenFile()) {
			return false;
		}

		m_Logger.log(LogLevel::Info, "Loading file data...");
		m_Logger.log(LogLevel::Info, "File name: {}", getFileName());
		m_Logger.log(LogLevel::Info, "File Size: {}", Size());

		while (!eof()) {
			vector<string> fields;
			string buffer;
			readData(buffer);
			fields = splitData(buffer, ',');
			push(fields);
		}
	}
	catch (exception& ex) {
		logLastError(m_Logger, ex.what(), GetLastError());
		return false;
	}
	return true;
}

// Function to perform binary search to find the correct index
int binary_search(const vector<int>& arr, int key, int right) {
	int left = 0;
	while (left <= right) {
		int mid = left + (right - left) / 2;
		if (arr[mid] < key) {
			left = mid + 1;
		}
		else {
			right = mid - 1;
		}
	}
	return left; // Return the index where the key should be inserted
}

// Function to perform binary insertion sort
void binary_insertion_sort(vector<int>& arr) {
	for (int i = 1; i < arr.size(); ++i) {
		int key = arr[i];
		// Find the position to insert the key
		int j = i - 1;
		int pos = binary_search(arr, key, j);

		// Move elements to make space for the key
		while (j >= pos) {
			arr[j + 1] = arr[j];
			j--;
		}
		arr[pos] = key; // Insert the key at its correct position
	}
}

void sortVector() {
	vector<int> numbers = { 5, 1, 4, 2, 8 };

	// Perform binary insertion sort
	binary_insertion_sort(numbers);
}

bool File::compare(const vector<string>& begin, const vector<string>& end, const unsigned int field) {
	return begin[field] < end[field];
}

// Function to perform binary insertion sort
void File::binarySort(vector<vector<string>>& data, const unsigned int field) {
	sort(data.begin(), data.end(), [&](vector<string>& begin, vector<string>& end) {
		if (begin.empty() || end.empty()) {
			return false;
		}
		return compare(begin, end, field);
	});
}


void File::sortDataBinary(vector<vector<string>>& data, const unsigned int fieldToSort) {
	binarySort(data, fieldToSort);
}


// Sort data by field
void File::sortData(unsigned int fieldSort) {
	sort(m_data.begin(), m_data.end(), [&fieldSort](const vector<string>& begin, const vector<string>& end) {
		if (begin.empty() || end.empty()) {
			return false;
		}
		return begin[fieldSort] < end[fieldSort]; 
	});
}
//Sort data parallel by field
void File::sortDataParallel(vector<vector<string>> &dataToSort, const unsigned int fieldToSort) {
	
	auto pivot = dataToSort.size() / 2;
	auto leftSort = async(launch::async, [&]() {
		sort(dataToSort.begin(), dataToSort.begin() + pivot, [&fieldToSort](const auto& first, const auto& last) {
			if (first.empty() || last.empty()) {
				return false;
			}
			return first[fieldToSort] < last[fieldToSort];
		});
	});

	auto rightSort = async(launch::async, [&]() {
		sort(dataToSort.begin() + pivot, dataToSort.end(), [&](const auto& first, const auto& last) {
			if (first.empty() || last.empty()) {
				return false;
			}
			return first[fieldToSort] < last[fieldToSort];
		});
	});

	leftSort.wait();
	rightSort.wait();

	inplace_merge(execution::seq, dataToSort.begin(), dataToSort.begin() + pivot, dataToSort.end(), [&](const auto& first, const auto& last) {
		if (first.empty() || last.empty()) {
			return false;
		}
		return first[fieldToSort] < last[fieldToSort];
	});
}

vector<vector<string>> File::serachData_find_if(const vector<vector<string>> &data, const string& matchData, const unsigned int fieldToSearch) {
	if (data.empty()) {
		cout << "No data to search." << endl;
		return {};
	}
	if (fieldToSearch <= 0 || fieldToSearch > data[0].size()) {
		return {};
	}

	auto it = find_if(data.begin(), data.end(), [&matchData, &fieldToSearch](const auto& entry) {
		return entry.size() > 0 && entry[fieldToSearch - 1] == matchData;
	});
	if (it != data.end()) {
		return { *it };
	}
	return {};
}

vector<vector<string>> File::searchDataParallel_find_if(const vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch) {
	if (data.empty()) {
		cout << "No data to search." << endl;
		return {};
	}
	const size_t hardwareThread = thread::hardware_concurrency();
	size_t blockSize = data.size() / hardwareThread;
	atomic<bool> doneFlag{ false };	
	vector<future<vector<vector<string>>>> result;		
	auto start = data.begin();

	for (size_t i = 0; i < hardwareThread; ++i) {
		if (doneFlag.load()) {
 			break;
		}
		auto end = (i == hardwareThread - 1) ? data.end() : next(start, blockSize);
		vector<vector<string>> dataChunk(start, end);
		result.emplace_back(async(launch::async, [&dataChunk, &matchData, &fieldToSearch, &doneFlag]() {
			vector<vector<string>> foundData;
			auto it = find_if(dataChunk.begin(), dataChunk.end(), [&matchData, &fieldToSearch, &doneFlag](const auto& entry) {
				if (doneFlag.load()) {
					return false;
				}	
				return entry.size() > 0 && entry[fieldToSearch - 1] == matchData;
			});
			if (it != dataChunk.end()) {
				doneFlag.store(true);
				foundData.push_back(*it);
			}
			return foundData;
		}));

		start = end; // Move to the next chunk
	}

	for (auto& ref : result) {
		const auto feature = ref.get();
		if (!feature.empty()) {
			return feature;
		}
	}
	return {};
}

vector<vector<string>> File::searchDataParallel_find_if_v1(const vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch) {
	if (data.empty()) {
		cout << "No data to search." << endl;
		return {};
	}
	const size_t blockSize = data.size() / 2;

	atomic<bool> doneFlag{ false };
	vector<future<vector<vector<string>>>> result;
	vector<vector<string>> left(data.begin(), next(data.begin(), blockSize));
	vector<vector<string>> right(data.begin() + blockSize, data.end());
	//cout << "data.size():" << data.size() << "\n";
	//cout << "left.size():" << left.size() << "\n";		
	//cout << "right.size():" << right.size() << "\n";

	result.emplace_back(async(launch::async, [this, &left, &matchData, &fieldToSearch, &doneFlag]() {
		auto ret = findData(left, matchData, fieldToSearch, doneFlag);
		return ret;
	}));

	result.emplace_back(async(launch::async, [this, &right, &matchData, &fieldToSearch, &doneFlag]() {
		return findData(right, matchData, fieldToSearch, doneFlag);
	}));

	for (auto& ref : result) {
		const auto feature = ref.get();
		if (!feature.empty()) {
			return feature;
		}
	}
	return {};
}

vector<vector<string>>File::findData(const vector<vector<string>>& data, const string& matchData, unsigned int fieldToSearch, atomic<bool>& doneFlag) {
	int count{ 0 };
	const auto it = find_if(data.begin(), data.end(), [&matchData, &fieldToSearch, &doneFlag, &count](const auto& entry) {
		count++;
		if (doneFlag.load()) {
			return false;
		}
		return entry.size() > 0 && entry[fieldToSearch - 1] == matchData;
			
	});
	//cout << "count:" << count << " Thread ID:" << this_thread::get_id() << "\n";
	if (!doneFlag.load() && it != data.end()) {
		doneFlag.store(true);
		return { *it };
	}
	return vector<vector<string>>{};
}

vector<vector<string>> File::searchDataBinary(const vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch) {
	vector<vector<string>> foundData{};
	if (data.empty()) {
		cout << "No more data to search. Returning foundData" << endl;
		return foundData;
	}

	if (fieldToSearch == 0 || fieldToSearch > data[0].size()) {
		cout << "Invalid field index." << endl;
		return {};
	}
	//matchData.erase(remove(matchData.begin(), matchData.end(), ' '), matchData.end());
	const auto middle = data.size() / 2;
	const auto it = data.begin() + middle;
	if ((*it)[fieldToSearch -1] == matchData) {
		foundData.push_back(*it);
	}
	if ((*it)[fieldToSearch -1] > matchData){
		vector<vector<string>> lower(data.begin(), data.begin() + middle);
		auto lowerResults = searchDataBinary(lower, matchData, fieldToSearch);
		foundData.insert(foundData.end(),lowerResults.begin(), lowerResults.end());

	}
	else {
		vector<vector<string>> upper(data.begin() + middle + 1, data.end());
		auto upperResults = searchDataBinary(upper, matchData, fieldToSearch);
		foundData.insert(foundData.end(),upperResults.begin(), upperResults.end());
	}
	return foundData;
}

vector<vector<string>> File::seachDataAsync_v1(vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch) {
	const size_t hardwareThread = thread::hardware_concurrency();
	const size_t blockSize = data.size() / hardwareThread;
	atomic<bool> doneFlag{ false };
	vector<future<vector<vector<string>>>> result;

	auto start = data.begin();

	{
		for (size_t i = 0; i < hardwareThread; ++i) {
			if (doneFlag.load()) {
				break;
			}
			auto end = (i == hardwareThread - 1) ? data.end() : next(start, blockSize);
			vector<vector<string>> dataChunk(start, end);
			result.emplace_back(async(launch::async, [this, dataChunk, &matchData, fieldToSearch, &doneFlag]() {
				vector<vector<string>> foundData;
				for (const auto& row : dataChunk) {
					if (doneFlag.load()) {
						return foundData;
					}
					if (row[fieldToSearch - 1] == matchData) {
						doneFlag.store(true);
						foundData.push_back(row);
						return foundData;
					}
				}
				return foundData;
				}));
			start = end; // Move to the next chunk
		}
	}

	for (auto& ref : result) {
		auto feature = ref.get();
		if (!feature.empty()) {
			return feature;
		}
	}
	return {};
}

vector<vector<string>> File::seachDataParallel(vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch) {
	const size_t hardwareThread = thread::hardware_concurrency();
	const size_t blockSize = data.size() / hardwareThread;
	atomic<bool> doneFlag{ false };
	vector<promise<vector<vector<string>>>> result;

	struct joinThreads {
		vector<thread> threads;
		explicit joinThreads(vector<thread>& mthreads) : threads(move(mthreads)) {
		}

		~joinThreads() {
			for (size_t i = 0; i < threads.size(); i++) {
				if (threads[i].joinable()) {
					threads[i].join();
				}
			}
		}
	};

	vector<thread> threads;

	auto start = data.begin();

	{
		for (size_t i = 0; i < hardwareThread; ++i) {
			//cout << "Count:" << i + 1 << "\n";
			if (doneFlag.load()) {
				break;
			}
			result.emplace_back(promise<vector<vector<string>>>());
			auto end = (i == hardwareThread) ? data.end() : next(start, blockSize);
			vector<vector<string>> dataChunk(start, end);
			threads.emplace_back(&File::findData_v1, this, dataChunk, ref(result[i]), ref(matchData), fieldToSearch, &doneFlag);
			start = end; // Move to the next chunk
		}
		joinThreads joiners(threads);
	}
	for (auto& ref : result) {
		auto feature = ref.get_future().get();
		if (!feature.empty()) {
			return feature;
		}
	}
	return {};
}
void File::findData_v1(vector<vector<string>> dataChunk, promise<vector<vector<string>>>& result, const string& matchData, unsigned int fieldToSearch, atomic<bool>* doneFlag) {
	try {
		//cout << "Thread ID:" << this_thread::get_id() << "\n";
		//cout << "dataChunk:" << da
		//  taChunk.size() << "\n";
		for (auto& row : dataChunk) {
			if (doneFlag->load()) {
				return;
			}
			//cout << "field:" << fieldToSearch << "-" << row[fieldToSearch - 1] <<  " Match:" << matchData << "\n";
			if (row.size() >= fieldToSearch && row[fieldToSearch - 1] == matchData) {
				//cout << "found(" << fieldToSearch << ")-" << row[fieldToSearch - 1] <<  " Match:" << matchData << "\n";
				doneFlag->store(true);
				result.set_value({ row });
				return;
			}
		}
		result.set_value({});
	}
	catch (exception& ex) {
		cout << "current exception:" << ex.what() << "\n";
		result.set_exception(current_exception());
		doneFlag->store(true);
		throw;
	}
};

vector<string> File::seachDataAsync_v2(vector<vector<string>>& data, const string& matchData, const unsigned int fieldToSearch) {
	const size_t hardwareThread = thread::hardware_concurrency();
	const size_t blockSize = data.size() / hardwareThread;
	atomic<bool> doneFlag{ false };
	vector<future<vector<string>>> result;

	auto start = data.begin();

	{
		for (size_t i = 0; i < hardwareThread; ++i) {
			//cout << "Count:" << i + 1 << "\n";
			if (doneFlag.load()) {
				break;
			}
			auto end = (i == hardwareThread - 1) ? data.end() : next(start, blockSize);
			vector<vector<string>> dataChunk(start, end);
			//cout << "dataChunk.size:" << dataChunk.size() << "\n";
			result.emplace_back(async(launch::async, &File::findData_v2, this, dataChunk, ref(matchData), fieldToSearch, &doneFlag));
			start = end; // Move to the next chunk
		}
	}

	for (auto& ref : result) {
		const auto feature = ref.get();
		if (!feature.empty()) {
			return feature;
		}
	}
	return {};
}

vector<string> File::findData_v2(const vector<vector<string>>& dataChunk, const string& matchData, unsigned int fieldToSearch, atomic<bool>* doneFlag) {
	try {
		//cout << "Thread ID:" << this_thread::get_id() << "\n";
		//cout << "dataChunk_1:" << dataChunk.size() << "\n";
		for (auto& row : dataChunk) {
			//cout << "before doneFlag check\n";
			if (doneFlag->load() || row.empty()) {
				if (row.empty()) {
					cout << "row.empty()\n";
				}
				else {
					cout << "doneFlag\n";
				}
				return {};
			}
			//cout << "after doneFlag check\n";
			//cout << "field:" << fieldToSearch << "-" << row[fieldToSearch - 1] <<  " Match:" << matchData << "\n";
			if (row[fieldToSearch - 1] == matchData) {
				//cout << "found(" << fieldToSearch << ")-" << row[fieldToSearch - 1] <<  " Match:" << matchData << "\n";
				doneFlag->store(true);
				return row;
			}
		}
		//cout << "dataChunk_2:" << dataChunk.size() << "\n";
		return {};
	}
	catch (exception& ex) {
		cout << "current exception:" << ex.what() << "\n";
		doneFlag->store(true);
		throw;
		return {};
	}
};

