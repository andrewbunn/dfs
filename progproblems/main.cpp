#include <vector>
#include <array>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <queue>
#include <functional>
#include <random>
#include <cstdint>
#include <chrono>
#include <ppl.h>

using namespace concurrency;


using namespace std;

vector<int> maxInWindows(const vector<int>& numbers, int windowSize)
{
	assert(numbers.size() - windowSize >= 0);

	vector<int> windows;
	windows.reserve(numbers.size() - windowSize + 1);

	list<int> candidateIndices;

	// within first window, assemble candidates
	// can prove induction that list is sorted
	for (int i = 0; i < windowSize; i++)
	{
		// can this be an if? and still ensure that we have a sorted list
		while (!candidateIndices.empty() && numbers[candidateIndices.back()] <= numbers[i])
		{
			candidateIndices.pop_back();
		}
		candidateIndices.push_back(i);
	}

	for (int i = windowSize, count = numbers.size(); i < count; i++)
	{
		windows.push_back(numbers[candidateIndices.front()]);
		while (!candidateIndices.empty() && 
			(numbers[candidateIndices.back()] <= numbers[i]))
		{
			candidateIndices.pop_back();
		}
		while (!candidateIndices.empty() &&
			(candidateIndices.front() <= (i - windowSize)))
		{
			candidateIndices.pop_front();
		}
		candidateIndices.push_back(i);
	}

	windows.push_back(numbers[candidateIndices.front()]);

	return windows;
}

void runWindows()
{
	int windowSize = 4;
	vector<int> numbers;
	array<int, 12> arr = { 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };


	for (auto i : arr)
	{
		numbers.push_back(i);
	}
	vector<int> windows = maxInWindows(numbers, windowSize);

	for (auto i : windows)
	{
		cout << i << ",";
	}
	cout << "\n";

}
struct Candidate {
	Candidate() : size(0) {}
	int minValue() const {
		assert(size > 0);
		return indices[0];
	};
	int maxValue() const { return indices[size - 1]; };

	array<unsigned int, 3> indices;
	
	unsigned int size;
};
// 4 3 2 1 2 0 1 3
bool findIncreasingTriple(const vector<int>& numbers, array<unsigned int, 3>& indices)
{
	// "candidate": count, highest value
	// candidates in order of decreasing size
	array<Candidate, 2> candidates;

	// initialize candidates
	// go until we have 2 small values
	const int* lowest = nullptr;
	unsigned int lowestindex = 0;
	const int* secondLowest = nullptr;
	unsigned int secondLowestindex = 0;
	unsigned int i;
	for (i = 0; i < numbers.size(); i++)
	{
		if (lowest == nullptr || 
			numbers[i] < *lowest)
		{
			lowest = &(numbers[i]);
			lowestindex = i;
		}
		else if (secondLowest == nullptr ||
			numbers[i] > *lowest)
		{
			secondLowest = &(numbers[i]);
			secondLowestindex = i;
			break;
		}
	}

	candidates[0].size = 2;
	candidates[0].indices[0] = lowestindex;
	candidates[0].indices[1] = secondLowestindex;
	candidates[1].size = 1;
	candidates[1].indices[0] = lowestindex;

	for (; i < numbers.size(); i++)
	{
		for (unsigned int c = 0; c < candidates.size(); c++)
		{
			auto candidate = candidates[c];
			//unsigned int candidateLength = indices.size() - c - 1;
			if (numbers[i] > numbers[candidate.maxValue()])
			{
				candidate.indices[candidate.size] = i;
				candidate.size++;
				if (candidate.size == indices.size())
				{
					indices = candidate.indices;
					return true;
				}

				// any time we increase size of candidate, need to compare to other candidates
				// overwrite last candidate
				candidates[c - 1] = candidate;
				break;
			}
			
			// only replace last candidate (has size 1, otherwise would've hit above block)
			if (numbers[i] < numbers[candidate.minValue()] &&
				candidate.size == 1)
			{
				candidate.indices[0] = i;
				// size stays one, just replaced index
				break;
			}


			if (numbers[i] < numbers[candidate.maxValue()])
			{
				// replace max value
				candidate.indices[candidate.size - 1] = i;

			}
		}
	}

	return false;
}

	//(n-1)n/2 = (n^2 - n)/2
// 1 1 1 2
// first = 1
// second = 0
//
//assume unique nums
int binSearchInRotatedArray(const vector<int>& numbers, int target)
{
	int l = 0;
	int r = numbers.size();
	bool fSorted = false;
	int m;
	do
	{
		m = ((r + l) / 2);
		if (numbers[m] == target)
		{
			return m;
		}

		if (numbers[l] < numbers[m])
		{
			//bottom half is sorted, min must be in upper half?
			// if target is in this range, we can just binSearch on it
			if (target >= numbers[l] && target <= numbers[m])
			{
				//fSorted = true;
				r = m - 1;
			}
			else
			{
				l = m + 1;

			}
		}
		// 5 1 2 3 4
		else
		{
			// top half is sorted. so if > m search top half otherwise go bottom
			if (target >= numbers[m])
			{
				//fSorted = true;
				l = m + 1;
			}
			else
			{
				r = m - 1;
			}
		}
	}
	while (l < r);

	// ?
	return m;
}

void runIndices()
{
	int windowSize = 4;
	vector<int> numbers;
	//array<int, 12> arr = { 12, 1, 10, 9, 8, 7, 2, 5, 4, 3, 2, 1 };
	//"1,2,5,"
	//array<int, 9> arr = { 3, 2, 5, 1, 4, 7, 9, 6, 8 };
	// "2,5,7,"
	//array<int, 6> arr = { 10, 50, 49, 48, 11, 12 };
	// "10,11,12,
	//array<int, 5> arr = { 6, 1, 5, 2, 4 };
	// "1,2,4,"
	array<int, 5> arr = { 4, 1, 5, 3, 2 };
	// ""
	for (auto i : arr)
	{
		numbers.push_back(i);
	}

	array<unsigned int, 3> indices;
	if (findIncreasingTriple(numbers, indices))
	{
		for (auto i : indices)
		{
			cout << numbers[i] << ",";
		}
	}

 	cout << "\n";

}


// boundary conditions:
// "" -> done (0)
// 1 char -> "" or duplicate (1)
// to verify a string >= 2: split in half and walk

// for "easy" version:
// keep counts of distinct characters, counts of same "sequence"
// find longest sequence that repeats
// dyn prog: for a character with > 1 instance
// pick second instance, now we have two substrings from i+1 -> j, j+1 -> n
// combine substrings and repeat problem
// 0 -> ""
// for each character repeated we have possible strings "ff" -> 2
// start from end of string to find last repeated chars
// get rid of a pair and you have a string:
// "frankfurt" -> f -> "rankurt"
// or you could do r -> ankfut (no repeat) -> dead end
// or could go backwards to "build up"
// to make it "dyn prog" we just store a result string and a value
unordered_map<string, int> map;

void createNewString(const char* s1, char* s2, char* s3, int x, int y, int len)
{
	int c = 0;
	int i;
	for (i = x + 1; i < y; i++)
	{
		s2[c++] = s1[i];
	}
	i++;
	c = 0;
	for (; i < len; i++)
	{
		s3[c++] = s1[i];
	}
}

void createNewString(const char* s1, char* s3, int x, int len1)
{
	int c = 0;
	for (int i = x + 1; i < len1; i++)
	{
		s3[c++] = s1[i];
	}
}

int runMaxLen2(const char* s1, const char* s2, int len1, int len2)
{
	if (len1 <= 0 || len2 <= 0)
	{
		return 0;
	}
	// could be "contains"
	int minlen = len1 <= len2 ? len1 : len2;
	if (strncmp(s1, s2, minlen) == 0)
	{
		return minlen * 2;
	}

	// fast check for any duplicate? or is n^2, could keep sorted string
	// later optimization
	int max = 0;
	for (int i = 0; i < len1; i++)
	{
		char a = s1[i];
		for (int j = 0; j < len2; j++)
		{
			if (s2[j] == a)
			{
				// remove i, j, new string is from i+1 -> j, j+1->n
				//char s3[50] = {};
				//char s4[50] = {};
				//createNewString(s1, s3, i, len1);
				//createNewString(s2, s4, j, len2);

				int newval;
				//string combo(s3 + ',' + s4);
				//createKeyString();
				int cch = len1 - i - 1;
				int cch2 = len2 - j - 1;
				bool found = false;
				if (cch > 0 && cch2 > 0)
				{
					char keystring[50];
					strncpy_s(keystring, s1 + i + 1, cch);
					keystring[cch] = ',';
					strncpy_s(keystring + cch + 1, 50 - cch - 1, s2 + j + 1, len2 - j - 1);


					auto iter = map.find(keystring);
					if (iter != map.end())
					{
						newval = iter->second;
					}
					else
					{
						newval = 2 + runMaxLen2(s1 + i + 1, s2 + j + 1, len1 - i - 1, len2 - j - 1);
						map.emplace(make_pair(keystring, newval));
					}
				}
				else
				{
					newval = 2 + runMaxLen2(s1 + i + 1, s2 + j + 1, len1 - i - 1, len2 - j - 1);
				}

				if (newval > max)
				{
					max = newval;
					// max is even so if len is max + 1 then its still maximum
					if (max + 1 >= len1 + len2)
					{
						return max;
					}
				}
			}
		}
	}

	return max;
}

int runMaxLen(const char* s, int len)
{
	if (len <= 1)
	{
		return 0;
	}
	
	if (len == 2)
	{
		if (s[0] == s[1])
		{
			return 2;
		}
		return 0;
	}

	// fast check for any duplicate? or is n^2, could keep sorted string
	// later optimization
	int max = 0;
	for (int i = 0; i < len - 1; i++)
	{
		char a = s[i];
		for (int j = i + 1; j < len; j++)
		{
			if (s[j] == a)
			{

				int newval = 2 + runMaxLen2(s + i + 1, s + j + 1, j - i - 1, len - j - 1);

				if (newval > max)
				{
					max = newval;
					// max is even so if len is max + 1 then its still maximum
					if (max + 1 >= len)
					{
						return max;
					}
				}
			}
		}
	}

	return max;
}

int maximalLength(string s)
{
	return runMaxLen(s.c_str(), s.size());
}

vector<int>* dpMap;
int _width;

inline void createKeyString(const char* s1, int len1, const char* s2, int len2, char* keystring)
{

	strncpy_s(keystring, 50, s1, len1);
	keystring[len1] = ',';
	strncpy_s(keystring + len1 + 1, 50 - len1 - 1, s2, len2);
}

int runMinMod2(const char* s1, int len1, const char* s2, int len2)
{
	if (len1 <= 0)
	{
		return len2;
	}
	if (len2 <= 0)
	{
		return len1;
	}
	// could be "contains"
	int maxlen = len1 >= len2 ? len1 : len2;
	int minlen = len1 <= len2 ? len1 : len2;
	int minCost = maxlen - minlen;
	if (strncmp(s1, s2, minlen) == 0)
	{
		return minCost;
	}

	int min = maxlen;
	for (int i = 0; i < len1; i++)
	{
		char a = s1[i];
		for (int j = 0; j < len2; j++)
		{
			if (s2[j] == a)
			{
				int newval;
				int cch = len1 - i - 1;
				int cch2 = len2 - j - 1;
				bool found = false;
				int cost = max(i, j);
				if (cch > 0 && cch2 > 0)
				{
					/*
					// could sort pairs
					char keystring[50];
					const char* p1 = s1 + i + 1;
					const char* p2 = s2 + j + 1;
					if (p1[0] <= p2[0])
					{
						createKeyString(p1, cch, p2, cch2, keystring);
					}
					else
					{
						createKeyString(p2, cch2, p1, cch, keystring);
					}

					auto iter = map.find(keystring);
					if (iter != map.end())
					{
						newval = cost + iter->second;
					}
					else
					{
						int result = runMinMod2(s1 + i + 1, len1 - i - 1, s2 + j + 1, len2 - j - 1);
						map.emplace(make_pair(keystring, result));
						newval = cost + result;
					}*/
					int index = (cch)* _width + (cch2);
					if ((*dpMap)[index] != 0)
					{
						newval = cost + (*dpMap)[index];
					}
					else
					{
						int result = runMinMod2(s1 + i + 1, len1 - i - 1, s2 + j + 1, len2 - j - 1);
						(*dpMap)[index] = result;
						newval = cost + result;
					}
				}
				else
				{
					newval = cost + runMinMod2(s1 + i + 1, cch, s2 + j + 1, cch2);
				}

				if (newval < min)
				{
					min = newval;
					if (min == minCost)
					{
						return min;
					}
				}
			}
		}
	}

	return min;
}

int runMinMod(const char* s, int len)
{
	if (len <= 1)
	{
		return len;
	}

	if (len == 2)
	{
		if (s[0] == s[1])
		{
			return 0;
		}
		return 1;
	}

	int min = (len + 1) / 2;
	// start at middle, the +1 -1, +2 -2
	
	for (int i = 0; (i < min) && (i < len - ((len + 1) / 2)); i++)
	{
		char a = s[i];
		int countFromMiddle = 0;
		int c = 1;
		int remLen = len - i;
		for (int j = remLen / 2 ; (j > i) && (min - i - (c / 2) > 0);)
		{
			if (s[j] == a)
			{
				//int costSoFar = i + countFromMiddle;
				int newval = i + runMinMod2(s + i + 1, j - i - 1, s + j + 1, len - j - 1);

				if (newval < min)
				{
					min = newval;
					if (min == 0)
					{
						return min;
					}
				}
			}
			c++;
			if (c % 2 == 0)
			{
				countFromMiddle = c / 2;
			}
			else
			{
				countFromMiddle = -(c / 2);
			}
			j = remLen / 2 + countFromMiddle;
		}
	}

	return min;
}

int minimalModify(string s)
{
	// insert: count + 1 possibilities
	// remove: count possibilities
	// replace: count possibilities
	// 3c + 1 total branches from a given string
	// the most would be c + 1 /2, since you could change half the chars
	// start with a split, maybe dont delete a char?
	// split probably rnages from about 25% to 75%
	// only worth splitting if some duplicated chars
	// max would be maxlen between the two
	// 12 -> we know we can get 6
	// every move from middle costs at least 2? (because size diff?)
	return runMinMod(s.c_str(), s.size());
}


void nextLexicographicalWords(vector<string>& words)
{
	for (auto word : words)
	{
		bool succeeded = false;
		int len = word.size();
		// start from second to last char
		for (int i = len - 2; !succeeded && i >= 0; i--)
		{
			// get furthest right char thats viable
			for (int j = len - 1; j > i; j--)
			{
				if (word[j] > word[i])
				{
					swap(word[i], word[j]);
					succeeded = true;
					break;
				}
			}
		}
		if (succeeded)
		{
			cout << word;
			cout << endl;
		}
	}
}

struct Interval {
	int start;
	int end;
	Interval() : start(0), end(0) {}
	Interval(int s, int e) : start(s), end(e) {}
};

bool intervalComparator(Interval& a, Interval& b)
{
	return (a.start < b.start);
}

vector<Interval> merge(vector<Interval>& intervals) {
	vector<Interval> mergedIntervals;
	if (intervals.size() > 0)
	{
		sort(intervals.begin(), intervals.end(), intervalComparator);
		mergedIntervals.push_back(intervals[0]);
		for (int i = 1; i < intervals.size(); i++)
		{
			// perform merge
			if (intervals[i].start <= mergedIntervals[mergedIntervals.size() - 1].end)
			{
				mergedIntervals[mergedIntervals.size() - 1].end = intervals[i].end;
			}
			else
			{
				mergedIntervals.push_back(intervals[i]);
			}
		}
	}
	return mergedIntervals;
}
struct CharPositionData {
	int countNeeded;
	list<list<int>::iterator> positions;
	CharPositionData() : countNeeded(1) {}
};
string minWindow(string s, string t) {
	if (s.size() == 0 || t.size() == 0)
	{
		return "";
	}

	int minWindowSoFar = s.size();
	int minWindowSoFarBegin = 0;

	list<int> currentWindowIndices;

	// make it a map of a list of iterators (all positions so far)
	// but we also need count required of each character
	unordered_map<char, CharPositionData> windowPositionMap;
	windowPositionMap.reserve(t.size());
	for (auto c : t)
	{
		auto iter = windowPositionMap.find(c);
		if (iter != windowPositionMap.end())
		{
			iter->second.countNeeded++;
		}
		else
		{
			windowPositionMap.emplace(make_pair(c, CharPositionData()));
		}
	}

	bool foundValidResult = false;
	for (int i = 0; i < s.size(); i++)
	{

		auto iter = windowPositionMap.find(s[i]);
		if (iter != windowPositionMap.end())
		{
			// not first occurance of char
			if (iter->second.positions.size() == iter->second.countNeeded)
			{
				// seen character before, update window
				currentWindowIndices.erase(iter->second.positions.front());
				iter->second.positions.pop_front();
			}

			// update current indices
			currentWindowIndices.push_back(i);
			auto listIter = currentWindowIndices.end();
			--listIter;
			iter->second.positions.push_back(listIter);

			// if we have a full window
			if (currentWindowIndices.size() == t.size())
			{
				foundValidResult = true;
				int currentWindowSize = i - currentWindowIndices.front() + 1;
				if (currentWindowSize < minWindowSoFar)
				{
					minWindowSoFarBegin = currentWindowIndices.front();
					minWindowSoFar = currentWindowSize;
					// early return if all chars are adjacent:
					if (minWindowSoFar == currentWindowIndices.size())
					{
						break;
					}
				}
			}
		}
	}

	if (foundValidResult)
	{
		return s.substr(minWindowSoFarBegin, minWindowSoFarBegin + minWindowSoFar);
	}
	else
	{
		return "";
	}

}
class MedianFinder {
private:
	// probably want a tree structure and keep pointers to medians
	// keep queues > median, below median, "center"
	priority_queue<int> below; // top is highest
	priority_queue<int, vector<int>, greater<int>> above; // top is lowest

	inline double currentMedian()
	{
		int diff = below.size() - above.size();
		if (diff == 0)
		{
			return static_cast<double>(below.top() + above.top()) / 2.0;
		}
		else if (diff > 0)
		{
			return below.top();
		}
		else
		{
			return above.top();
		}
	}
public:

	// Adds a number into the data structure.
	void addNum(int num) {
		if (below.size() > 0 || above.size() > 0)
		{
			double median = currentMedian();
			if (num > median)
			{
				above.push(num);
			}
			else
			{
				below.push(num);
			}

			// rebalance
			int diff = below.size() - above.size();
			if (diff > 1)
			{
				above.push(below.top());
				below.pop();
			}
			else if (diff < -1)
			{
				below.push(above.top());
				above.pop();
			}
		}
		else
		{
			// doesnt matter which to start
			below.push(num);
		}
	}

	// Returns the median of current data stream
	double findMedian() {
		if (below.size() > 0 || above.size() > 0)
		{
			return currentMedian();
		}
		else
		{
			return 0;
		}
	}
};


bool validWordSquare(vector<string>& words) {
	bool isValid = true;

	int height = words.size();
	for (int i = 0; i < height; i++)
	{
		int width = words[i].size();

		int heightWidthDiff = height - width;
		if (heightWidthDiff < 0)
		{
			return false;
		}

		if (heightWidthDiff > 0 && width >(height / 2) && (i + 1) < height)
		{
			if (words[i + 1].size() != width - 1)
			{
				return false;
			}
		}

		for (int j = i + 1; j < width; j++)
		{
			if (words[j].size() < i)
			{
				return false;
			}
			if (words[i][j] != words[j][i])
			{
				return false;
			}
		}
	}

	return isValid;
}
/// 11011
//  11100
//  00100
//  00010


static bool compareEndToStart(const Interval &a, const Interval &b){
	return a.end < b.start;
}
static bool compareStartToEnd(const Interval &a, const Interval &b){
	return a.start <= b.end;
}
vector<Interval> insert(vector<Interval>& intervals, Interval newInterval) {

	vector<Interval> newIntervals;
	newIntervals.reserve(intervals.size() + 1);

	auto iterStart = upper_bound(intervals.begin(), intervals.end(), newInterval, compareStartToEnd);

	// insert new interval
	if (iterStart != intervals.end())
	{
		copy(intervals.begin(), iterStart, back_inserter(newIntervals));
		auto iterEnd = upper_bound(iterStart, intervals.end(), newInterval, compareEndToStart);

		//if (iterEnd == iterStart)
		//{
		//intervals.insert(iterStart, newInterval);

		//}
		//else
		if (iterEnd != iterStart)
		{
			if (iterStart->start < newInterval.start)
			{
				newInterval.start = iterStart->start;
			}

			auto beforeEnd = iterEnd - 1;
			if (beforeEnd->end > newInterval.end)
			{
				newInterval.end = beforeEnd->end;
			}


			/*intervals[std::distance(intervals.begin(), iterStart)] = newInterval;
			iterStart++;
			if (iterStart != intervals.end())
			{
			intervals.erase(iterStart, iterEnd);
			}*/
		}

		//newIntervals.push_back(newInterval);
		newIntervals.insert(newIntervals.end(), newInterval);
		if (iterEnd != intervals.end())
		{
			auto iter = newIntervals.end() - 1;
			copy(iterEnd, intervals.end(), back_inserter(newIntervals));
		}
	}
	else
	{
		//intervals.push_back(newInterval);
		copy(intervals.begin(), intervals.end(), back_inserter(newIntervals));

		newIntervals.push_back(newInterval);
	}

	return newIntervals;
}

unordered_set<string> dict;
vector<string> otherdict;

	// Adds a word into the data structure.
	void addWord(string word) {
		// 2^ wordlength insert
		// cheat for words > 31?
		if (word.size() < 32)
		{
			int wordsize = word.size();
			int max = (2 << wordsize) - 1;
			vector<char> str(wordsize + 1, 0);
			for (int i = 0; i < max; i++)
			{
				for (int c = 0; c < wordsize; c++)
				{
					int bit = 1 << c;
					if ((i & bit) != 0)
					{
						str[c] = word[c];
					}
					else
					{
						str[c] = '.';
					}
				}
				str[wordsize] = '\0';
				dict.emplace(str.data());
			}
		}
		else
		{
			otherdict.push_back(word);
		}
	}

	// Returns if the word is in the data structure. A word could
	// contain the dot character '.' to represent any one letter.
	bool search(string word) {
		if (word.size() < 32)
		{
			return (dict.find(word) != dict.end());
		}
		else
		{
			int targetsize = word.size();

			for (int i = 0; i < otherdict.size(); i++)
			{
				string w = otherdict[i];
				if (w.size() == targetsize)
				{
					bool validWord = true;
					for (int c = 0; c < targetsize; c++)
					{
						if (word[c] != '.' && word[c] != w[c])
						{
							validWord = false;
							break;
						}
					}

					if (validWord)
					{
						return true;
					}
				}
			}
		}
	}

	struct node 
	{
		node* root;
		int size;
		node() : root(this), size(1) {}
	};
	unordered_map<int, node*> consecutiveMap;
	// indexes to sequenceCounts
	//int nextCount;
	//int nextIndex;
	//vector<int> crazyArray;
	vector<node> tree;

	node* find(node* x)
	{
		if (x->root != x)
		{
			x->root = find(x->root);
		}
		return x->root;
	}

	//vector<int> sequenceCounts;
	int longestConsecutive(vector<int>& nums) {
		//sequenceCounts.reserve(nums.size());
		//sequenceIdToCount.reserve(nums.size());
		tree.reserve(nums.size());
		//crazyArray.assign(nums.size() * 2, -1);
		consecutiveMap.reserve(nums.size());
		//nextCount = 0;
		//nextIndex = nums.size();

		int maxSoFar = 0;
		for (int c : nums)
		{
			// if we're already part of sequence, then we're done
			auto iter = consecutiveMap.find(c);
			if (iter == consecutiveMap.end())
			{
				auto iterPrev = consecutiveMap.find(c - 1);
				auto iterNext = consecutiveMap.find(c + 1);
				// if 1 is non empty, point to same sequnce, update count
				// if both nonempty, need to merge sequnce (both point to same place in vector, set to sum)
				int size;
				if (iterPrev == consecutiveMap.end() && iterNext == consecutiveMap.end())
				{
					size = 1;
					tree.emplace_back();
					consecutiveMap.emplace(make_pair(c, &tree[tree.size() - 1]));
				}
				else if (iterPrev != consecutiveMap.end() && iterNext != consecutiveMap.end())
				{
					// merge two sequences, update both counts
					node* nodePrev = iterPrev->second;
					node* nodeNext = iterNext->second;
					node* rootPrev = find(nodePrev);
					node* rootNext = find(nodeNext);

					size = rootPrev->size + rootNext->size + 1;
					rootPrev->size = size;
					//Union(rootPrev, rootNext);
					rootNext->root = rootPrev;
					consecutiveMap.emplace(make_pair(c, rootPrev));
				}
				else if (iterPrev != consecutiveMap.end())
				{
					node* nodePrev = iterPrev->second;
					node* rootPrev = find(nodePrev);

					size = rootPrev->size + 1;
					rootPrev->size = size;
					consecutiveMap.emplace(make_pair(c, rootPrev));
				}
				else
				{
					node* nodeNext = iterNext->second;
					node* rootNext = find(nodeNext);

					size = rootNext->size + 1;
					rootNext->size = size;
					consecutiveMap.emplace(make_pair(c, rootNext));
				}
				maxSoFar = max(maxSoFar, size);
			}
		}
		return maxSoFar;
	}

	class NumMatrix {
		vector<vector<int>> _matrix;
	public:
		NumMatrix(vector<vector<int>> &matrix) : _matrix(matrix){
			int height = matrix.size();
			int sum = 0;
			for (int i = 0; i < height; i++)
			{
				int len = matrix[i].size();
				for (int j = 0; j < len; j++)
				{
					int diag = (i == 0 || j == 0) ? 0 : _matrix[i - 1][j - 1];
					int above = (i == 0) ? 0 : _matrix[i - 1][j];
					int behind = (j == 0) ? 0 : _matrix[i][j - 1];
					_matrix[i][j] = _matrix[i][j] + above + behind - diag;
				}
			}
		}

		int sumRegion(int row1, int col1, int row2, int col2) {
			int rowBefore = row1 - 1;
			int colBefore = col1 - 1;
			int diag = (rowBefore < 0 || colBefore < 0) ? 0 : _matrix[rowBefore][colBefore];
			int above = (rowBefore < 0) ? 0 : _matrix[rowBefore][col2];
			int behind = (colBefore < 0) ? 0 : _matrix[row2][colBefore];
			return _matrix[row2][col2] - above - behind + diag;
		}
	};


	vector<int> majorityElement(vector<int>& nums) {
		// if we look at a chunk of 4 elements, at some point we're guaranteed to have > 1 of a majority element
		// can be at most 2 majority elements
		// 10 randoms gives ~ .99% confidence
		int len = nums.size();
		int majority = len / 3;
		default_random_engine generator;
		uniform_int_distribution<int> distribution(0, len - 1);
		vector<int> arr;
		vector<int> result;

		for (int i = 0; i < 10; i++)
		{
			int candidate = nums[distribution(generator)];

			// haven't already checked value
			if (find(arr.begin(), arr.end(), candidate) == arr.end())
			{
				arr.push_back(candidate);
				int count = 0;
				// stop if len - majority
				for (int j = 0; j < len; j++)
				{
					if (nums[j] == candidate)
					{
						count++;
					}
				}
				if (count > majority)
				{
					result.push_back(candidate);
				}
			}
		}

		return result;
	}
	
	enum Position {
		qb = 0,
		rb = 1,
		wr = 2,
		te = 3,
		def = 4
	};

	int numPositions = 5;

	int PositionCount[5] = { 1, 2, 3, 1, 1 };
	int slots[9] = {0, 1, 1, 2, 2, 2, 3, 5/*flex*/, 4 };

	struct Player {
		string name;
		float proj;
		//float val;
		uint8_t cost;
		uint8_t pos;
		uint8_t index;
		Player(string s, uint8_t c, float p, uint8_t pos, uint8_t idx) : name(s), cost(c), proj(p), pos(pos), index(idx)
		{
			//val = proj / (float)cost;
		}
	};

	struct Players {
		array<int, 9> p;
		array<int, 5> posCounts;
		int totalCount;
		float value;
		bool hasFlex;

		inline void addPlayer(int pos, float proj, int index)
		{
				posCounts[pos]++;
				p[totalCount++] = index;
				value += proj;
		}

		bool tryAddPlayer(int pos, float proj, int index)
		{
			int diff = posCounts[pos] - PositionCount[pos];
			if (diff < 0)
			{
				addPlayer(pos, proj, index);
				return true;
			}

			if (hasFlex || pos == 0 || pos == 4 || diff > 1)
			{
				return false;
			}

			addPlayer(pos, proj, index);
			hasFlex = true;
			return true;
		}

		Players() : totalCount(0), value(0), /*costOfUnfilledPositions(90),*/ hasFlex(false)
		{
			posCounts.fill(0);
		}
	};

	struct Players2 {
		uint64_t bitset;
		array<uint8_t, 5> posCounts;
		uint8_t totalCount;
		float value;
		bool hasFlex;

		inline bool addPlayer(int pos, float proj, int index)
		{
			uint64_t bit = (uint64_t)1 << index;
			if ((bitset & bit) == 0)
			{
				posCounts[pos]++;
				bitset |= bit;
				totalCount++;
				value += proj;
				return true;
			}
			return false;
		}

		bool tryAddPlayer(int pos, float proj, int index)
		{
			int diff = posCounts[pos] - PositionCount[pos];
			if (diff < 0)
			{
				return addPlayer(pos, proj, index);
			}

			if (hasFlex || pos == 0 || pos == 4 || diff > 1)
			{
				return false;
			}

			bool succeeded = addPlayer(pos, proj, index);
			hasFlex = succeeded;
			return succeeded;
		}

		bool Players2::operator==(const Players2& other) {
			return bitset == other.bitset;
		}

		Players2() : totalCount(0), value(0), bitset(0), /*costOfUnfilledPositions(90),*/ hasFlex(false)
		{
			posCounts.fill(0);
		}
	};

	bool operator==(const Players2& first, const Players2& other) {
		return first.bitset == other.bitset;
	}

	bool operator<(const Players2& lhs, const Players2& rhs)
	{
		// backwards so highest value is "lowest" (best ranked lineup)
		return lhs.value > rhs.value;
	}

	size_t players_hash(const Players2& ps)
	{
		return hash<uint64_t>()(ps.bitset);
	}



	vector<string> getNextLineAndSplitIntoTokens(istream& str)
	{
		vector<string>   result;
		string                line;
		getline(str, line);

		stringstream          lineStream(line);
		string                cell;

		while (getline(lineStream, cell, ','))
		{
			result.push_back(cell);
		}
		return result;
	}

	vector<Player> parsePlayers(string filename)
	{
		vector<Player> result;
		ifstream       file(filename);
		vector<string> tokens = getNextLineAndSplitIntoTokens(file);
		int count = 0;
		while (tokens.size() >= 4)
		{
			int c = atoi(tokens[1].c_str()) - 10;
			float p = (float)atof(tokens[2].c_str());
			int pos = atoi(tokens[3].c_str());
			if (pos == 0)
			{
				c -= 10;
			}
			if (tokens.size() == 4)
			{
				result.emplace_back(tokens[0], c, p, pos, count++);
			}
			tokens = getNextLineAndSplitIntoTokens(file);
		}

		return result;
	}

	void parsePlayerInfo()
	{
		vector<vector<Player>> allPlayers;
		allPlayers.reserve(numPositions);
		for (int i = 0; i < numPositions; i++)
		{
			vector<Player> players;
			allPlayers.push_back(players);
		}
		// 5 positions
	}

	

	// parse csv of player, cost, proj?
	// budget = 200
	// restraints = 1 qb, 2rb, 3wr, 1te, 1flx, 1def
	// for def could just do budget/2 for loose estimate
	// dp array of costs?
	// when we find a "max" need to find a "next" -> disallow a player?
	// iterate through original lineup - toggle players
	// playerid -> pos, index
	
	Players knapsack(int budget, vector<Player>& players)
	{
		// probably want arrays sorted by value
		// pick player, adjust remaining budget, current value
		// recurse
		// keep array of cost?
		// "current player list" could be array<int, 9>
		// keep "index" of each player, know how many of each player
		vector<vector<Players>> m(players.size() + 1);
		//m.reserve(players.size());
		for (int i = 0; i <= players.size(); i++)
		{
			m[i].reserve(budget + 1);
		}
		// TODO initialize vectors
		for (int j = 0; j <= budget; j++)
		{
			m[0].emplace_back();
		}
		//m[0].assign(budget + 1, Players());
		
		// iterate through all players, when we "add player" verify its valid
		for (int i = 1; i <= players.size(); i++)
		{
			Player& p = players[i - 1];
			// 0 -> 200
			for (int j = 0; j <= budget; j++)
			{
				// m [i, j] -> value, player list (best value using first i players, under j weight)
				if (p.cost > j)
				{
					// player is too expensive so we can't add him
					m[i].push_back(m[i - 1][j]);
				}
				else
				{
					auto prev = m[i - 1][j - p.cost];
					bool useNewVal = false;
					// prev.second needs to describe list of players
					if (prev.tryAddPlayer(p.pos, p.proj, i - 1))
					{
						// check player list of m[i - 1, j] if we can add
						// check m[i - 1, j - i.cost] to see if we can add our player
						// could make that part of tryAddPlayer
						//prev.value = m[i - 1][j - p.cost].value + p.proj;
						if (prev.value > m[i - 1][j].value)
						{
							m[i].push_back(prev);
							//m[i][j] = prev;
							useNewVal = true;
						}
					}
					
					if (!useNewVal)
					{
						m[i].push_back(m[i - 1][j]);
						//m[i][j] = m[i - 1][j];
					}
				}
			}
		}

		return m[players.size()][budget];
	}

	// can transpose players of same type
	Players2 knapsackPositions(int budget, int pos, const Players2 oldLineup, const vector<vector<Player>>& players)
	{
		if (pos >= 9)
		{
			return oldLineup;
		}

		Players2 bestLineup = oldLineup;
		auto loop = [&](const Player& p)
		{
			Players2 currentLineup = oldLineup;
			if (p.cost <= budget)
			{
				if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
				{
					return knapsackPositions(budget - p.cost, pos + 1, currentLineup, players);
				}
			}
			return currentLineup;
		};
		if (pos <= 2)
		{
			//parallel_for(size_t(0), players[pos].size(), loop);
			// could parallel transform each to a "lineup", keep best lineup
			vector<Players2> lineupResults(players[pos].size());
			//parallel_for_each(begin(players[pos]), end(players[pos]), loop);
			parallel_transform(begin(players[pos]), end(players[pos]), begin(lineupResults), loop);

			for (auto lineup : lineupResults)
			{
				if (lineup.value > bestLineup.value)
				{
					bestLineup = lineup;
				}
			}
		}
		else
		{

			for (int i = 0; i < players[pos].size(); i++)
			{
				const Player& p = players[pos][i];
				Players2 currentLineup = oldLineup;
				if (p.cost <= budget)
				{
					if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
					{
						Players2 lineup = knapsackPositions(budget - p.cost, pos + 1, currentLineup, players);
						if (lineup.value > bestLineup.value)
						{
							bestLineup = lineup;
						}
					}
				}
			}
		}
		return bestLineup;
	}

	typedef vector<Players2> lineup_list;

#define LINEUPCOUNT 10


	// can transpose players of same type
	lineup_list knapsackPositionsN(int budget, int pos, const Players2 oldLineup, const vector<vector<Player>>& players)
	{
		if (pos >= 9)
		{
			return lineup_list(1, oldLineup);
		}

		auto loop = [&](const Player& p)
		{
			Players2 currentLineup = oldLineup;
			if (p.cost <= budget)
			{
				if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
				{
					return knapsackPositionsN(budget - p.cost, pos + 1, currentLineup, players);
				}
			}

			return lineup_list(1, currentLineup);
		};
		if (pos <= 2)
		{
			vector<lineup_list> lineupResults(players[pos].size());
			parallel_transform(begin(players[pos]), end(players[pos]), begin(lineupResults), loop);

			lineup_list merged;
			merged.reserve(LINEUPCOUNT * lineupResults.size());
			for (auto& lineup : lineupResults)
			{
				merged.insert(merged.end(), lineup.begin(), lineup.end());
				sort(merged.begin(), merged.end());
				unique(merged.begin(), merged.end());
				if (merged.size() > LINEUPCOUNT)
				{
					merged.resize(LINEUPCOUNT);
				}
			}

			return merged;
		}
		else
		{
			lineup_list bestLineups;
			//lineup_list bestLineups(1, oldLineup);
			bestLineups.reserve(2 * LINEUPCOUNT);
			for (int i = 0; i < players[pos].size(); i++)
			{
				const Player& p = players[pos][i];
				Players2 currentLineup = oldLineup;
				if (p.cost <= budget)
				{
					if (currentLineup.tryAddPlayer(p.pos, p.proj, p.index))
					{
						// optimization to inline last call
						if (pos == 8)
						{
							bestLineups.push_back(currentLineup);
						}
						else
						{
							lineup_list lineups = knapsackPositionsN(budget - p.cost, pos + 1, currentLineup, players);
							bestLineups.insert(bestLineups.end(), lineups.begin(), lineups.end());
						}
						sort(bestLineups.begin(), bestLineups.end());
						unique(bestLineups.begin(), bestLineups.end());

						if (bestLineups.size() > LINEUPCOUNT)
						{
							bestLineups.resize(LINEUPCOUNT);
						}
					}
				}
			}
			return bestLineups;
		}
	}

	Players2 generateLineup(vector<Player>& p)
	{
		vector<vector<Player>> playersByPos(9);
		for (int i = 0; i < 9; i++)
		{
			for (auto& pl : p)
			{
				if (pl.pos == slots[i])
				{
					playersByPos[i].push_back(pl);
				}
				else if (slots[i] == 5 && (pl.pos == 1 || pl.pos == 2 || pl.pos == 3))
				{
					playersByPos[i].push_back(pl);
				}
			}
		}
		//auto start = chrono::steady_clock::now();

		return knapsackPositions(100, 0, Players2(), playersByPos);
	}

	lineup_list generateLineupN(vector<Player>& p)
	{
		vector<vector<Player>> playersByPos(9);
		for (int i = 0; i < 9; i++)
		{
			for (auto& pl : p)
			{
				if (pl.pos == slots[i])
				{
					playersByPos[i].push_back(pl);
				}
				else if (slots[i] == 5 && (pl.pos == 1 || pl.pos == 2 || pl.pos == 3))
				{
					playersByPos[i].push_back(pl);
				}
			}
		}

		return knapsackPositionsN(100, 0, Players2(), playersByPos);
	}

	vector<int> toggleLowValuePlayers(vector<Player>& p, Players2 lineup)
	{
		vector<int> result;
		int totalcost = 0;
		uint64_t bitset = lineup.bitset;
		int count = 0;
		float worstValue = 100.f;
		int worstValueIndex = 0;
		for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
		{
			if (bitset & 1 == 1)
			{
				count++;
				//result.push_back(p[i]);
				if (p[i].cost > 0)
				{
					float value = p[i].proj / p[i].cost;
					if (value < worstValue)
					{
						worstValue = value;
						worstValueIndex = i;
					}
				}
			}
			bitset = bitset >> 1;
		}

		result.push_back(worstValueIndex);
		return result;
	}

	void runPlayerOptimizer(string filein, string fileout)
	{
		vector<Player> p = parsePlayers(filein);
		auto start = chrono::steady_clock::now();

		Players2 lineup = generateLineup(p);
		auto end = chrono::steady_clock::now();
		auto diff = end - start;

		ofstream myfile;
		myfile.open(fileout);
		myfile << chrono::duration <double, milli>(diff).count() << " ms" << endl;

			int totalcost = 0;
			uint64_t bitset = lineup.bitset;
			int count = 0;
			for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
			{
				if (bitset & 1 == 1)
				{
					count++;
					myfile << p[i].name;
					totalcost += p[i].cost;
					myfile << endl;
				}
				bitset = bitset >> 1;
			}

			myfile << lineup.value;
			myfile << endl;
			myfile << totalcost;
			myfile << endl;
			myfile << endl;

		myfile.close();
	}

	void runPlayerOptimizerN(string filein, string fileout)
	{
		vector<Player> p = parsePlayers(filein);
		auto start = chrono::steady_clock::now();

		//Players2 lineup = generateLineup(p);
		lineup_list lineups = generateLineupN(p);
		auto end = chrono::steady_clock::now();
		auto diff = end - start;

			ofstream myfile;
			myfile.open(fileout);
			myfile << chrono::duration <double, milli>(diff).count() << " ms" << endl;

			for (auto lineup : lineups)
			{
				int totalcost = 0;
				uint64_t bitset = lineup.bitset;
				int count = 0;
				for (int i = 0; i < 64 && bitset != 0 && count < lineup.totalCount; i++)
				{
					if (bitset & 1 == 1)
					{
						count++;
						myfile << p[i].name;
						totalcost += p[i].cost;
						myfile << endl;
					}
					bitset = bitset >> 1;
				}

				myfile << lineup.value;
				myfile << endl;
				myfile << totalcost;
				myfile << endl;
				myfile << endl;

			}

			myfile.close();
	}

	void normalizeName(string& name)
	{
		// normalize to just 'a-z' + space
		// what about jr/sr?
		transform(name.begin(), name.end(), name.begin(), ::tolower);
		//return name;
		auto it = copy_if(name.begin(), name.end(), name.begin(), 
			[](char& c) {
			return (islower(c) != 0) || c == ' ';
		});
		name.resize(distance(name.begin(), it));

		// sr/jr
		static string sr = " sr";
		static string jr = " jr";

		it = find_end(name.begin(), name.end(), sr.begin(), sr.end());
		name.resize(distance(name.begin(), it));

		it = find_end(name.begin(), name.end(), jr.begin(), jr.end());
		name.resize(distance(name.begin(), it));

		array<string, 28> dsts = {
			"Baltimore",
				"Houston",
				"Arizona",
				"Denver",
				"Carolina",
				"Kansas City",
				"Chicago",
				"Jacksonville",
				"New England",
				"New Orleans",
				"Tampa Bay",
				"Los Angeles",
				"Philadelphia",
				"New York Jets",
				"Dallas",
				"Cincinnati",
				"Cleveland",
				"San Diego",
				"Minnesota",
				"Washington",
				"San Francisco",
				"Atlanta",
				"New York Giants",
				"Miami",
				"Green Bay",
				"Seattle",
				"Tennessee",
				"Pittsburgh"
		};

		for_each(dsts.begin(), dsts.end(), [](string& n) {
			transform(n.begin(), n.end(), n.begin(), ::tolower);
		});

		auto i = find_if(dsts.begin(), dsts.end(), [&name](string& n){
			return name.find(n) != string::npos;
		});
		if (i != dsts.end())
		{
			name = *i;
		}
	}

	vector<tuple<string, int, int>> parseCosts(string filename)
	{
		vector<tuple<string, int, int>> result;
		ifstream       file(filename);
		vector<string> tokens = getNextLineAndSplitIntoTokens(file);
		int count = 0;
		while (tokens.size() == 3)
		{
			int c = stoi(tokens[1]);
			int pos = stoi(tokens[2]);
			normalizeName(tokens[0]);
			// dont add david johnson TE
			if (tokens[0] != "david johnson" || pos == 1)
			{
				result.emplace_back(tokens[0], c, pos);
			}
			tokens = getNextLineAndSplitIntoTokens(file);
		}

		return result;
	}

	unordered_map<string, float> parseProjections(string filename)
	{
		unordered_map<string, float> result;
		ifstream       file(filename);
		vector<string> tokens = getNextLineAndSplitIntoTokens(file);
		int count = 0;
		// two david johnsons, just only add first value for now, should be fine
		while (tokens.size() == 2)
		{
			float proj = stof(tokens[1]);
			normalizeName(tokens[0]);
			if (result.find(tokens[0]) != result.end())
			{
				cout << tokens[0] << endl;
			}
			else
			{
				result.emplace(tokens[0], proj);
			}
			tokens = getNextLineAndSplitIntoTokens(file);
		}
		cout << endl;
		return result;
	}

	vector<string> parseRanks(string filename)
	{
		vector<string> result;
		ifstream       file(filename);
		vector<string> tokens = getNextLineAndSplitIntoTokens(file);
		while (tokens.size() >= 1)
		{
			normalizeName(tokens[0]);

			result.emplace_back(tokens[0]);
			tokens = getNextLineAndSplitIntoTokens(file);
		}

		return result;
	}


	// "tweak projections" take generated values from costs + numfire, perform swaps based on input
	void tweakProjections(vector<tuple<string, int, float, int>>& positionPlayers, int pos)
	{
		ostringstream stream;
		stream << "rankings-pos" << pos << ".csv";
		vector<string> ranks = parseRanks(stream.str());

		for (int i = 0; i < ranks.size(); i++)
		{
			auto& name = ranks[i];
			// get rank
			// if rank off by threshold, swap, swap values
			auto it = find_if(positionPlayers.begin() + i, positionPlayers.end(), [&name](tuple<string, int, float, int>& p){
				return (get<0>(p) == name);
			});
			if (it != positionPlayers.end())
			{
				int diff = distance(positionPlayers.begin() + i, it);
				if (diff >= 1)
				{
					// bring elem at it to i, shifting other elements down, get proj for this position first
					//float proj = get<2>(positionPlayers[i]);

					for (int j = i + diff; j > i; j--)
					{
						swap(positionPlayers[j], positionPlayers[j - 1]);
						float proj = get<2>(positionPlayers[j]);
						get<2>(positionPlayers[j]) = get<2>(positionPlayers[j - 1]);
						get<2>(positionPlayers[j - 1]) = proj;
						//swap(get<2>(positionPlayers[j], get<2>(positionPlayers[j + 1]);
					}

					//swap(positionPlayers[j], positionPlayers[j + 1]);
					//rotate(positionPlayers.begin() + i, it, it + 1);
					//get<2>(positionPlayers[i]) = proj;
				}
			}
		}

	}

	// or calculate?
	void importProjections(string fileout, bool tweaked, bool tweakAndDiff)
	{
		// generate csv with our format: player, cost, proj, posid
		// one option is start with analytics results, pull down
		// how are we going to customize? pull down jake ciely and "swap"

		// can use larger player sets if we remove "dominated players"
		// player is dominated if there are n (positional count) players of lesser price with higher value
		// initial set can have a threshold to let in more options for toggling

		// most helpful would be scraper for yahoo, numberfire

		// for now just assimilate data we parsed from sites
		vector<tuple<string, int, int>> costs = parseCosts("costs.csv");
		unordered_map<string, float> projections = parseProjections("projs.csv");
		unordered_map<string, float> differencing = parseProjections("diffs.csv");
		vector<tuple<string, int, float, int>> playersResult;
		for (auto& player : costs)
		{
			auto iter = projections.find(get<0>(player));
			if (iter != projections.end())
			{
				float proj = iter->second;
				if (!tweaked || tweakAndDiff)
				{
					auto itDiff = differencing.find(get<0>(player));

					// players we dont want to include can just have large negative diff
					if (itDiff != differencing.end())
					{
						proj += itDiff->second;
					}
				}
				playersResult.emplace_back(get<0>(player), get<1>(player), proj, get<2>(player));
			}
			else
			{
				cout << get<0>(player) << endl;
			}
		}


		ofstream myfile;
		myfile.open(fileout);
		// for a slot, if there is a player cheaper cost but > epsilon score, ignore
		// no def for now?
		for (int i = 0; i <= 4; i++)
		{
			vector<tuple<string, int, float, int>> positionPlayers;
			copy_if(playersResult.begin(), playersResult.end(), back_inserter(positionPlayers), [i](tuple<string, int, float, int>& p){
				return (get<3>(p) == i);
			});

			// sort by value, descending
			sort(positionPlayers.begin(), positionPlayers.end(), [](tuple<string, int, float, int>& a, tuple<string, int, float, int>& b) { return get<2>(a) > get<2>(b); });

			// don't rely much on "tweaked projections", maybe a few lineups using this method
			// more importantly, "personal tweaks" via differencing lists
			// differencing file can contain players we like/dont like for the week with adjusted projs (+2, -2, *)
			if (tweaked)
			{
				tweakProjections(positionPlayers, i);
			}
			// getRankings for tweaking

			// biggest issue is for rb/wr we dont account for how many we can use.
			for (int j = 0; j < positionPlayers.size(); j++)
			{
				// remove all players with cost > player
				//auto& p = positionPlayers[j];
				auto it = remove_if(positionPlayers.begin() + j + 1, positionPlayers.end(), [&](tuple<string, int, float, int>& a){

					// PositionCount
					static float minValue[5] = { 8, 8, 8, 5, 5 };
					return get<2>(a) < minValue[i] ||
						(count_if(positionPlayers.begin(), positionPlayers.begin() + j + 1, [&](tuple<string, int, float, int>& p){
						static float epsilon = 1;

						// probably want minvalue by pos 8,8,8,5,5?
						// probably want a bit more aggression here, if equal cost but ones player dominates the other
						// cost > current player && value < current player
						int costDiff = (get<1>(p) - get<1>(a));
						float valueDiff = get<2>(p) - get<2>(a);
						bool lessValuable = (valueDiff > epsilon);
						bool atLeastAsExpensive = costDiff <= 0;
						return (atLeastAsExpensive && lessValuable) || 
							costDiff <= -3;
					}) >= PositionCount[i]);
				});

				positionPlayers.resize(distance(positionPlayers.begin(), it));
			}

			for (auto& p : positionPlayers)
			{
				myfile << get<0>(p);
				myfile << ",";
				myfile << get<1>(p);
				myfile << ",";
				myfile << get<2>(p);
				myfile << ",";
				myfile << get<3>(p);
				myfile << ",";

				myfile << endl;
			}
		}
		myfile.close();
	}


int main(int argc, char* argv[]) {
	// for toggling lineups, we can assign "risk" to lower value plays
	// ie. quiz/gil are highest value, so less likely to toggle
	// TODO:
	// make program properly usable via cmd line (optimize x y) (import x ) so we dont run from vs all the time
	// generate x lineups
	//
	//runPlayerOptimizer("players.csv", "output.csv");
	//importProjections("players.csv", false, false);


	// optimize [file] [output]
	// import [output] /*optional?*/
	if (argc > 1)
	{
		if (strcmp(argv[1], "optimize") == 0)
		{
			string filein, fileout;
			if (argc > 2)
			{
				filein = argv[2];
			}
			else
			{
				filein = "players.csv";
			}

			if (argc > 3)
			{
				fileout = argv[3];
			}
			else
			{
				fileout = "output.csv";
			}
			runPlayerOptimizer(filein, fileout);
		}

		if (strcmp(argv[1], "optimizen") == 0)
		{
			string filein, fileout;
			if (argc > 2)
			{
				filein = argv[2];
			}
			else
			{
				filein = "players.csv";
			}

			if (argc > 3)
			{
				fileout = argv[3];
			}
			else
			{
				fileout = "output.csv";
			}
			runPlayerOptimizerN(filein, fileout);
		}

		if (strcmp(argv[1], "import") == 0)
		{
			string fileout;
			if (argc > 2)
			{
				fileout = argv[2];
			}
			else
			{
				fileout = "players.csv";
			}
			importProjections(fileout, false, false);
		}
	}
	return 0;
}
/*
int main()
{
	//runWindows();
	//runIndices();
	string s = "abcdbcd";
	//string s = "singing";
	int result = minimalModify(s);

	cout << result;
	return 0;
	}*/