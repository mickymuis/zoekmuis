#ifndef _GRFMT_BMP_H_
#define _GRFMT_BMP_H_

#include "grfmt_base.hpp"


namespace cv
{

enum BmpCompression
{
    BMP_RGB = 0,
    BMP_RLE8 = 1,
    BMP_RLE4 = 2,
    BMP_BITFIELDS = 3
};


// Windows Bitmap reader
class BmpDecoder : public BaseImageDecoder
{
public:
    
    BmpDecoder();
    ~BmpDecoder();
    
    bool  readData( Mat& img );
    bool  readHeader();
    void  close();

    ImageDecoder newDecoder() const;

protected:
    
    RLByteStream    m_strm;
    PaletteEntry    m_palette[256];
    int             m_origin;
    int             m_bpp;
    int             m_offset;
    BmpCompression  m_rle_code;
};


// ... writer
class BmpEncoder : public BaseImageEncoder
{
public:
    BmpEncoder();
    ~BmpEncoder();
     
    bool  write( const Mat& img, const vector<int>& params );

    ImageEncoder newEncoder() const;
};

}

#endif/*_GRFMT_BMP_H_*/
