#ifndef ANDERSONACCELERATION_H
#define ANDERSONACCELERATION_H

#include <Eigen/Eigen>
#include <Eigen/Dense>

#ifdef USE_FLOAT_SCALAR
typedef float Scalar
#else
typedef double Scalar;
#endif

typedef Eigen::SparseMatrix<Scalar, Eigen::ColMajor> ColMajorSparseMatrix;
typedef Eigen::SparseMatrix<Scalar, Eigen::RowMajor> RowMajorSparseMatrix;
typedef Eigen::Triplet<Scalar> Triplet;

#ifdef EIGEN_DONT_ALIGN
#define EIGEN_ALIGNMENT Eigen::DontAlign
#else
#define EIGEN_ALIGNMENT Eigen::AutoAlign
#endif

template<int Rows, int Cols, int Options = (Eigen::ColMajor | EIGEN_ALIGNMENT) >
using MatrixT = Eigen::Matrix<Scalar, Rows, Cols, Options>;  ///< A typedef of the dense matrix of Eigen.
typedef MatrixT<2, 1> Vector2;								///< A 2d column vector.
typedef MatrixT<2, 2> Matrix22;								///< A 2 by 2 matrix.
typedef MatrixT<2, 3> Matrix23;								///< A 2 by 3 matrix.
typedef MatrixT<3, 1> Vector3;								///< A 3d column vector.
typedef MatrixT<3, 2> Matrix32;								///< A 3 by 2 matrix.
typedef MatrixT<3, 3> Matrix33;								///< A 3 by 3 matrix.
typedef MatrixT<3, 4> Matrix34;								///< A 3 by 4 matrix.
typedef MatrixT<4, 1> Vector4;								///< A 4d column vector.
typedef MatrixT<4, 4> Matrix44;								///< A 4 by 4 matrix.
typedef MatrixT<4, Eigen::Dynamic> Matrix4X;				///< A 4 by n matrix.
typedef MatrixT<3, Eigen::Dynamic> Matrix3X;				///< A 3 by n matrix.
typedef MatrixT<Eigen::Dynamic, 3> MatrixX3;				///< A n by 3 matrix.
typedef MatrixT<2, Eigen::Dynamic> Matrix2X;				///< A 2 by n matrix.
typedef MatrixT<Eigen::Dynamic, 2> MatrixX2;				///< A n by 2 matrix.
typedef MatrixT<Eigen::Dynamic, 1> VectorX;					///< A nd column vector.
typedef MatrixT<Eigen::Dynamic, Eigen::Dynamic> MatrixXX;  ///< A n by m matrix.

class AndersonAcceleration {
public:
	AndersonAcceleration()
		: m_(-1),
		dim_(-1),
		iter_(-1),
		col_idx_(-1) {
	}

	void replace(const Scalar *u) {
		current_u_ = Eigen::Map<const VectorX>(u, dim_);
	}

	const VectorX& compute(const Scalar* g) {
		assert(iter_ >= 0);

		Eigen::Map<const VectorX> G(g, dim_);
		current_F_ = G - current_u_;

		if (iter_ == 0) {
			prev_dF_.col(0) = -current_F_;
			prev_dG_.col(0) = -G;
			current_u_ = G;
		}
		else {
			prev_dF_.col(col_idx_) += current_F_;
			prev_dG_.col(col_idx_) += G;

			Scalar eps = 1e-14;
			Scalar scale = std::max(eps, prev_dF_.col(col_idx_).norm());
			dF_scale_(col_idx_) = scale;
			prev_dF_.col(col_idx_) /= scale;

			int m_k = std::min(m_, iter_);

			if (m_k == 1) {
				theta_(0) = 0;
				Scalar dF_sqrnorm = prev_dF_.col(col_idx_).squaredNorm();
				M_(0, 0) = dF_sqrnorm;
				Scalar dF_norm = std::sqrt(dF_sqrnorm);

				if (dF_norm > eps) {
					// compute theta = (dF * F) / (dF * dF)
					theta_(0) = (prev_dF_.col(col_idx_) / dF_norm).dot(
						current_F_ / dF_norm);
				}
			}
			else {
				// Update the normal equation matrix, for the column and row corresponding to the new dF column
				VectorX new_inner_prod = (prev_dF_.col(col_idx_).transpose()
					* prev_dF_.block(0, 0, dim_, m_k)).transpose();
				M_.block(col_idx_, 0, 1, m_k) = new_inner_prod.transpose();
				M_.block(0, col_idx_, m_k, 1) = new_inner_prod;

				// Solve normal equation
				cod_.compute(M_.block(0, 0, m_k, m_k));
				theta_.head(m_k) = cod_.solve(
					prev_dF_.block(0, 0, dim_, m_k).transpose() * current_F_);
			}

			// Use rescaled theata to compute new u
			current_u_ =
				G
				- prev_dG_.block(0, 0, dim_, m_k)
				* ((theta_.head(m_k).array() / dF_scale_.head(m_k).array())
					.matrix());

			col_idx_ = (col_idx_ + 1) % m_;
			prev_dF_.col(col_idx_) = -current_F_;
			prev_dG_.col(col_idx_) = -G;
		}

		iter_++;
		return current_u_;
	}

	// m: number of previous iterations used
	// d: dimension of variables
	// u0: initial variable values
	void init(int m, int d, const Scalar* u0) {
		assert(m > 0);
		m_ = m;
		dim_ = d;
		current_u_.resize(d);
		current_F_.resize(d);
		prev_dG_.resize(d, m);
		prev_dF_.resize(d, m);
		M_.resize(m, m);
		theta_.resize(m);
		dF_scale_.resize(m);
		current_u_ = Eigen::Map<const VectorX>(u0, d);
		iter_ = 0;
		col_idx_ = 0;
	}

private:
	VectorX current_u_;
	VectorX current_F_;
	MatrixXX prev_dG_;
	MatrixXX prev_dF_;
	MatrixXX M_;		// Normal equations matrix for the computing theta
	VectorX theta_;  // theta value computed from normal equations
	VectorX dF_scale_;		// The scaling factor for each column of prev_dF
	Eigen::CompleteOrthogonalDecomposition<MatrixXX> cod_;

	int m_;		// Number of previous iterates used for Andreson Acceleration
	int dim_;  // Dimension of variables
	int iter_;	// Iteration count since initialization
	int col_idx_;  // Index for history matrix column to store the next value
};

#endif // !ANDERSONACCELERATION_H
