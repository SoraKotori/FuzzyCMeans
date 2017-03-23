#include "FuzzyCMeans.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include <fstream>
#include <sstream>
#include <cstring>

#include <iostream>
#include <filesystem>

using namespace std;
using namespace std::experimental::filesystem;

#define ICAL

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

#ifdef ICAL
    string _FirstLine;
    getline(__file, _FirstLine);
#endif // ICAL

    size_type __fdimension = __comma / __line + 1;
    size_type __fdata_count = __line;

#ifdef ICAL
    __fdimension -= 1;
    __fdata_count -= 1;
#endif // ICAL

    vector<_Ty> __fdata(__fdimension * __fdata_count);

#ifdef ICAL
    auto __it_fdata = __fdata.begin();
    for (size_type __fdata_index = 0; __fdata_index < __fdata_count; __fdata_index++)
    {
        _Ty __data_id;
        __file >> __data_id;

        __it_fdata = copy_n(istream_iterator<_Ty>(__file), __fdimension, __it_fdata);
    }
#else
    copy_n(istream_iterator<_Ty>(__file), __fdata.size(), __fdata.begin());
#endif // ICAL

    __data = move(__fdata);
    __dimension = static_cast<_Size>(__fdimension);
    __data_count = static_cast<_Size>(__fdata_count);

    return true;
}

template<typename _FileName, typename _Ty, typename _Size>
bool csv_write(const _FileName& __filename, const vector<_Ty>& __center, const vector<_Ty>& __data, const vector<_Size>& __data_cluster, _Size __cluster)
{
    typedef typename vector<_Ty>::size_type size_type;
    size_type __line = static_cast<size_type>(__data_cluster.size());
    size_type __dimension = __data.size() / __line;

    ostringstream __cluster_string;
    __cluster_string << __cluster;
    path __file_directories(__cluster_string.str());

    bool bResult = is_directory(__file_directories);
    if (!bResult)
    {
        bResult = create_directories(__file_directories);
        if (!bResult)
        {
            return false;
        }
    }

    path __file_path = __file_directories / "Center.csv";
    ofstream __file_center(__file_path);
    if (!__file_center)
    {
        return false;
    }

    __file_path = __file_directories / __filename;
    ofstream __file(__file_path);
    if (!__file)
    {
        return false;
    }

    vector<ofstream> __file_cluster(__cluster);
    for (_Size __cluster_index = 0; __cluster_index < __cluster; __cluster_index++)
    {
        __cluster_string.seekp(0, ios::beg);
        __cluster_string << __cluster_index << ".csv";
        __file_path = __file_directories / __cluster_string.str();

        __file_cluster[__cluster_index] = ofstream(__file_path);
        if (!__file_cluster[__cluster_index])
        {
            return false;
        }
    }

    __file_center << "id";
    __file << "id";
    for (size_type __dimension_index = 0; __dimension_index < __dimension; __dimension_index++)
    {
        auto __dimension_index_add = __dimension_index + 1;

        __file_center << ',' << "dim" << __dimension_index_add;
        __file << ',' << "dim" << __dimension_index_add;
    }
    __file_center << endl;;
    __file << ',' << "cid" << endl;;

    auto __center_begin = __center.begin();
    for (_Size __cluster_index = 0; __cluster_index < __cluster; __cluster_index++)
    {
        auto& __file_cluster_index = __file_cluster[__cluster_index];
        
        __file_center << __cluster_index + 1;
        __file_cluster_index << "id";
        for (size_type __dimension_index = 0; __dimension_index < __dimension; __dimension_index++)
        {
            __file_center << ',' << *__center_begin++;
            __file_cluster_index << ',' << "dim" << __dimension_index + 1;
        }
        __file_center << endl;;
        __file_cluster_index << ',' << "cid" << endl;;
    }

    auto __data_begin = __data.begin();
    for (size_type __data_index = 0; __data_index < __line; __data_index++)
    {
        auto __cluster_index = __data_cluster[__data_index];
        auto& __file_cluster_index = __file_cluster[__cluster_index];

        auto __data_index_add = __data_index + 1;
        __file << __data_index_add;
        __file_cluster_index << __data_index_add;

        for (size_type __dimension_index = 0; __dimension_index < __dimension; __dimension_index++)
        {
            auto __data_value = *__data_begin++;

            __file << ',' << __data_value;
            __file_cluster_index << ',' << __data_value;
        }
        __file << ',' << __cluster_index << endl;
        __file_cluster_index << ',' << __cluster_index << endl;
    }

    return true;
}

int main(int argc, char *argv[])
{
    if (6 != argc)
    {
        return EXIT_FAILURE;
    }

    string _FileName = string(argv[1]);
    string _OutputFileName = string(argv[2]);

    int _kCluster;
    istringstream(string(argv[3])) >> _kCluster;

    double _mWeight;
    istringstream(string(argv[4])) >> _mWeight;

    double _Epsilon;
    istringstream(string(argv[5])) >> _Epsilon;

    vector<double> _Data;
    int _Dimension;
    int _DataSize;

    bool _bResult = csv_read(_FileName, _Data, _Dimension, _DataSize);
    if (false == _bResult)
    {
        cout << ".csv Read Failed" << endl;
        return EXIT_FAILURE;
    }

    srand(time(NULL));
    FuzzyCMeans _FCM;
    _bResult = _FCM.Create(_Data.data(), _Dimension, _DataSize, _kCluster, _mWeight, _Epsilon, FuzzyCMeans::EnumInitializeMethod::OriginData);
    if (false == _bResult)
    {
        cout << "Create Failed" << endl;
        return EXIT_FAILURE;
    }

    int Iteration;
    for (Iteration = 0; Iteration < 100000; Iteration++)
    {
        bool Run = _FCM.Run();

#ifndef ICAL
        cout << "Iteration : " << Iteration << endl;
        for (int DataIndex = 0; DataIndex < _DataSize; DataIndex++)
        {
            cout << "Data[" << DataIndex << "] " << _FCM.GetDataCluster(DataIndex) << " Cluster" << endl;
        }
        cout << endl;
#endif // !ICAL

        if (false == Run)
        {
            Iteration++;
            break;
        }
    }

    vector<int> _DataCluster(_DataSize);

    cout << "IterationCount : " << Iteration << endl;
    for (int _DataIndex = 0; _DataIndex < _DataSize; _DataIndex++)
    {
        auto _Cluster = _FCM.GetDataCluster(_DataIndex);
        _DataCluster[_DataIndex] = _Cluster;

        cout << "Data[" << _DataIndex << "] " << _Cluster << " Cluster" << endl;
    }
    cout << endl;

    double* _pCenter = _FCM.GetCenter();
    vector<double> _Center(_pCenter, _pCenter + sizeof(double) * _kCluster * _Dimension);

    _bResult = csv_write(_OutputFileName, _Center, _Data, _DataCluster, _kCluster);
    if (false == _bResult)
    {
        cout << "Create Failed" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
