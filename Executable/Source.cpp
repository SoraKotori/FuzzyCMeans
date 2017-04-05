#include "FuzzyCMeans.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include <fstream>
#include <sstream>
#include <cstring>
#include <utility>

#include <iostream>
#include <filesystem>

using namespace std;
using namespace std::experimental::filesystem;

template<typename _FileName, typename _Ty, typename _Size>
bool csv_read(_FileName&& __filename, vector<_Ty>& __data, _Size& __dimension, _Size& __data_count);

template<typename _FileName, typename _Ty, typename _Size>
bool csv_write(const _FileName& __filename, const vector<_Ty>& _Center, const vector<_Ty>& _Data, const vector<_Size>& _DataCluster);


template<typename _InputIterator1, typename _InputIterator2,
	typename _Type = typename _InputIterator1::value_type>
	inline auto SumSquares(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _Type __init = _Type(0))
{
	for (; __first1 != __last1; ++__first1, ++__first2)
	{
		auto __minus = *__first1 - *__first2;
		__init += __minus * __minus;
	}

	return __init;
}

template<typename... _Args>
inline auto EuclideanDistance(_Args&&... __args)
{
	return sqrt(SumSquares(forward<_Args>(__args)...));
}

template<typename _Ty, typename _Size>
auto csvDistance(const vector<_Ty>& _Center, const vector<_Ty>& _Data, const vector<_Size>& _DataCluster)
{
	decltype(auto) _DataCount = _DataCluster.size();
	decltype(auto) _Dimension = (_Data.size() / _DataCount);
	decltype(auto) _CenterCount = (_Center.size() / _Dimension);

	vector<_Size> _Count(_CenterCount, 0);
	vector<_Ty> _Distance(_CenterCount, 0);

	for (decltype(_DataCount) _DataIndex = 0; _DataIndex < _DataCount; ++_DataIndex)
	{
		decltype(auto) _DataClusterIndex = _DataCluster[_DataIndex];
		decltype(auto) _DataIndexFirst = &_Data[_DataIndex];
		decltype(auto) _CenterIndexFirst = &_Center[_DataClusterIndex];

		_Count[_DataClusterIndex]++;
		_Distance[_DataClusterIndex] +=
			EuclideanDistance(_DataIndexFirst, _DataIndexFirst + _Dimension, _CenterIndexFirst, _Ty(0));
	}

	decltype(auto) _First = _Count.begin();
	decltype(auto) _Last = _Count.end();
	decltype(auto) _Result = _Distance.begin();

	for (; _First != _Last; ++_First, ++_Result)
	{
		*_Result /= *_First;
	}

	path _Directories(to_string(_CenterCount));
	ofstream _FileDistance(_Directories / "Distance.csv");

	_FileDistance << "id" << endl;
	for (decltype(_CenterCount) _CenterIndex = 0; _CenterIndex < _CenterCount; _CenterIndex++)
	{
		_FileDistance << _CenterIndex + 1;
		_FileDistance << ',' << _Distance[_CenterIndex];
		_FileDistance << endl;;
	}

	return move(_Distance);
}
int main(int argc, char *argv[])
{
	if (6 != argc)
	{
		return EXIT_FAILURE;
	}

	auto _FileName = string(argv[1]);
	auto _OutputFileName = string(argv[2]);
	auto _kCluster = atoi(argv[3]);
	auto _mWeight = strtod(argv[4], nullptr);
	auto _Epsilon = strtod(argv[5], nullptr);

	srand((unsigned)time(NULL));
	vector<double> _Data;
	int _Dimension;
	int _DataSize;

	bool _bResult = csv_read(_FileName, _Data, _Dimension, _DataSize);
	if (false == _bResult)
	{
		return EXIT_FAILURE;
	}

	FuzzyCMeans _FCM;
	_bResult = _FCM.Create(_Data.data(), _Dimension, _DataSize, _kCluster, _mWeight, _Epsilon, FuzzyCMeans::EnumInitializeMethod::OriginData);
	if (false == _bResult)
	{
		return EXIT_FAILURE;
	}

	int Iteration;
	for (Iteration = 0; Iteration < 100000 && _FCM.Run(); Iteration++);

	vector<int> _DataCluster(_DataSize);
	for (int Index = 0; Index < _DataSize; _DataCluster[Index] = _FCM.GetDataCluster(Index), Index++);

	auto _Center = [&, _pCenter = _FCM.GetCenter()]
	{ return vector<double>(_pCenter, _pCenter + _kCluster * _Dimension); }();

	_bResult = csv_write(_OutputFileName, _Center, _Data, _DataCluster);
	if (false == _bResult)
	{
		return EXIT_FAILURE;
	}

	decltype(auto) _Distance = csvDistance(_Center, _Data, _DataCluster);
	copy(_Distance.begin(), _Distance.end(), ostream_iterator<double>(cout, "\n"));


	return EXIT_SUCCESS;
}

class csv_ifstream : virtual public ifstream
{
public:
	template<typename... _Args>
	explicit csv_ifstream(_Args&&... __args) : ifstream(forward<_Args>(__args)...)
	{
		struct csv_type : ctype<char>
		{
			static const mask* get_table()
			{
				mask* __csv_table = new mask[table_size];
				memcpy(__csv_table, classic_table(), sizeof(mask) * table_size);

				__csv_table[','] |= space;
				return __csv_table;
			}

			csv_type(size_t __refs = 0U) : ctype(get_table(), true, __refs)
			{ }
		};

		imbue(locale(getloc(), new csv_type));
	}
};

template<typename _FileName, typename _Ty, typename _Size>
bool csv_read(_FileName&& __filename, vector<_Ty>& __data, _Size& __dimension, _Size& __data_count)
{
	typedef typename vector<_Ty>::size_type size_type;

	csv_ifstream __file(forward<_FileName>(__filename));
	if (!__file)
	{
		return false;
	}

	size_type __comma = 0;
	size_type __line = 0;

	for (auto __char = __file.get(); EOF != __char; __char = __file.get())
	{
		switch (__char)
		{
		case ',':
			__comma++;
			break;
		case '\n':
			__line++;
			break;
		}
	}

	if (0 == __comma || 0 == __line)
	{
		return false;
	}

	__file.clear();
	__file.seekg(0, __file.beg);

	string _FirstLine;
	getline(__file, _FirstLine);

	size_type __fdimension = __comma / __line;
	size_type __fdata_count = __line - 1;
	vector<_Ty> __fdata(__fdimension * __fdata_count);

	auto __it_fdata = __fdata.begin();
	for (size_type __fdata_index = 0; __fdata_index < __fdata_count; __fdata_index++)
	{
		_Ty __data_id;
		__file >> __data_id;

		__it_fdata = copy_n(istream_iterator<_Ty>(__file), __fdimension, __it_fdata);
	}

	__data = move(__fdata);
	__dimension = static_cast<_Size>(__fdimension);
	__data_count = static_cast<_Size>(__fdata_count);

	return true;
}

template<typename _FileName, typename _Ty, typename _Size>
bool csv_write(const _FileName& __filename, const vector<_Ty>& _Center, const vector<_Ty>& _Data, const vector<_Size>& _DataCluster)
{
	typedef typename vector<_Ty>::size_type size_type;
	auto _DataCount = static_cast<size_type>(_DataCluster.size());
	auto _Dimension = _Data.size() / _DataCount;
	auto _ClusterCount = _Center.size() / _Dimension;

	path _Directories(to_string(_ClusterCount));
	bool bResult = is_directory(_Directories);
	if (!bResult)
	{
		bResult = create_directories(_Directories);
		if (!bResult)
		{
			return false;
		}
	}

	ofstream _FileCenter(_Directories / "Center.csv");
	if (!_FileCenter)
	{
		return false;
	}

	ofstream _FileOutput(_Directories / __filename);
	if (!_FileOutput)
	{
		return false;
	}

	vector<ofstream> _FileCluster(_ClusterCount);
	for (decltype(_ClusterCount) _ClusterIndex = 0; _ClusterIndex < _ClusterCount; _ClusterIndex++)
	{
		ofstream _FileClusterIndex(_Directories / (to_string(_ClusterIndex) + ".csv"));
		if (!_FileClusterIndex)
		{
			return false;
		}

		_FileCluster[_ClusterIndex] = move(_FileClusterIndex);
	}

	_FileCenter << "id";
	_FileOutput << "id";
	for (size_type _DimensionIndex = 0; _DimensionIndex < _Dimension; _DimensionIndex++)
	{
		auto _FirstLine = string(",dim") + to_string(_DimensionIndex + 1);

		_FileCenter << _FirstLine;
		_FileOutput << _FirstLine;
	}
	_FileCenter << endl;
	_FileOutput << ',' << "cid" << endl;;

	auto _CenterFirst = _Center.begin();
	for (decltype(_ClusterCount) _ClusterIndex = 0; _ClusterIndex < _ClusterCount; _ClusterIndex++)
	{
		decltype(auto) _FileClusterIndex = _FileCluster[_ClusterIndex];

		_FileCenter << _ClusterIndex + 1;
		_FileClusterIndex << "id";
		for (size_type _DimensionIndex = 0; _DimensionIndex < _Dimension; _DimensionIndex++)
		{
			_FileCenter << ',' << *_CenterFirst++;
			_FileClusterIndex << ',' << "dim" << _DimensionIndex + 1;
		}
		_FileCenter << endl;;
		_FileClusterIndex << ',' << "cid" << endl;;
	}

	auto _DataFirst = _Data.begin();
	for (size_type _DataIndex = 0; _DataIndex < _DataCount; _DataIndex++)
	{
		decltype(auto) _DataClusterIndex = _DataCluster[_DataIndex];
		decltype(auto) _FileClusterIndex = _FileCluster[_DataClusterIndex];

		auto _DataIndexAdd = _DataIndex + 1;
		_FileOutput << _DataIndexAdd;
		_FileClusterIndex << _DataIndexAdd;
		for (size_type _DimensionIndex = 0; _DimensionIndex < _Dimension; _DimensionIndex++)
		{
			auto _DataValue = *_DataFirst++;

			_FileOutput << ',' << _DataValue;
			_FileClusterIndex << ',' << _DataValue;
		}
		_FileOutput << ',' << _DataClusterIndex << endl;
		_FileClusterIndex << ',' << _DataClusterIndex << endl;
	}

	return true;
}
