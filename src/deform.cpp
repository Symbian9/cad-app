
#include "deform.h"

void Deform::init(const Mesh &mesh) {
  V = mesh.V;
  F = mesh.F;
  W = mesh.W;
  constraints = {};
}

void Deform::setConstraint(int id, const Eigen::Vector3f &position) {
  constraints[id] = position;
}

void Deform::solve(Eigen::MatrixXf &U) {
  Vprime = U;
  initializeRotations();
  initializeConstraints();
  initializeLinearSystem();

  int iter = 0;
  int iterMax = 3;
  while (iter < iterMax) {
    estimateRotations();
    estimatePositions();
    iter++;
  }
  U = Vprime;
}

void Deform::initializeRotations() {
  R.clear();
  R.resize(V.cols(), Eigen::MatrixXf::Identity(3, 3));
}

void Deform::initializeConstraints() {
  freeIdxMap.resize(V.cols());
  freeIdx = 0;
  for (int i=0; i<V.cols(); ++i) {
    int idx = constraints.find(i) != constraints.end() ? -1 : freeIdx++;
    freeIdxMap[i] = idx;
  }

  for (auto i = constraints.begin(); i != constraints.end(); ++i) {
    Vprime.col(i->first) = i->second;
  }
}

void Deform::initializeLinearSystem() {
  L.resize(freeIdx, freeIdx);
  L.reserve(Eigen::VectorXi::Constant(freeIdx, 7));
  L.setZero();

  bFixed.resize(3, freeIdx);
  bFixed.setZero();

  b.resize(3, freeIdx);

  typedef Eigen::Triplet<float> T;
  std::vector<T> l_ij;
  l_ij.reserve(freeIdx * 7);

  for (int k=0; k<W.outerSize(); ++k) {
    for(Eigen::SparseMatrix<float>::InnerIterator it(W, k); it; ++it) {
      int i = it.row();
      int j = it.col();
      int idx_i = freeIdxMap[i];
      int idx_j = freeIdxMap[j];
      float w_ij = it.value();

      if (idx_i == -1) {
        continue;
      }

      if (idx_j == -1) {
        bFixed.col(idx_i) += w_ij * constraints[j];
      } else {
        l_ij.push_back(T(idx_i, idx_j, -w_ij));
      }
      l_ij.push_back(T(idx_i, idx_i, w_ij));
    }
  }

  L.setFromTriplets(l_ij.begin(), l_ij.end());
  // std::cout << L.size() << std::endl;
  /*
  for (int i=0; i<L.outerSize(); ++i) {
    for(Eigen::SparseMatrix<float>::InnerIterator it(L, i); it; ++it) {
      std::cout << "(" << it.row() << ","; // row index (j)
      std::cout << it.col() << ")\t"; // col index (i)
      std::cout << it.value() << std::endl;
    }
  }
  */
  solver.compute(L);
}


void Deform::estimateRotations() {
  for (int k=0; k<W.outerSize(); ++k) {
    Eigen::MatrixXf cov;
    cov = Eigen::MatrixXf::Zero(3, 3);

    for (Eigen::SparseMatrix<float>::InnerIterator it(W, k); it; ++it) {
      int i = it.row();
      int j = it.col();
      float w_ij = it.value();

      Eigen::Vector3f p_i = V.col(i);
      Eigen::Vector3f p_j = V.col(j);
      Eigen::Vector3f pp_i = Vprime.col(i);
      Eigen::Vector3f pp_j = Vprime.col(j);

      cov += w_ij * (p_i - p_j) * ((pp_i - pp_j).transpose());
    }

    // Jacobian SVD (Singular Value Decomposition)
    Eigen::JacobiSVD<Eigen::MatrixXf> svd(cov, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::MatrixXf SV = svd.matrixV();
    Eigen::MatrixXf SU = svd.matrixU().transpose();
    Eigen::MatrixXf I = Eigen::MatrixXf::Identity(3, 3);
    I(2, 2) = (SV * SU).determinant();

    // Rotation matrix for vertex k
    R[k] = (SV * I * SU);
  }
}

void Deform::estimatePositions() {
  b = bFixed;

  for (int k=0; k<W.outerSize(); ++k) {
    for(Eigen::SparseMatrix<float>::InnerIterator it(W, k); it; ++it) {
      int i = it.row();
      int j = it.col();
      int idx_i = freeIdxMap[i];
      if (idx_i == -1) {
        continue;
      }

      float w_ij = it.value();
      Eigen::MatrixXf r = R[i] + R[j];
      Eigen::Vector3f p_i = V.col(i);
      Eigen::Vector3f p_j = V.col(j);
      Eigen::Vector3f p = r * (p_i - p_j) * w_ij * 0.5;
      b.col(idx_i) += p;
    }
  }

  // Solve Lp' = b
  Eigen::Matrix<float, Eigen::Dynamic, 1> LU;
  for (int i=0; i<3; ++i) {
    LU = solver.solve(b.row(i).transpose());
    int idx = 0;
    for (int j=0; j < V.cols(); ++j) {
      if (freeIdxMap[j] != -1) {
        Vprime(i, j) = LU(idx++);
      }
    }
  }
  // std::cout << Vprime << std::endl;
}
