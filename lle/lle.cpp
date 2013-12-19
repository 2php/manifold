#include "lle.h"
#include "stddef.h"
#include <vector>
#include <cstdlib>

// Unfortunately, Eigen *still* does not provide sparse eigenvalue solvers...
//    so we're doing everything dense for now.

#include "Eigen/Core"
#include "Eigen/Dense"
// #include "Eigen/SparseCore"
#include "Eigen/Eigenvalues"

using namespace std;
using namespace Eigen;

typedef unsigned int UINT;
// typedef SparseMatrix<double> SparseMatrixd;
typedef vector< vector<int> > NeighborList;
// typedef Triplet<double> Tripletd;

class NeighborWithDist
{
public:
	int index;
	double dist;
	NeighborWithDist() {}
	NeighborWithDist(int i, double d) : index(i), dist(d) {}
	inline bool operator < (const NeighborWithDist& other) const
	{
		return this->dist < other.dist;
	}
};

void computeNeighbors(int k, const vector<VectorXd>& inData, NeighborList& outNeighbors)
{
	// For now, just brute force KNN
	outNeighbors.resize(inData.size());
	vector<NeighborWithDist> neighborDists(inData.size()-1);
	for (UINT i = 0; i < inData.size(); i++)
	{
		for (UINT j = 0; j < inData.size(); j++)
		{
			int currIndex = 0;
			if (i != j)
			{
				double distSq = (inData[i] - inData[j]).squaredNorm();
				neighborDists[currIndex] = NeighborWithDist(j, distSq);
				currIndex++;
			}
		}
		sort(neighborDists.begin(), neighborDists.end());
		for (int j = 0; j < k; j++)
			outNeighbors[i].push_back(neighborDists[j].index);
	}
}

void computeReconstructionWeights(const vector<VectorXd>& inData, const NeighborList& inNeighbors, MatrixXd& outW)
{
	int dim = inData[0].size();
	int k = inNeighbors[0].size();
	// vector<Tripletd> sparseWeights;
	outW = MatrixXd::Zero(outW.rows(), outW.cols());
	for (UINT i = 0; i < inData.size(); i++)
	{
		MatrixXd Z(dim,k);
		const VectorXd& Xi = inData[i];
		for (UINT j = 0; j < k; j++)
		{
			const VectorXd& Xnj = inData[inNeighbors[i][j]];
			Z.col(j) = Xnj - Xi;
		}
		MatrixXd C = Z.transpose() * Z;
		// Have to regularize if num neighbors is greater than the space dimension
		if (k > dim)
		{
			double eps = 1e-3*C.trace();
			C += MatrixXd::Identity(k, k) * eps;
		}
		VectorXd w = C.ldlt().solve(VectorXd::Ones(dim));
		double wsum = w.sum();
		for (UINT j = 0; j < k; j++)
			outW(i,inNeighbors[i][j]) = w[i]/wsum;
		// 	sparseWeights.push_back(Tripletd(i, inNeighbors[i][j], w[j]/wsum));
	}
	// outW.setFromTriplets(sparseWeights.begin(), sparseWeights.end());
}

void computeEmbeddingCoords(int outDim, const MatrixXd& inW, MatrixXd& outData)
{
	// SparseMatrixd I(inW.rows(), inW.cols());
	// vector<Tripletd> identTriplets;
	// for (UINT i = 0; i < inW.rows(); i++)
	// 	identTriplets.push_back(Tripletd(i, i, 1.0));
	// I.setFromTriplets(identTriplets.begin(), identTriplets.end());
	// SparseMatrixd IminusW = I - inW;
	// SparseMatrixd M = (IminusW.transpose() * IminusW).pruned();

	MatrixXd IminusW = MatrixXd::Identity(inW.rows(), inW.cols()) - inW;
	MatrixXd M = IminusW.transpose()*IminusW;
	SelfAdjointEigenSolver<MatrixXd> eigensolver(M);
	MatrixXd eigenvectors = eigensolver.eigenvectors();
	for (UINT i = 1; i < outDim+1; i++)
		outData.row(i-1) = eigenvectors.col(i);
}

extern "C"
{
	EXPORT double* LLE(int inDim, int outDim, int numPoints, int k, double* data)
	{
		// Convert data to Eigen format
		vector<VectorXd> dataSet(numPoints);
		for (int i = 0; i < numPoints; i++)
		{
			double* dataP = data + i*inDim;
			VectorXd v(inDim);
			for (int d = 0; d < inDim; d++)
				v[d] = dataP[d];
			dataSet[i] = v;
		}

		// Run LLE
		NeighborList Ns;
		MatrixXd W(numPoints, numPoints);
		MatrixXd outData(outDim, numPoints);
		computeNeighbors(k, dataSet, Ns);
		computeReconstructionWeights(dataSet, Ns, W);
		computeEmbeddingCoords(outDim, W, outData);

		// Convert data back to interchange format
		// Important to use malloc, since the Terra side will free it
		double* returnData = (double*)malloc(outDim*numPoints*sizeof(double));
		for (int i = 0; i < numPoints; i++)
		{
			const VectorXd& v = outData.col(i);
			double* returnDataP = returnData + i*outDim;
			for (int d = 0; d < outDim; d++)
				returnDataP[d] = v[d];
		}
		return returnData;
	}
}





