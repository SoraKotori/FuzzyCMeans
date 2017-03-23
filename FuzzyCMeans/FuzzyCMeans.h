#pragma once
#include <memory>

class FuzzyCMeans
{
public:
    enum class EnumInitializeMethod
    {
        Random,
        RandomMaxMin,
        OriginData
    };

    FuzzyCMeans();
    ~FuzzyCMeans();

    FuzzyCMeans(const FuzzyCMeans&);
    FuzzyCMeans &operator=(const FuzzyCMeans&);

    FuzzyCMeans(FuzzyCMeans&&);
    FuzzyCMeans &operator=(FuzzyCMeans&&);

    explicit operator bool() const;

    bool Create(double *pDataSet, int Dimension, int DataCount, int kCluster, double mWeight, double Epsilon, EnumInitializeMethod InitializeMethod);
    bool Run();

    int GetDataCluster(int DataIndex);
    double* GetCenter();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
