#include "FuzzyCMeans.h"
#include <cstdlib>
#include <iostream>

#define Dimension 9
#define DataCount 9
#define kCluster 3
#define mWeight 2.0
#define Epsilon 1e-6

using namespace std;

int main(void)
{
    double MultiplicationTable[] =
    {
        1,  2,  3,  4,  5,  6,  7,  8,  9,
        2,  4,  6,  8, 10, 12, 14, 16, 18,
        3,  6,  9, 12, 15, 18, 21, 24, 27,
        4,  8, 12, 16, 20, 24, 28, 32, 36,
        5, 10, 15, 20, 25, 30, 35, 40, 45,
        6, 12, 18, 24, 30, 36, 42, 48, 54,
        7, 14, 21, 28, 35, 42, 49, 56, 63,
        8, 16, 24, 32, 40, 48, 56, 64, 72,
        9, 18, 27, 36, 45, 54, 63, 72, 81,
    };

    FuzzyCMeans FCM;
    bool bResult = FCM.Create(MultiplicationTable, Dimension, DataCount, kCluster, mWeight, Epsilon, FuzzyCMeans::EnumInitializeMethod::Random);
    if (false == bResult)
    {
        cout << "Create Failed" << endl;
        return EXIT_FAILURE;
    }

    int Iteration;
    for (Iteration = 0; Iteration < 2000; Iteration++)
    {
        bool Run = FCM.Run();

        cout << "Iteration : " << Iteration << endl;
        for (int DataIndex = 0; DataIndex < DataCount; DataIndex++)
        {
            cout << "Data[" << DataIndex << "] " << FCM.GetDataCluster(DataIndex) << " Cluster" << endl;
        }
        cout << endl;

        if (false == Run)
        {
            Iteration++;
            break;
        }
    }

    cout << "IterationCount : " << Iteration << endl;
    return EXIT_SUCCESS;
}
