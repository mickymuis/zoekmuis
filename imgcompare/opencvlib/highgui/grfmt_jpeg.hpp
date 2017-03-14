#ifndef _GRFMT_JPEG_H_
#define _GRFMT_JPEG_H_

#include "grfmt_base.hpp"
#include "bitstrm.hpp"

#ifdef HAVE_JPEG

// IJG-based Jpeg codec

namespace cv
{

class JpegDecoder : public BaseImageDecoder
{
public:

    JpegDecoder();
    virtual ~JpegDecoder();

    bool  readData( Mat& img );
    bool  readHeader();
    void  close();

    ImageDecoder newDecoder() const;

protected:

    FILE* m_f;
    void* m_state;
};


class JpegEncoder : public BaseImageEncoder
{
public:
    JpegEncoder();
    virtual ~JpegEncoder();

    bool  write( const Mat& img, const vector<int>& params );
    ImageEncoder newEncoder() const;
};

}

#endif

#endif/*_GRFMT_JPEG_H_*/
