#include "FuzzyCMeans.h"
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace std;

class FuzzyCMeans::Impl
{
public:
    Impl() = default;

    ~Impl() = default;

    Impl(const Impl &impl) :
        m_pUMatrix(make_unique<double[]>(impl.m_DataCount * impl.m_kCluster)),
        m_pUMatrixExponent(make_unique<double[]>(impl.m_DataCount)),
        m_pCenter(make_unique<double[]>(impl.m_Dimension * impl.m_kCluster)),
        m_pDistance(make_unique<double[]>(impl.m_kCluster)),
        m_pDataCluster(make_unique<int[]>(impl.m_DataCount)),
        m_pCenterDataCount(make_unique<int[]>(impl.m_kCluster)),
        m_pDataSet(impl.m_pDataSet),
        m_Dimension(impl.m_Dimension),
        m_DataCount(impl.m_DataCount),
        m_kCluster(impl.m_kCluster),
        m_CenterCount(impl.m_CenterCount),
        m_mWeight(impl.m_mWeight),
        m_Exponent(impl.m_Exponent),
        m_LastJ(impl.m_LastJ),
        m_Epsilon(impl.m_Epsilon)
    {
        memcpy(m_pUMatrix.get(), impl.m_pUMatrix.get(), sizeof(double) * impl.m_DataCount * impl.m_kCluster);
        memcpy(m_pUMatrixExponent.get(), impl.m_pUMatrixExponent.get(), sizeof(double) * impl.m_DataCount);
        memcpy(m_pCenter.get(), impl.m_pCenter.get(), sizeof(double) * impl.m_Dimension * impl.m_kCluster);
        memcpy(m_pDistance.get(), impl.m_pDistance.get(), sizeof(double) * impl.m_kCluster);
        memcpy(m_pDataCluster.get(), impl.m_pDataCluster.get(), sizeof(int) * impl.m_DataCount);
        memcpy(m_pCenterDataCount.get(), impl.m_pCenterDataCount.get(), sizeof(int) * impl.m_kCluster);
    }

    Impl &operator=(const Impl &impl)
    {
        if (this != &impl)
        {
            Impl TemporaryImpl(impl);
            *this = move(TemporaryImpl);
        }

        return *this;
    }

    Impl(Impl&&) = default;

    Impl &operator=(Impl&&) = default;

    explicit operator bool() const
    {
        return nullptr != m_pUMatrix;
    }

    bool Create(double *pDataSet, int Dimension, int DataCount, int kCluster, double mWeight, double Epsilon, EnumInitializeMethod InitializeMethod)
    {
        if (nullptr == pDataSet)
        {
            return false;
        }

        if (0 >= Dimension)
        {
            return false;
        }

        if (0 >= DataCount)
        {
            return false;
        }

        if (0 >= kCluster || DataCount < kCluster)
        {
            return false;
        }

        if (1.0 == mWeight)
        {
            return false;
        }

        if (0.0 > Epsilon)
        {
            return false;
        }

        try
        {
            auto pUMatrix(make_unique<double[]>(DataCount * kCluster));
            auto pUMatrixExponent(make_unique<double[]>(DataCount));
            auto pCenter(make_unique<double[]>(Dimension * kCluster));
            auto pDistance(make_unique<double[]>(kCluster));
            auto pDataCluster(make_unique<int[]>(DataCount));
            auto pCenterDataCount(make_unique<int[]>(kCluster));

            m_pUMatrix = move(pUMatrix);
            m_pUMatrixExponent = move(pUMatrixExponent);
            m_pCenter = move(pCenter);
            m_pDistance = move(pDistance);
            m_pDataCluster = move(pDataCluster);
            m_pCenterDataCount = move(pCenterDataCount);
        }
        catch (const std::exception&)
        {
            return false;
        }

        m_pDataSet = pDataSet;
        m_Dimension = Dimension;
        m_DataCount = DataCount;

        m_kCluster = kCluster;

        m_mWeight = mWeight;
        m_Exponent = 2.0 / (mWeight - 1.0);

        m_Epsilon = Epsilon;

        UMatrixInitialize(InitializeMethod);
        return true;
    }

    bool Run()
    {
        if (false == bool(*this))
        {
            return false;
        }

        CenterCalculate();
        UMatrixMembership();
        double ThisJ = UMatrixObjective();
        bool Run = !(fabs(m_LastJ - ThisJ) < m_Epsilon && m_kCluster == m_CenterCount);

        m_LastJ = ThisJ;
        return Run;
    }

    int GetDataCluster(int DataIndex)
    {
        if (false == bool(*this) || m_DataCount <= DataIndex)
        {
            return -1;
        }

        return m_pDataCluster[DataIndex];
    }

    double* GetCenter()
    {
        return bool(*this) ? m_pCenter.get() : nullptr;
    }

private:
    unique_ptr<double[]> m_pUMatrix;
    unique_ptr<double[]> m_pUMatrixExponent;
    unique_ptr<double[]> m_pCenter;
    unique_ptr<double[]> m_pDistance;
    unique_ptr<int[]> m_pDataCluster;
    unique_ptr<int[]> m_pCenterDataCount;

    double *m_pDataSet = nullptr;
    int m_Dimension = 0;
    int m_DataCount = 0;

    int m_kCluster = 0;
    int m_CenterCount = 0;

    double m_mWeight = 0.0;
    double m_Exponent = 0.0;

    double m_LastJ = 0.0;
    double m_Epsilon = 0.0;

    void EnumInitializeMethodRandom()
    {
        for (int DataIndex = 0; DataIndex < m_DataCount; DataIndex++)
        {
            double RandomSum = 0.0;
            for (int ClusterIndex = 0; ClusterIndex < m_kCluster; ClusterIndex++)
            {
                double Random = static_cast<double>(rand());
                RandomSum += Random;

                m_pUMatrix[DataIndex * m_kCluster + ClusterIndex] = Random;
            }

            for (int ClusterIndex = 0; ClusterIndex < m_kCluster; ClusterIndex++)
            {
                m_pUMatrix[DataIndex * m_kCluster + ClusterIndex] /= RandomSum;
            }
        }
    }

    void EnumInitializeMethodRandomMaxMin()
    {
        for (int ClusterIndex = 0; ClusterIndex < m_kCluster; ClusterIndex++)
        {
            for (int DimensionIndex = 0; DimensionIndex < m_Dimension; DimensionIndex++)
            {
                double Max = DBL_MIN;
                double Min = DBL_MAX;

                for (int DataIndex = 0; DataIndex < m_DataCount; DataIndex++)
                {
                    if (Max < m_pDataSet[DataIndex * m_Dimension + DimensionIndex])
                    {
                        Max = m_pDataSet[DataIndex * m_Dimension + DimensionIndex];
                    }

                    if (Min > m_pDataSet[DataIndex * m_Dimension + DimensionIndex])
                    {
                        Min = m_pDataSet[DataIndex * m_Dimension + DimensionIndex];
                    }
                }

                double Random = static_cast<double>(rand());
                m_pCenter[ClusterIndex * m_Dimension + DimensionIndex] = fmod(Random, Max - Min) + Min;
            }
        }
    }

    void EnumInitializeMethodOriginData()
    {
        for (int ClusterIndex = 0; ClusterIndex < m_kCluster; ClusterIndex++)
        {
            int RandomDataIndex = rand() % m_DataCount;
            for (int DimensionIndex = 0; DimensionIndex < m_Dimension; DimensionIndex++)
            {
                m_pCenter[ClusterIndex * m_Dimension + DimensionIndex] = m_pDataSet[RandomDataIndex * m_Dimension + DimensionIndex];
            }
        }
    }

    void UMatrixInitialize(EnumInitializeMethod InitializeMethod)
    {
        switch (InitializeMethod)
        {
        case EnumInitializeMethod::Random:
            EnumInitializeMethodRandom();
            break;

        case EnumInitializeMethod::RandomMaxMin:
            EnumInitializeMethodRandomMaxMin();
            UMatrixMembership();
            break;

        case EnumInitializeMethod::OriginData:
            EnumInitializeMethodOriginData();
            UMatrixMembership();
            break;
        }
    }

    void UMatrixMembership()
    {
        memset(m_pCenterDataCount.get(), 0, sizeof(*m_pCenterDataCount.get()) * m_kCluster);

        for (int DataIndex = 0; DataIndex < m_DataCount; DataIndex++)
        {
            int DataClusterIndex = -1;
            for (int ClusterIndex = 0; ClusterIndex < m_kCluster; ClusterIndex++)
            {
                double Distance = 0.0;
                for (int DimensionIndex = 0; DimensionIndex < m_Dimension; DimensionIndex++)
                {
                    Distance += pow(m_pDataSet[DataIndex * m_Dimension + DimensionIndex] - m_pCenter[ClusterIndex * m_Dimension + DimensionIndex], 2.0);
                }

                if (0.0 == Distance)
                {
                    DataClusterIndex = ClusterIndex;
                    break;
                }

                m_pDistance[ClusterIndex] = sqrt(Distance);
            }

            double *pUMatrix = &m_pUMatrix[DataIndex * m_kCluster];
            if (-1 == DataClusterIndex)
            {
                DataClusterIndex = 0;
                for (int OuterClusterIndex = 0; OuterClusterIndex < m_kCluster; OuterClusterIndex++)
                {
                    double Sum = 0.0;
                    for (int InnerClusterIndex = 0; InnerClusterIndex < m_kCluster; InnerClusterIndex++)
                    {
                        Sum += pow(m_pDistance[OuterClusterIndex] / m_pDistance[InnerClusterIndex], m_Exponent);
                    }

                    pUMatrix[OuterClusterIndex] = 1.0 / Sum;
                    if (pUMatrix[DataClusterIndex] < pUMatrix[OuterClusterIndex])
                    {
                        DataClusterIndex = OuterClusterIndex;
                    }
                }
            }
            else //Prevent division by zero
            {
                memset(pUMatrix, 0, sizeof(*pUMatrix) * m_kCluster);
                pUMatrix[DataClusterIndex] = 1.0;
            }

            m_pDataCluster[DataIndex] = DataClusterIndex;
            m_pCenterDataCount[DataClusterIndex]++;
        }
    }

    void CenterCalculate()
    {
        for (int ClusterIndex = 0; ClusterIndex < m_kCluster; ClusterIndex++)
        {
            double Denominator = 0.0;
            for (int DataIndex = 0; DataIndex < m_DataCount; DataIndex++)
            {
                double UMatrixExponent = pow(m_pUMatrix[DataIndex * m_kCluster + ClusterIndex], m_mWeight);
                Denominator += UMatrixExponent;

                m_pUMatrixExponent[DataIndex] = UMatrixExponent;
            }

            for (int DimensionIndex = 0; DimensionIndex < m_Dimension; DimensionIndex++)
            {
                double Numerator = 0.0;
                for (int DataIndex = 0; DataIndex < m_DataCount; DataIndex++)
                {
                    Numerator += m_pUMatrixExponent[DataIndex] * m_pDataSet[DataIndex * m_Dimension + DimensionIndex];
                }

                m_pCenter[ClusterIndex * m_Dimension + DimensionIndex] = Numerator / Denominator;
            }
        }
    }

    double UMatrixObjective()
    {
        m_CenterCount = 0;

        double JObjective = 0.0;
        for (int ClusterIndex = 0; ClusterIndex < m_kCluster; ClusterIndex++)
        {
            for (int DataIndex = 0; DataIndex < m_DataCount; DataIndex++)
            {
                double Distance = 0.0;
                for (int DimensionIndex = 0; DimensionIndex < m_Dimension; DimensionIndex++)
                {
                    Distance += pow(m_pDataSet[DataIndex * m_Dimension + DimensionIndex] - m_pCenter[ClusterIndex * m_Dimension + DimensionIndex], 2.0);
                }

                JObjective += pow(m_pUMatrix[DataIndex * m_kCluster + ClusterIndex], m_mWeight) * Distance;
            }

            if (0 != m_pCenterDataCount[ClusterIndex])
            {
                m_CenterCount++;
            }
        }

        return JObjective;
    }
};

FuzzyCMeans::FuzzyCMeans()
    : pImpl(make_unique<Impl>())
{
}

FuzzyCMeans::~FuzzyCMeans() = default;

FuzzyCMeans::FuzzyCMeans(const FuzzyCMeans &FCM) :
    pImpl(make_unique<Impl>(*FCM.pImpl))
{
}

FuzzyCMeans & FuzzyCMeans::operator=(const FuzzyCMeans &FCM)
{
    *pImpl = *FCM.pImpl;
    return *this;
}

FuzzyCMeans::FuzzyCMeans(FuzzyCMeans&&) = default;

FuzzyCMeans &FuzzyCMeans::operator=(FuzzyCMeans&&) = default;

FuzzyCMeans::operator bool() const
{
    return pImpl->operator bool();
}

bool FuzzyCMeans::Create(double * pDataSet, int Dimension, int DataCount, int kCluster, double mWeight, double Epsilon, EnumInitializeMethod InitializeMethod)
{
    return pImpl->Create(pDataSet, Dimension, DataCount, kCluster, mWeight, Epsilon, InitializeMethod);
}

bool FuzzyCMeans::Run()
{
    return pImpl->Run();
}

int FuzzyCMeans::GetDataCluster(int DataIndex)
{
    return pImpl->GetDataCluster(DataIndex);
}

double * FuzzyCMeans::GetCenter()
{
    return pImpl->GetCenter();
}
