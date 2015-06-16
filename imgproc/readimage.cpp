#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil/extension/io/png_io.hpp>
#include <boost/gil/extension/io/tiff_io.hpp>

#include <boost/algorithm/string/case_conv.hpp>

#include <opencv2/highgui/highgui.hpp>

#include "dbglog/dbglog.hpp"

#include "./readimage.hpp"
#include "./error.hpp"
#include "./jp2.hpp"

#ifdef IMGPROC_HAS_GIF
#  include "./gif.hpp"
#endif

namespace gil = boost::gil;
namespace ba = boost::algorithm;

namespace imgproc {

cv::Mat readImage(const void *data, std::size_t size)
{
    auto image(cv::imdecode({data, int(size)}, CV_LOAD_IMAGE_COLOR));

#ifdef IMGPROC_HAS_GIF
    if (!image.data) {
        // try gif
        try {
            image = imgproc::readGif(data, size);
        } catch (const std::runtime_error &e) {
        }
    }
#endif

    // TODO: try tiff
    return image;
}

cv::Mat readImage(const boost::filesystem::path &path)
{
    auto image(cv::imread(path.string(), CV_LOAD_IMAGE_COLOR));

#ifdef IMGPROC_HAS_GIF
    if (!image.data) {
        // try gif
        try {
            image = imgproc::readGif(path);
        } catch (const std::runtime_error &e) {
        }
    }
#endif

    // TODO: try tiff
    return image;
}

math::Size2 imageSize(const boost::filesystem::path &path)
{
    std::string ext(path.extension().string());
    ba::to_lower(ext);

    if ((ext == ".jpg") || (ext == ".jpeg")) {
        auto size(gil::jpeg_read_dimensions(path.string()));
        return { int(size.x), int(size.y) };
    }

    if (ext == ".tif") {
        auto size(gil::tiff_read_dimensions(path.string()));
        return { int(size.x), int(size.y) };
    }

    if (ext == ".png") {
        auto size(gil::png_read_dimensions(path.string()));
        return { int(size.x), int(size.y) };
    }

    if (ext == ".jp2") {
        return jp2Size(path);
    }

    if (ext == ".gif") {
#ifdef IMGPROC_HAS_GIF
        return gifSize(path);
#else
    LOGTHROW(err1, Error)
        << "Cannot determine size of image in file " << path
        << ": GIF support not compiled in.";
#endif
    }

    LOGTHROW(err1, Error)
        << "Cannot determine size of image in file " << path
        << ": Unknown file format.";
    throw;
}

} // namespace imgproc
