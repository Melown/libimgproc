#ifndef imgproc_jp2_hpp_included_
#define imgproc_jp2_hpp_included_

#include <boost/filesystem/path.hpp>

#include "math/geometry_core.hpp"

namespace imgproc {

math::Size2 jp2Size(const boost::filesystem::path &path);

} // namespace imgproc

#endif // imgproc_jp2_hpp_included_