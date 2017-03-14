#include "precomp.hpp"

#include "grfmt_base.hpp"
#include "bitstrm.hpp"

namespace cv
{

BaseImageDecoder::BaseImageDecoder()
{
    m_width = m_height = 0;
    m_type = -1;
    m_buf_supported = false;
}

bool BaseImageDecoder::setSource( const string& filename )
{
    m_filename = filename;
    m_buf.release();
    return true;
}

bool BaseImageDecoder::setSource( const Mat& buf )
{
    if( !m_buf_supported )
        return false;
    m_filename = string();
    m_buf = buf;
    return true;
}

size_t BaseImageDecoder::signatureLength() const
{
    return m_signature.size();
}

bool BaseImageDecoder::checkSignature( const string& signature ) const
{
    size_t len = signatureLength();
    return signature.size() >= len && memcmp( signature.c_str(), m_signature.c_str(), len ) == 0;
}

ImageDecoder BaseImageDecoder::newDecoder() const
{
    return ImageDecoder();
}

BaseImageEncoder::BaseImageEncoder()
{
    m_buf_supported = false;
}

bool  BaseImageEncoder::isFormatSupported( int depth ) const
{
    return depth == CV_8U;
}

string BaseImageEncoder::getDescription() const
{
    return m_description;
}

bool BaseImageEncoder::setDestination( const string& filename )
{
    m_filename = filename;
    m_buf = 0;
    return true;
}

bool BaseImageEncoder::setDestination( vector<uchar>& buf )
{
    if( !m_buf_supported )
        return false;
    m_buf = &buf;
    m_buf->clear();
    m_filename = string();
    return true;
}

ImageEncoder BaseImageEncoder::newEncoder() const
{
    return ImageEncoder();
}

}

/* End of file. */
