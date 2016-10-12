/**
 * @file scattered-interpolation.hpp
 * @author Jakub Cerveny <jakub.cerveny@ext.citationtech.net>
 *
 * Interpolation on 2D scattered data.
 */

#ifndef imgproc_scattered_interpolation_hpp_included_
#define imgproc_scattered_interpolation_hpp_included_

#include <opencv2/core/core.hpp>

#include "utility/gccversion.hpp"

#include "./rastermask.hpp"

namespace imgproc {

/** Solves the boundary value problem -\Delta u = 0 on elements in the matrix
 *  'data' that correspond to unset elements in 'mask'. Elements corresponding
 *  to set positions in 'mask' are regarded as given data.
 *
 *  The method is described in section 3.8 "Laplace Interpolation" of
 *  Numerical Recipes in C, Third Edition.
 *
 *  NOTE: 'data' must be of type CV_32F.
 */
void laplaceInterpolate(cv::Mat &data, const imgproc::RasterMask &mask,
                        double tol = 1e-12);
#if !defined(IMGPROC_HAS_OPENCV) || !defined(IMGPROC_HAS_EIGEN3)
    UTILITY_FUNCTION_ERROR("Laplace interpolation is available only when compiled with both OpenCV and Eigen3 libraries.")
#endif

} // imgproc

#endif // imgproc_scattered_interpolation_hpp_included_
