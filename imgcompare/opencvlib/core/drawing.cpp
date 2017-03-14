#include "precomp.hpp"

namespace cv
{

enum { XY_SHIFT = 16, XY_ONE = 1 << XY_SHIFT, DRAWING_STORAGE_BLOCK = (1<<12) - 256 };

struct PolyEdge
{
    PolyEdge() : y0(0), y1(0), x(0), dx(0), next(0) {}
    //PolyEdge(int _y0, int _y1, int _x, int _dx) : y0(_y0), y1(_y1), x(_x), dx(_dx) {}

    int y0, y1;
    int x, dx;
    PolyEdge *next;
};

static void
CollectPolyEdges( Mat& img, const Point* v, int npts,
                  vector<PolyEdge>& edges, const void* color, int line_type,
                  int shift, Point offset=Point() );

static void
FillEdgeCollection( Mat& img, vector<PolyEdge>& edges, const void* color );

static void
PolyLine( Mat& img, const Point* v, int npts, bool closed,
          const void* color, int thickness, int line_type, int shift );

static void
FillConvexPoly( Mat& img, const Point* v, int npts,
                const void* color, int line_type, int shift );

/****************************************************************************************\
*                                   Lines                                                *
\****************************************************************************************/

bool clipLine( Size img_size, Point& pt1, Point& pt2 )
{
    int x1, y1, x2, y2;
    int c1, c2;
    int right = img_size.width-1, bottom = img_size.height-1;

    if( img_size.width <= 0 || img_size.height <= 0 )
        return false;

    x1 = pt1.x; y1 = pt1.y; x2 = pt2.x; y2 = pt2.y;
    c1 = (x1 < 0) + (x1 > right) * 2 + (y1 < 0) * 4 + (y1 > bottom) * 8;
    c2 = (x2 < 0) + (x2 > right) * 2 + (y2 < 0) * 4 + (y2 > bottom) * 8;

    if( (c1 & c2) == 0 && (c1 | c2) != 0 )
    {
        int a;
        if( c1 & 12 )
        {
            a = c1 < 8 ? 0 : bottom;
            x1 += (int) (((int64) (a - y1)) * (x2 - x1) / (y2 - y1));
            y1 = a;
            c1 = (x1 < 0) + (x1 > right) * 2;
        }
        if( c2 & 12 )
        {
            a = c2 < 8 ? 0 : bottom;
            x2 += (int) (((int64) (a - y2)) * (x2 - x1) / (y2 - y1));
            y2 = a;
            c2 = (x2 < 0) + (x2 > right) * 2;
        }
        if( (c1 & c2) == 0 && (c1 | c2) != 0 )
        {
            if( c1 )
            {
                a = c1 == 1 ? 0 : right;
                y1 += (int) (((int64) (a - x1)) * (y2 - y1) / (x2 - x1));
                x1 = a;
                c1 = 0;
            }
            if( c2 )
            {
                a = c2 == 1 ? 0 : right;
                y2 += (int) (((int64) (a - x2)) * (y2 - y1) / (x2 - x1));
                x2 = a;
                c2 = 0;
            }
        }

        assert( (c1 & c2) != 0 || (x1 | y1 | x2 | y2) >= 0 );

        pt1.x = x1;
        pt1.y = y1;
        pt2.x = x2;
        pt2.y = y2;
    }

    return (c1 | c2) == 0;
}

bool clipLine( Rect img_rect, Point& pt1, Point& pt2 )
{
    Point tl = img_rect.tl();
    pt1 -= tl; pt2 -= tl;
    bool inside = clipLine(img_rect.size(), pt1, pt2);
    pt1 += tl; pt2 += tl;
    
    return inside;
}

/* 
   Initializes line iterator.
   Returns number of points on the line or negative number if error.
*/
LineIterator::LineIterator(const Mat& img, Point pt1, Point pt2,
                           int connectivity, bool left_to_right)
{
    count = -1;

    CV_Assert( connectivity == 8 || connectivity == 4 );

    if( (unsigned)pt1.x >= (unsigned)(img.cols) ||
        (unsigned)pt2.x >= (unsigned)(img.cols) ||
        (unsigned)pt1.y >= (unsigned)(img.rows) ||
        (unsigned)pt2.y >= (unsigned)(img.rows) )
    {
        if( !clipLine( img.size(), pt1, pt2 ) )
        {
            ptr = img.data;
            err = plusDelta = minusDelta = plusStep = minusStep = count = 0;
            return;
        }
    }

    int bt_pix0 = (int)img.elemSize(), bt_pix = bt_pix0;
    size_t step = img.step;

    int dx = pt2.x - pt1.x;
    int dy = pt2.y - pt1.y;
    int s = dx < 0 ? -1 : 0;

    if( left_to_right )
    {
        dx = (dx ^ s) - s;
        dy = (dy ^ s) - s;
        pt1.x ^= (pt1.x ^ pt2.x) & s;
        pt1.y ^= (pt1.y ^ pt2.y) & s;
    }
    else
    {
        dx = (dx ^ s) - s;
        bt_pix = (bt_pix ^ s) - s;
    }

    ptr = (uchar*)(img.data + pt1.y * step + pt1.x * bt_pix0);

    s = dy < 0 ? -1 : 0;
    dy = (dy ^ s) - s;
    step = (step ^ s) - s;

    s = dy > dx ? -1 : 0;
    
    /* conditional swaps */
    dx ^= dy & s;
    dy ^= dx & s;
    dx ^= dy & s;

    bt_pix ^= step & s;
    step ^= bt_pix & s;
    bt_pix ^= step & s;

    if( connectivity == 8 )
    {
        assert( dx >= 0 && dy >= 0 );
        
        err = dx - (dy + dy);
        plusDelta = dx + dx;
        minusDelta = -(dy + dy);
        plusStep = (int)step;
        minusStep = bt_pix;
        count = dx + 1;
    }
    else /* connectivity == 4 */
    {
        assert( dx >= 0 && dy >= 0 );
        
        err = 0;
        plusDelta = (dx + dx) + (dy + dy);
        minusDelta = -(dy + dy);
        plusStep = (int)step - bt_pix;
        minusStep = bt_pix;
        count = dx + dy + 1;
    }
    
    this->ptr0 = img.data;
    this->step = (int)img.step;
    this->elemSize = bt_pix0;
}

static void
Line( Mat& img, Point pt1, Point pt2,
      const void* _color, int connectivity = 8 )
{
    if( connectivity == 0 )
        connectivity = 8;
    if( connectivity == 1 )
        connectivity = 4;

    LineIterator iterator(img, pt1, pt2, connectivity, true);
    int i, count = iterator.count;
    int pix_size = (int)img.elemSize();
    const uchar* color = (const uchar*)_color;

    for( i = 0; i < count; i++, ++iterator )
    {
        uchar* ptr = *iterator;
        if( pix_size == 1 )
            ptr[0] = color[0];
        else if( pix_size == 3 )
        {
            ptr[0] = color[0];
            ptr[1] = color[1];
            ptr[2] = color[2];
        }
        else
            memcpy( *iterator, color, pix_size );
    }
}


/* Correction table depent on the slope */
static const uchar SlopeCorrTable[] = {
    181, 181, 181, 182, 182, 183, 184, 185, 187, 188, 190, 192, 194, 196, 198, 201,
    203, 206, 209, 211, 214, 218, 221, 224, 227, 231, 235, 238, 242, 246, 250, 254
};

/* Gaussian for antialiasing filter */
static const int FilterTable[] = {
    168, 177, 185, 194, 202, 210, 218, 224, 231, 236, 241, 246, 249, 252, 254, 254,
    254, 254, 252, 249, 246, 241, 236, 231, 224, 218, 210, 202, 194, 185, 177, 168,
    158, 149, 140, 131, 122, 114, 105, 97, 89, 82, 75, 68, 62, 56, 50, 45,
    40, 36, 32, 28, 25, 22, 19, 16, 14, 12, 11, 9, 8, 7, 5, 5
};

static void
LineAA( Mat& img, Point pt1, Point pt2, const void* color )
{
    int dx, dy;
    int ecount, scount = 0;
    int slope;
    int ax, ay;
    int x_step, y_step;
    int i, j;
    int ep_table[9];
    int cb = ((uchar*)color)[0], cg = ((uchar*)color)[1], cr = ((uchar*)color)[2];
    int _cb, _cg, _cr;
    int nch = img.channels();
    uchar* ptr = img.data;
    size_t step = img.step;
    Size size = img.size();

    if( !((nch == 1 || nch == 3) && img.depth() == CV_8U) )
    {
        Line(img, pt1, pt2, color);
        return;
    }

    pt1.x -= XY_ONE*2;
    pt1.y -= XY_ONE*2;
    pt2.x -= XY_ONE*2;
    pt2.y -= XY_ONE*2;
    ptr += img.step*2 + 2*nch;

    size.width = ((size.width - 5) << XY_SHIFT) + 1;
    size.height = ((size.height - 5) << XY_SHIFT) + 1;

    if( !clipLine( size, pt1, pt2 ))
        return;

    dx = pt2.x - pt1.x;
    dy = pt2.y - pt1.y;

    j = dx < 0 ? -1 : 0;
    ax = (dx ^ j) - j;
    i = dy < 0 ? -1 : 0;
    ay = (dy ^ i) - i;

    if( ax > ay )
    {
        dx = ax;
        dy = (dy ^ j) - j;
        pt1.x ^= pt2.x & j;
        pt2.x ^= pt1.x & j;
        pt1.x ^= pt2.x & j;
        pt1.y ^= pt2.y & j;
        pt2.y ^= pt1.y & j;
        pt1.y ^= pt2.y & j;

        x_step = XY_ONE;
        y_step = (int) (((int64) dy << XY_SHIFT) / (ax | 1));
        pt2.x += XY_ONE;
        ecount = (pt2.x >> XY_SHIFT) - (pt1.x >> XY_SHIFT);
        j = -(pt1.x & (XY_ONE - 1));
        pt1.y += (int) ((((int64) y_step) * j) >> XY_SHIFT) + (XY_ONE >> 1);
        slope = (y_step >> (XY_SHIFT - 5)) & 0x3f;
        slope ^= (y_step < 0 ? 0x3f : 0);

        /* Get 4-bit fractions for end-point adjustments */
        i = (pt1.x >> (XY_SHIFT - 7)) & 0x78;
        j = (pt2.x >> (XY_SHIFT - 7)) & 0x78;
    }
    else
    {
        dy = ay;
        dx = (dx ^ i) - i;
        pt1.x ^= pt2.x & i;
        pt2.x ^= pt1.x & i;
        pt1.x ^= pt2.x & i;
        pt1.y ^= pt2.y & i;
        pt2.y ^= pt1.y & i;
        pt1.y ^= pt2.y & i;

        x_step = (int) (((int64) dx << XY_SHIFT) / (ay | 1));
        y_step = XY_ONE;
        pt2.y += XY_ONE;
        ecount = (pt2.y >> XY_SHIFT) - (pt1.y >> XY_SHIFT);
        j = -(pt1.y & (XY_ONE - 1));
        pt1.x += (int) ((((int64) x_step) * j) >> XY_SHIFT) + (XY_ONE >> 1);
        slope = (x_step >> (XY_SHIFT - 5)) & 0x3f;
        slope ^= (x_step < 0 ? 0x3f : 0);

        /* Get 4-bit fractions for end-point adjustments */
        i = (pt1.y >> (XY_SHIFT - 7)) & 0x78;
        j = (pt2.y >> (XY_SHIFT - 7)) & 0x78;
    }

    slope = (slope & 0x20) ? 0x100 : SlopeCorrTable[slope];

    /* Calc end point correction table */
    {
        int t0 = slope << 7;
        int t1 = ((0x78 - i) | 4) * slope;
        int t2 = (j | 4) * slope;

        ep_table[0] = 0;
        ep_table[8] = slope;
        ep_table[1] = ep_table[3] = ((((j - i) & 0x78) | 4) * slope >> 8) & 0x1ff;
        ep_table[2] = (t1 >> 8) & 0x1ff;
        ep_table[4] = ((((j - i) + 0x80) | 4) * slope >> 8) & 0x1ff;
        ep_table[5] = ((t1 + t0) >> 8) & 0x1ff;
        ep_table[6] = (t2 >> 8) & 0x1ff;
        ep_table[7] = ((t2 + t0) >> 8) & 0x1ff;
    }

    if( nch == 3 )
    {
        #define  ICV_PUT_POINT()            \
        {                                   \
            _cb = tptr[0];                  \
            _cb += ((cb - _cb)*a + 127)>> 8;\
            _cg = tptr[1];                  \
            _cg += ((cg - _cg)*a + 127)>> 8;\
            _cr = tptr[2];                  \
            _cr += ((cr - _cr)*a + 127)>> 8;\
            tptr[0] = (uchar)_cb;           \
            tptr[1] = (uchar)_cg;           \
            tptr[2] = (uchar)_cr;           \
        }
        if( ax > ay )
        {
            ptr += (pt1.x >> XY_SHIFT) * 3;

            while( ecount >= 0 )
            {
                uchar *tptr = ptr + ((pt1.y >> XY_SHIFT) - 1) * step;

                int ep_corr = ep_table[(((scount >= 2) + 1) & (scount | 2)) * 3 +
                                       (((ecount >= 2) + 1) & (ecount | 2))];
                int a, dist = (pt1.y >> (XY_SHIFT - 5)) & 31;

                a = (ep_corr * FilterTable[dist + 32] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                tptr += step;
                a = (ep_corr * FilterTable[dist] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                tptr += step;
                a = (ep_corr * FilterTable[63 - dist] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                pt1.y += y_step;
                ptr += 3;
                scount++;
                ecount--;
            }
        }
        else
        {
            ptr += (pt1.y >> XY_SHIFT) * step;

            while( ecount >= 0 )
            {
                uchar *tptr = ptr + ((pt1.x >> XY_SHIFT) - 1) * 3;

                int ep_corr = ep_table[(((scount >= 2) + 1) & (scount | 2)) * 3 +
                                       (((ecount >= 2) + 1) & (ecount | 2))];
                int a, dist = (pt1.x >> (XY_SHIFT - 5)) & 31;

                a = (ep_corr * FilterTable[dist + 32] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                tptr += 3;
                a = (ep_corr * FilterTable[dist] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                tptr += 3;
                a = (ep_corr * FilterTable[63 - dist] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                pt1.x += x_step;
                ptr += step;
                scount++;
                ecount--;
            }
        }
        #undef ICV_PUT_POINT
    }
    else
    {
        #define  ICV_PUT_POINT()            \
        {                                   \
            _cb = tptr[0];                  \
            _cb += ((cb - _cb)*a + 127)>> 8;\
            tptr[0] = (uchar)_cb;           \
        }

        if( ax > ay )
        {
            ptr += (pt1.x >> XY_SHIFT);

            while( ecount >= 0 )
            {
                uchar *tptr = ptr + ((pt1.y >> XY_SHIFT) - 1) * step;

                int ep_corr = ep_table[(((scount >= 2) + 1) & (scount | 2)) * 3 +
                                       (((ecount >= 2) + 1) & (ecount | 2))];
                int a, dist = (pt1.y >> (XY_SHIFT - 5)) & 31;

                a = (ep_corr * FilterTable[dist + 32] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                tptr += step;
                a = (ep_corr * FilterTable[dist] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                tptr += step;
                a = (ep_corr * FilterTable[63 - dist] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                pt1.y += y_step;
                ptr++;
                scount++;
                ecount--;
            }
        }
        else
        {
            ptr += (pt1.y >> XY_SHIFT) * step;

            while( ecount >= 0 )
            {
                uchar *tptr = ptr + ((pt1.x >> XY_SHIFT) - 1);

                int ep_corr = ep_table[(((scount >= 2) + 1) & (scount | 2)) * 3 +
                                       (((ecount >= 2) + 1) & (ecount | 2))];
                int a, dist = (pt1.x >> (XY_SHIFT - 5)) & 31;

                a = (ep_corr * FilterTable[dist + 32] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                tptr++;
                a = (ep_corr * FilterTable[dist] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                tptr++;
                a = (ep_corr * FilterTable[63 - dist] >> 8) & 0xff;
                ICV_PUT_POINT();
                ICV_PUT_POINT();

                pt1.x += x_step;
                ptr += step;
                scount++;
                ecount--;
            }
        }
        #undef ICV_PUT_POINT
    }
}


static void
Line2( Mat& img, Point pt1, Point pt2, const void* color )
{
    int dx, dy;
    int ecount;
    int ax, ay;
    int i, j;
    int x_step, y_step;
    int cb = ((uchar*)color)[0];
    int cg = ((uchar*)color)[1];
    int cr = ((uchar*)color)[2];
    int pix_size = (int)img.elemSize();
    uchar *ptr = img.data, *tptr;
    size_t step = img.step;
    Size size = img.size();

    //assert( img && (nch == 1 || nch == 3) && img.depth() == CV_8U );

    pt1.x -= XY_ONE*2;
    pt1.y -= XY_ONE*2;
    pt2.x -= XY_ONE*2;
    pt2.y -= XY_ONE*2;
    ptr += img.step*2 + 2*pix_size;

    size.width = ((size.width - 5) << XY_SHIFT) + 1;
    size.height = ((size.height - 5) << XY_SHIFT) + 1;

    if( !clipLine( size, pt1, pt2 ))
        return;

    dx = pt2.x - pt1.x;
    dy = pt2.y - pt1.y;

    j = dx < 0 ? -1 : 0;
    ax = (dx ^ j) - j;
    i = dy < 0 ? -1 : 0;
    ay = (dy ^ i) - i;

    if( ax > ay )
    {
        dx = ax;
        dy = (dy ^ j) - j;
        pt1.x ^= pt2.x & j;
        pt2.x ^= pt1.x & j;
        pt1.x ^= pt2.x & j;
        pt1.y ^= pt2.y & j;
        pt2.y ^= pt1.y & j;
        pt1.y ^= pt2.y & j;

        x_step = XY_ONE;
        y_step = (int) (((int64) dy << XY_SHIFT) / (ax | 1));
        ecount = (pt2.x - pt1.x) >> XY_SHIFT;
    }
    else
    {
        dy = ay;
        dx = (dx ^ i) - i;
        pt1.x ^= pt2.x & i;
        pt2.x ^= pt1.x & i;
        pt1.x ^= pt2.x & i;
        pt1.y ^= pt2.y & i;
        pt2.y ^= pt1.y & i;
        pt1.y ^= pt2.y & i;

        x_step = (int) (((int64) dx << XY_SHIFT) / (ay | 1));
        y_step = XY_ONE;
        ecount = (pt2.y - pt1.y) >> XY_SHIFT;
    }

    pt1.x += (XY_ONE >> 1);
    pt1.y += (XY_ONE >> 1);

    if( pix_size == 3 )
    {
        #define  ICV_PUT_POINT()    \
        {                           \
            tptr[0] = (uchar)cb;    \
            tptr[1] = (uchar)cg;    \
            tptr[2] = (uchar)cr;    \
        }
        
        tptr = ptr + ((pt2.x + (XY_ONE >> 1))>> XY_SHIFT)*3 +
            ((pt2.y + (XY_ONE >> 1)) >> XY_SHIFT)*step;
        ICV_PUT_POINT();
        
        if( ax > ay )
        {
            ptr += (pt1.x >> XY_SHIFT) * 3;

            while( ecount >= 0 )
            {
                tptr = ptr + (pt1.y >> XY_SHIFT) * step;
                ICV_PUT_POINT();
                pt1.y += y_step;
                ptr += 3;
                ecount--;
            }
        }
        else
        {
            ptr += (pt1.y >> XY_SHIFT) * step;

            while( ecount >= 0 )
            {
                tptr = ptr + (pt1.x >> XY_SHIFT) * 3;
                ICV_PUT_POINT();
                pt1.x += x_step;
                ptr += step;
                ecount--;
            }
        }

        #undef ICV_PUT_POINT
    }
    else if( pix_size == 1 )
    {
        #define  ICV_PUT_POINT()            \
        {                                   \
            tptr[0] = (uchar)cb;            \
        }

        tptr = ptr + ((pt2.x + (XY_ONE >> 1))>> XY_SHIFT) +
            ((pt2.y + (XY_ONE >> 1)) >> XY_SHIFT)*step;
        ICV_PUT_POINT();

        if( ax > ay )
        {
            ptr += (pt1.x >> XY_SHIFT);

            while( ecount >= 0 )
            {
                tptr = ptr + (pt1.y >> XY_SHIFT) * step;
                ICV_PUT_POINT();
                pt1.y += y_step;
                ptr++;
                ecount--;
            }
        }
        else
        {
            ptr += (pt1.y >> XY_SHIFT) * step;

            while( ecount >= 0 )
            {
                tptr = ptr + (pt1.x >> XY_SHIFT);
                ICV_PUT_POINT();
                pt1.x += x_step;
                ptr += step;
                ecount--;
            }
        }
        #undef ICV_PUT_POINT
    }
    else
    {
        #define  ICV_PUT_POINT()                \
            for( j = 0; j < pix_size; j++ )     \
                tptr[j] = ((uchar*)color)[j];

        tptr = ptr + ((pt2.x + (XY_ONE >> 1))>> XY_SHIFT)*pix_size +
            ((pt2.y + (XY_ONE >> 1)) >> XY_SHIFT)*step;
        ICV_PUT_POINT();
        
        if( ax > ay )
        {
            ptr += (pt1.x >> XY_SHIFT) * pix_size;

            while( ecount >= 0 )
            {
                tptr = ptr + (pt1.y >> XY_SHIFT) * step;
                ICV_PUT_POINT();
                pt1.y += y_step;
                ptr += pix_size;
                ecount--;
            }
        }
        else
        {
            ptr += (pt1.y >> XY_SHIFT) * step;

            while( ecount >= 0 )
            {
                tptr = ptr + (pt1.x >> XY_SHIFT) * pix_size;
                ICV_PUT_POINT();
                pt1.x += x_step;
                ptr += step;
                ecount--;
            }
        }

        #undef ICV_PUT_POINT
    }
}


/****************************************************************************************\
*                   Antialiazed Elliptic Arcs via Antialiazed Lines                      *
\****************************************************************************************/

static const float SinTable[] =
    { 0.0000000f, 0.0174524f, 0.0348995f, 0.0523360f, 0.0697565f, 0.0871557f,
    0.1045285f, 0.1218693f, 0.1391731f, 0.1564345f, 0.1736482f, 0.1908090f,
    0.2079117f, 0.2249511f, 0.2419219f, 0.2588190f, 0.2756374f, 0.2923717f,
    0.3090170f, 0.3255682f, 0.3420201f, 0.3583679f, 0.3746066f, 0.3907311f,
    0.4067366f, 0.4226183f, 0.4383711f, 0.4539905f, 0.4694716f, 0.4848096f,
    0.5000000f, 0.5150381f, 0.5299193f, 0.5446390f, 0.5591929f, 0.5735764f,
    0.5877853f, 0.6018150f, 0.6156615f, 0.6293204f, 0.6427876f, 0.6560590f,
    0.6691306f, 0.6819984f, 0.6946584f, 0.7071068f, 0.7193398f, 0.7313537f,
    0.7431448f, 0.7547096f, 0.7660444f, 0.7771460f, 0.7880108f, 0.7986355f,
    0.8090170f, 0.8191520f, 0.8290376f, 0.8386706f, 0.8480481f, 0.8571673f,
    0.8660254f, 0.8746197f, 0.8829476f, 0.8910065f, 0.8987940f, 0.9063078f,
    0.9135455f, 0.9205049f, 0.9271839f, 0.9335804f, 0.9396926f, 0.9455186f,
    0.9510565f, 0.9563048f, 0.9612617f, 0.9659258f, 0.9702957f, 0.9743701f,
    0.9781476f, 0.9816272f, 0.9848078f, 0.9876883f, 0.9902681f, 0.9925462f,
    0.9945219f, 0.9961947f, 0.9975641f, 0.9986295f, 0.9993908f, 0.9998477f,
    1.0000000f, 0.9998477f, 0.9993908f, 0.9986295f, 0.9975641f, 0.9961947f,
    0.9945219f, 0.9925462f, 0.9902681f, 0.9876883f, 0.9848078f, 0.9816272f,
    0.9781476f, 0.9743701f, 0.9702957f, 0.9659258f, 0.9612617f, 0.9563048f,
    0.9510565f, 0.9455186f, 0.9396926f, 0.9335804f, 0.9271839f, 0.9205049f,
    0.9135455f, 0.9063078f, 0.8987940f, 0.8910065f, 0.8829476f, 0.8746197f,
    0.8660254f, 0.8571673f, 0.8480481f, 0.8386706f, 0.8290376f, 0.8191520f,
    0.8090170f, 0.7986355f, 0.7880108f, 0.7771460f, 0.7660444f, 0.7547096f,
    0.7431448f, 0.7313537f, 0.7193398f, 0.7071068f, 0.6946584f, 0.6819984f,
    0.6691306f, 0.6560590f, 0.6427876f, 0.6293204f, 0.6156615f, 0.6018150f,
    0.5877853f, 0.5735764f, 0.5591929f, 0.5446390f, 0.5299193f, 0.5150381f,
    0.5000000f, 0.4848096f, 0.4694716f, 0.4539905f, 0.4383711f, 0.4226183f,
    0.4067366f, 0.3907311f, 0.3746066f, 0.3583679f, 0.3420201f, 0.3255682f,
    0.3090170f, 0.2923717f, 0.2756374f, 0.2588190f, 0.2419219f, 0.2249511f,
    0.2079117f, 0.1908090f, 0.1736482f, 0.1564345f, 0.1391731f, 0.1218693f,
    0.1045285f, 0.0871557f, 0.0697565f, 0.0523360f, 0.0348995f, 0.0174524f,
    0.0000000f, -0.0174524f, -0.0348995f, -0.0523360f, -0.0697565f, -0.0871557f,
    -0.1045285f, -0.1218693f, -0.1391731f, -0.1564345f, -0.1736482f, -0.1908090f,
    -0.2079117f, -0.2249511f, -0.2419219f, -0.2588190f, -0.2756374f, -0.2923717f,
    -0.3090170f, -0.3255682f, -0.3420201f, -0.3583679f, -0.3746066f, -0.3907311f,
    -0.4067366f, -0.4226183f, -0.4383711f, -0.4539905f, -0.4694716f, -0.4848096f,
    -0.5000000f, -0.5150381f, -0.5299193f, -0.5446390f, -0.5591929f, -0.5735764f,
    -0.5877853f, -0.6018150f, -0.6156615f, -0.6293204f, -0.6427876f, -0.6560590f,
    -0.6691306f, -0.6819984f, -0.6946584f, -0.7071068f, -0.7193398f, -0.7313537f,
    -0.7431448f, -0.7547096f, -0.7660444f, -0.7771460f, -0.7880108f, -0.7986355f,
    -0.8090170f, -0.8191520f, -0.8290376f, -0.8386706f, -0.8480481f, -0.8571673f,
    -0.8660254f, -0.8746197f, -0.8829476f, -0.8910065f, -0.8987940f, -0.9063078f,
    -0.9135455f, -0.9205049f, -0.9271839f, -0.9335804f, -0.9396926f, -0.9455186f,
    -0.9510565f, -0.9563048f, -0.9612617f, -0.9659258f, -0.9702957f, -0.9743701f,
    -0.9781476f, -0.9816272f, -0.9848078f, -0.9876883f, -0.9902681f, -0.9925462f,
    -0.9945219f, -0.9961947f, -0.9975641f, -0.9986295f, -0.9993908f, -0.9998477f,
    -1.0000000f, -0.9998477f, -0.9993908f, -0.9986295f, -0.9975641f, -0.9961947f,
    -0.9945219f, -0.9925462f, -0.9902681f, -0.9876883f, -0.9848078f, -0.9816272f,
    -0.9781476f, -0.9743701f, -0.9702957f, -0.9659258f, -0.9612617f, -0.9563048f,
    -0.9510565f, -0.9455186f, -0.9396926f, -0.9335804f, -0.9271839f, -0.9205049f,
    -0.9135455f, -0.9063078f, -0.8987940f, -0.8910065f, -0.8829476f, -0.8746197f,
    -0.8660254f, -0.8571673f, -0.8480481f, -0.8386706f, -0.8290376f, -0.8191520f,
    -0.8090170f, -0.7986355f, -0.7880108f, -0.7771460f, -0.7660444f, -0.7547096f,
    -0.7431448f, -0.7313537f, -0.7193398f, -0.7071068f, -0.6946584f, -0.6819984f,
    -0.6691306f, -0.6560590f, -0.6427876f, -0.6293204f, -0.6156615f, -0.6018150f,
    -0.5877853f, -0.5735764f, -0.5591929f, -0.5446390f, -0.5299193f, -0.5150381f,
    -0.5000000f, -0.4848096f, -0.4694716f, -0.4539905f, -0.4383711f, -0.4226183f,
    -0.4067366f, -0.3907311f, -0.3746066f, -0.3583679f, -0.3420201f, -0.3255682f,
    -0.3090170f, -0.2923717f, -0.2756374f, -0.2588190f, -0.2419219f, -0.2249511f,
    -0.2079117f, -0.1908090f, -0.1736482f, -0.1564345f, -0.1391731f, -0.1218693f,
    -0.1045285f, -0.0871557f, -0.0697565f, -0.0523360f, -0.0348995f, -0.0174524f,
    -0.0000000f, 0.0174524f, 0.0348995f, 0.0523360f, 0.0697565f, 0.0871557f,
    0.1045285f, 0.1218693f, 0.1391731f, 0.1564345f, 0.1736482f, 0.1908090f,
    0.2079117f, 0.2249511f, 0.2419219f, 0.2588190f, 0.2756374f, 0.2923717f,
    0.3090170f, 0.3255682f, 0.3420201f, 0.3583679f, 0.3746066f, 0.3907311f,
    0.4067366f, 0.4226183f, 0.4383711f, 0.4539905f, 0.4694716f, 0.4848096f,
    0.5000000f, 0.5150381f, 0.5299193f, 0.5446390f, 0.5591929f, 0.5735764f,
    0.5877853f, 0.6018150f, 0.6156615f, 0.6293204f, 0.6427876f, 0.6560590f,
    0.6691306f, 0.6819984f, 0.6946584f, 0.7071068f, 0.7193398f, 0.7313537f,
    0.7431448f, 0.7547096f, 0.7660444f, 0.7771460f, 0.7880108f, 0.7986355f,
    0.8090170f, 0.8191520f, 0.8290376f, 0.8386706f, 0.8480481f, 0.8571673f,
    0.8660254f, 0.8746197f, 0.8829476f, 0.8910065f, 0.8987940f, 0.9063078f,
    0.9135455f, 0.9205049f, 0.9271839f, 0.9335804f, 0.9396926f, 0.9455186f,
    0.9510565f, 0.9563048f, 0.9612617f, 0.9659258f, 0.9702957f, 0.9743701f,
    0.9781476f, 0.9816272f, 0.9848078f, 0.9876883f, 0.9902681f, 0.9925462f,
    0.9945219f, 0.9961947f, 0.9975641f, 0.9986295f, 0.9993908f, 0.9998477f,
    1.0000000f
};


static void
sincos( int angle, float& cosval, float& sinval )
{
    angle += (angle < 0 ? 360 : 0);
    sinval = SinTable[angle];
    cosval = SinTable[450 - angle];
}

/* 
   constructs polygon that represents elliptic arc.
*/
void ellipse2Poly( Point center, Size axes, int angle,
                   int arc_start, int arc_end,
                   int delta, vector<Point>& pts )
{
    float alpha, beta;
    double size_a = axes.width, size_b = axes.height;
    double cx = center.x, cy = center.y;
    Point prevPt(INT_MIN,INT_MIN);
    int i;

    while( angle < 0 )
        angle += 360;
    while( angle > 360 )
        angle -= 360;

    if( arc_start > arc_end )
    {
        i = arc_start;
        arc_start = arc_end;
        arc_end = i;
    }
    while( arc_start < 0 )
    {
        arc_start += 360;
        arc_end += 360;
    }
    while( arc_end > 360 )
    {
        arc_end -= 360;
        arc_start -= 360;
    }
    if( arc_end - arc_start > 360 )
    {
        arc_start = 0;
        arc_end = 360;
    }
    sincos( angle, alpha, beta );
    pts.resize(0);

    for( i = arc_start; i < arc_end + delta; i += delta )
    {
        double x, y;
        angle = i;
        if( angle > arc_end )
            angle = arc_end;
        if( angle < 0 )
            angle += 360;
        
        x = size_a * SinTable[450-angle];
        y = size_b * SinTable[angle];
        Point pt;
        pt.x = cvRound( cx + x * alpha - y * beta );
        pt.y = cvRound( cy + x * beta + y * alpha );
        if( pt != prevPt )
            pts.push_back(pt);
    }

    if( pts.size() < 2 )
        pts.push_back(pts[0]);
}


static void
EllipseEx( Mat& img, Point center, Size axes,
           int angle, int arc_start, int arc_end,
           const void* color, int thickness, int line_type )
{
    axes.width = std::abs(axes.width), axes.height = std::abs(axes.height);
    int delta = (std::max(axes.width,axes.height)+(XY_ONE>>1))>>XY_SHIFT;
    delta = delta < 3 ? 90 : delta < 10 ? 30 : delta < 15 ? 18 : 5;

    vector<Point> v;
    ellipse2Poly( center, axes, angle, arc_start, arc_end, delta, v );

    if( thickness >= 0 )
        PolyLine( img, &v[0], (int)v.size(), false, color, thickness, line_type, XY_SHIFT );
    else if( arc_end - arc_start >= 360 )
        FillConvexPoly( img, &v[0], (int)v.size(), color, line_type, XY_SHIFT );
    else
    {
        v.push_back(center);
        vector<PolyEdge> edges;
        CollectPolyEdges( img,  &v[0], (int)v.size(), edges, color, line_type, XY_SHIFT );
        FillEdgeCollection( img, edges, color );
    }
}


/****************************************************************************************\
*                                Polygons filling                                        * 
\****************************************************************************************/

/* helper macros: filling horizontal row */
#define ICV_HLINE( ptr, xl, xr, color, pix_size )            \
{                                                            \
    uchar* hline_ptr = (uchar*)(ptr) + (xl)*(pix_size);      \
    uchar* hline_max_ptr = (uchar*)(ptr) + (xr)*(pix_size);  \
                                                             \
    for( ; hline_ptr <= hline_max_ptr; hline_ptr += (pix_size))\
    {                                                        \
        int hline_j;                                         \
        for( hline_j = 0; hline_j < (pix_size); hline_j++ )  \
        {                                                    \
            hline_ptr[hline_j] = ((uchar*)color)[hline_j];   \
        }                                                    \
    }                                                        \
}


/* filling convex polygon. v - array of vertices, ntps - number of points */
static void
FillConvexPoly( Mat& img, const Point* v, int npts, const void* color, int line_type, int shift )
{
    struct
    {
        int idx, di;
        int x, dx, ye;
    }
    edge[2];

    int delta = shift ? 1 << (shift - 1) : 0;
    int i, y, imin = 0, left = 0, right = 1, x1, x2;
    int edges = npts;
    int xmin, xmax, ymin, ymax;
    uchar* ptr = img.data;
    Size size = img.size();
    int pix_size = (int)img.elemSize();
    Point p0;
    int delta1, delta2;

    if( line_type < CV_AA )
        delta1 = delta2 = XY_ONE >> 1;
    else
        delta1 = XY_ONE - 1, delta2 = 0;

    p0 = v[npts - 1];
    p0.x <<= XY_SHIFT - shift;
    p0.y <<= XY_SHIFT - shift;

    assert( 0 <= shift && shift <= XY_SHIFT );
    xmin = xmax = v[0].x;
    ymin = ymax = v[0].y;

    for( i = 0; i < npts; i++ )
    {
        Point p = v[i];
        if( p.y < ymin )
        {
            ymin = p.y;
            imin = i;
        }

        ymax = std::max( ymax, p.y );
        xmax = std::max( xmax, p.x );
        xmin = MIN( xmin, p.x );

        p.x <<= XY_SHIFT - shift;
        p.y <<= XY_SHIFT - shift;

        if( line_type <= 8 )
        {
            if( shift == 0 )
            {
                Point pt0, pt1;
                pt0.x = p0.x >> XY_SHIFT;
                pt0.y = p0.y >> XY_SHIFT;
                pt1.x = p.x >> XY_SHIFT;
                pt1.y = p.y >> XY_SHIFT;
                Line( img, pt0, pt1, color, line_type );
            }
            else
                Line2( img, p0, p, color );
        }
        else
            LineAA( img, p0, p, color );
        p0 = p;
    }

    xmin = (xmin + delta) >> shift;
    xmax = (xmax + delta) >> shift;
    ymin = (ymin + delta) >> shift;
    ymax = (ymax + delta) >> shift;

    if( npts < 3 || xmax < 0 || ymax < 0 || xmin >= size.width || ymin >= size.height )
        return;

    ymax = MIN( ymax, size.height - 1 );
    edge[0].idx = edge[1].idx = imin;

    edge[0].ye = edge[1].ye = y = ymin;
    edge[0].di = 1;
    edge[1].di = npts - 1;

    ptr += img.step*y;

    do
    {
        if( line_type < CV_AA || y < ymax || y == ymin )
        {
            for( i = 0; i < 2; i++ )
            {
                if( y >= edge[i].ye )
                {
                    int idx = edge[i].idx, di = edge[i].di;
                    int xs = 0, xe, ye, ty = 0;

                    for(;;)
                    {
                        ty = (v[idx].y + delta) >> shift;
                        if( ty > y || edges == 0 )
                            break;
                        xs = v[idx].x;
                        idx += di;
                        idx -= ((idx < npts) - 1) & npts;   /* idx -= idx >= npts ? npts : 0 */
                        edges--;
                    }

                    ye = ty;
                    xs <<= XY_SHIFT - shift;
                    xe = v[idx].x << (XY_SHIFT - shift);

                    /* no more edges */
                    if( y >= ye )
                        return;

                    edge[i].ye = ye;
                    edge[i].dx = ((xe - xs)*2 + (ye - y)) / (2 * (ye - y));
                    edge[i].x = xs;
                    edge[i].idx = idx;
                }
            }
        }

        if( edge[left].x > edge[right].x )
        {
            left ^= 1;
            right ^= 1;
        }

        x1 = edge[left].x;
        x2 = edge[right].x;

        if( y >= 0 )
        {
            int xx1 = (x1 + delta1) >> XY_SHIFT;
            int xx2 = (x2 + delta2) >> XY_SHIFT;

            if( xx2 >= 0 && xx1 < size.width )
            {
                if( xx1 < 0 )
                    xx1 = 0;
                if( xx2 >= size.width )
                    xx2 = size.width - 1;
                ICV_HLINE( ptr, xx1, xx2, color, pix_size );
            }
        }

        x1 += edge[left].dx;
        x2 += edge[right].dx;

        edge[left].x = x1;
        edge[right].x = x2;
        ptr += img.step;
    }
    while( ++y <= ymax );
}


/******** Arbitrary polygon **********/

static void
CollectPolyEdges( Mat& img, const Point* v, int count, vector<PolyEdge>& edges,
                  const void* color, int line_type, int shift, Point offset )
{
    int i, delta = offset.y + (shift ? 1 << (shift - 1) : 0);
    Point pt0 = v[count-1], pt1;
    pt0.x = (pt0.x + offset.x) << (XY_SHIFT - shift);
    pt0.y = (pt0.y + delta) >> shift;

    edges.reserve( edges.size() + count );

    for( i = 0; i < count; i++, pt0 = pt1 )
    {
        Point t0, t1;
        PolyEdge edge;
        
        pt1 = v[i];
        pt1.x = (pt1.x + offset.x) << (XY_SHIFT - shift);
        pt1.y = (pt1.y + delta) >> shift;

        if( line_type < CV_AA )
        {
            t0.y = pt0.y; t1.y = pt1.y;
            t0.x = (pt0.x + (XY_ONE >> 1)) >> XY_SHIFT;
            t1.x = (pt1.x + (XY_ONE >> 1)) >> XY_SHIFT;
            Line( img, t0, t1, color, line_type );
        }
        else
        {
            t0.x = pt0.x; t1.x = pt1.x;
            t0.y = pt0.y << XY_SHIFT;
            t1.y = pt1.y << XY_SHIFT;
            LineAA( img, t0, t1, color );
        }

        if( pt0.y == pt1.y )
            continue;

        if( pt0.y < pt1.y )
        {
            edge.y0 = pt0.y;
            edge.y1 = pt1.y;
            edge.x = pt0.x;
        }
        else
        {
            edge.y0 = pt1.y;
            edge.y1 = pt0.y;
            edge.x = pt1.x;
        }
        edge.dx = (pt1.x - pt0.x) / (pt1.y - pt0.y);
        edges.push_back(edge);
    }
}

struct CmpEdges
{
    bool operator ()(const PolyEdge& e1, const PolyEdge& e2)
    {
        return e1.y0 - e2.y0 ? e1.y0 < e2.y0 :
            e1.x - e2.x ? e1.x < e2.x : e1.dx < e2.dx;
    }
};

/**************** helper macros and functions for sequence/contour processing ***********/

static void
FillEdgeCollection( Mat& img, vector<PolyEdge>& edges, const void* color )
{
    PolyEdge tmp;
    int i, y, total = (int)edges.size();
    Size size = img.size();
    PolyEdge* e;
    int y_max = INT_MIN, x_max = INT_MIN, y_min = INT_MAX, x_min = INT_MAX;
    int pix_size = (int)img.elemSize();

    if( total < 2 )
        return;

    for( i = 0; i < total; i++ )
    {
        PolyEdge& e1 = edges[i];
        assert( e1.y0 < e1.y1 );
        y_min = std::min( y_min, e1.y0 );
        y_max = std::max( y_max, e1.y1 );
        x_min = std::min( x_min, e1.x );
        x_max = std::max( x_max, e1.x );
    }

    if( y_max < 0 || y_min >= size.height || x_max < 0 || x_min >= (size.width<<XY_SHIFT) )
        return;

    std::sort( edges.begin(), edges.end(), CmpEdges() );

    // start drawing
    tmp.y0 = INT_MAX;
    edges.push_back(tmp); // after this point we do not add
                          // any elements to edges, thus we can use pointers
    i = 0;
    tmp.next = 0;
    e = &edges[i];
    y_max = MIN( y_max, size.height );

    for( y = e->y0; y < y_max; y++ )
    {
        PolyEdge *last, *prelast, *keep_prelast;
        int sort_flag = 0;
        int draw = 0;
        int clipline = y < 0;

        prelast = &tmp;
        last = tmp.next;
        while( last || e->y0 == y )
        {
            if( last && last->y1 == y )
            {
                // exclude edge if y reachs its lower point
                prelast->next = last->next;
                last = last->next;
                continue;
            }
            keep_prelast = prelast;
            if( last && (e->y0 > y || last->x < e->x) )
            {
                // go to the next edge in active list
                prelast = last;
                last = last->next;
            }
            else if( i < total )
            {
                // insert new edge into active list if y reachs its upper point
                prelast->next = e;
                e->next = last;
                prelast = e;
                e = &edges[++i];
            }
            else
                break;

            if( draw )
            {
                if( !clipline )
                {
                    // convert x's from fixed-point to image coordinates
                    uchar *timg = img.data + y * img.step;
                    int x1 = keep_prelast->x;
                    int x2 = prelast->x;

                    if( x1 > x2 )
                    {
                        int t = x1;

                        x1 = x2;
                        x2 = t;
                    }

                    x1 = (x1 + XY_ONE - 1) >> XY_SHIFT;
                    x2 = x2 >> XY_SHIFT;

                    // clip and draw the line
                    if( x1 < size.width && x2 >= 0 )
                    {
                        if( x1 < 0 )
                            x1 = 0;
                        if( x2 >= size.width )
                            x2 = size.width - 1;
                        ICV_HLINE( timg, x1, x2, color, pix_size );
                    }
                }
                keep_prelast->x += keep_prelast->dx;
                prelast->x += prelast->dx;
            }
            draw ^= 1;
        }

        // sort edges (using bubble sort)
        keep_prelast = 0;

        do
        {
            prelast = &tmp;
            last = tmp.next;

            while( last != keep_prelast && last->next != 0 )
            {
                PolyEdge *te = last->next;

                // swap edges
                if( last->x > te->x )
                {
                    prelast->next = te;
                    last->next = te->next;
                    te->next = last;
                    prelast = te;
                    sort_flag = 1;
                }
                else
                {
                    prelast = last;
                    last = te;
                }
            }
            keep_prelast = prelast;
        }
        while( sort_flag && keep_prelast != tmp.next && keep_prelast != &tmp );
    }
}


/* draws simple or filled circle */
static void
Circle( Mat& img, Point center, int radius, const void* color, int fill )
{
    Size size = img.size();
    size_t step = img.step;
    int pix_size = (int)img.elemSize();
    uchar* ptr = img.data;
    int err = 0, dx = radius, dy = 0, plus = 1, minus = (radius << 1) - 1;
    int inside = center.x >= radius && center.x < size.width - radius &&
        center.y >= radius && center.y < size.height - radius;

    #define ICV_PUT_POINT( ptr, x )     \
        memcpy( ptr + (x)*pix_size, color, pix_size );

    while( dx >= dy )
    {
        int mask;
        int y11 = center.y - dy, y12 = center.y + dy, y21 = center.y - dx, y22 = center.y + dx;
        int x11 = center.x - dx, x12 = center.x + dx, x21 = center.x - dy, x22 = center.x + dy;

        if( inside )
        {
            uchar *tptr0 = ptr + y11 * step;
            uchar *tptr1 = ptr + y12 * step;
            
            if( !fill )
            {
                ICV_PUT_POINT( tptr0, x11 );
                ICV_PUT_POINT( tptr1, x11 );
                ICV_PUT_POINT( tptr0, x12 );
                ICV_PUT_POINT( tptr1, x12 );
            }
            else
            {
                ICV_HLINE( tptr0, x11, x12, color, pix_size );
                ICV_HLINE( tptr1, x11, x12, color, pix_size );
            }

            tptr0 = ptr + y21 * step;
            tptr1 = ptr + y22 * step;

            if( !fill )
            {
                ICV_PUT_POINT( tptr0, x21 );
                ICV_PUT_POINT( tptr1, x21 );
                ICV_PUT_POINT( tptr0, x22 );
                ICV_PUT_POINT( tptr1, x22 );
            }
            else
            {
                ICV_HLINE( tptr0, x21, x22, color, pix_size );
                ICV_HLINE( tptr1, x21, x22, color, pix_size );
            }
        }
        else if( x11 < size.width && x12 >= 0 && y21 < size.height && y22 >= 0 )
        {
            if( fill )
            {
                x11 = std::max( x11, 0 );
                x12 = MIN( x12, size.width - 1 );
            }
            
            if( (unsigned)y11 < (unsigned)size.height )
            {
                uchar *tptr = ptr + y11 * step;

                if( !fill )
                {
                    if( x11 >= 0 )
                        ICV_PUT_POINT( tptr, x11 );
                    if( x12 < size.width )
                        ICV_PUT_POINT( tptr, x12 );
                }
                else
                    ICV_HLINE( tptr, x11, x12, color, pix_size );
            }

            if( (unsigned)y12 < (unsigned)size.height )
            {
                uchar *tptr = ptr + y12 * step;

                if( !fill )
                {
                    if( x11 >= 0 )
                        ICV_PUT_POINT( tptr, x11 );
                    if( x12 < size.width )
                        ICV_PUT_POINT( tptr, x12 );
                }
                else
                    ICV_HLINE( tptr, x11, x12, color, pix_size );
            }

            if( x21 < size.width && x22 >= 0 )
            {
                if( fill )
                {
                    x21 = std::max( x21, 0 );
                    x22 = MIN( x22, size.width - 1 );
                }

                if( (unsigned)y21 < (unsigned)size.height )
                {
                    uchar *tptr = ptr + y21 * step;

                    if( !fill )
                    {
                        if( x21 >= 0 )
                            ICV_PUT_POINT( tptr, x21 );
                        if( x22 < size.width )
                            ICV_PUT_POINT( tptr, x22 );
                    }
                    else
                        ICV_HLINE( tptr, x21, x22, color, pix_size );
                }

                if( (unsigned)y22 < (unsigned)size.height )
                {
                    uchar *tptr = ptr + y22 * step;

                    if( !fill )
                    {
                        if( x21 >= 0 )
                            ICV_PUT_POINT( tptr, x21 );
                        if( x22 < size.width )
                            ICV_PUT_POINT( tptr, x22 );
                    }
                    else
                        ICV_HLINE( tptr, x21, x22, color, pix_size );
                }
            }
        }
        dy++;
        err += plus;
        plus += 2;

        mask = (err <= 0) - 1;

        err -= minus & mask;
        dx += mask;
        minus -= mask & 2;
    }

    #undef  ICV_PUT_POINT
}


static void
ThickLine( Mat& img, Point p0, Point p1, const void* color,
           int thickness, int line_type, int flags, int shift )
{
    static const double INV_XY_ONE = 1./XY_ONE;

    p0.x <<= XY_SHIFT - shift;
    p0.y <<= XY_SHIFT - shift;
    p1.x <<= XY_SHIFT - shift;
    p1.y <<= XY_SHIFT - shift;

    if( thickness <= 1 )
    {
        if( line_type < CV_AA )
        {
            if( line_type == 1 || line_type == 4 || shift == 0 )
            {
                p0.x = (p0.x + (XY_ONE>>1)) >> XY_SHIFT;
                p0.y = (p0.y + (XY_ONE>>1)) >> XY_SHIFT;
                p1.x = (p1.x + (XY_ONE>>1)) >> XY_SHIFT;
                p1.y = (p1.y + (XY_ONE>>1)) >> XY_SHIFT;
                Line( img, p0, p1, color, line_type );
            }
            else
                Line2( img, p0, p1, color );
        }
        else
            LineAA( img, p0, p1, color );
    }
    else
    {
        Point pt[4], dp = Point(0,0);
        double dx = (p0.x - p1.x)*INV_XY_ONE, dy = (p1.y - p0.y)*INV_XY_ONE;
        double r = dx * dx + dy * dy;
        int i, oddThickness = thickness & 1;
        thickness <<= XY_SHIFT - 1;

        if( fabs(r) > DBL_EPSILON )
        {
            r = (thickness + oddThickness*XY_ONE*0.5)/std::sqrt(r);
            dp.x = cvRound( dy * r );
            dp.y = cvRound( dx * r );

            pt[0].x = p0.x + dp.x;
            pt[0].y = p0.y + dp.y;
            pt[1].x = p0.x - dp.x;
            pt[1].y = p0.y - dp.y;
            pt[2].x = p1.x - dp.x;
            pt[2].y = p1.y - dp.y;
            pt[3].x = p1.x + dp.x;
            pt[3].y = p1.y + dp.y;

            FillConvexPoly( img, pt, 4, color, line_type, XY_SHIFT );
        }

        for( i = 0; i < 2; i++ )
        {
            if( flags & (i+1) )
            {
                if( line_type < CV_AA )
                {
                    Point center;
                    center.x = (p0.x + (XY_ONE>>1)) >> XY_SHIFT;
                    center.y = (p0.y + (XY_ONE>>1)) >> XY_SHIFT;
                    Circle( img, center, (thickness + (XY_ONE>>1)) >> XY_SHIFT, color, 1 ); 
                }
                else
                {
                    EllipseEx( img, p0, cvSize(thickness, thickness),
                               0, 0, 360, color, -1, line_type );
                }
            }
            p0 = p1;
        }
    }
}


static void
PolyLine( Mat& img, const Point* v, int count, bool is_closed,
          const void* color, int thickness,
          int line_type, int shift )
{
    if( !v || count <= 0 )
        return;
    
    int i = is_closed ? count - 1 : 0;
    int flags = 2 + !is_closed;
    Point p0;
    CV_Assert( 0 <= shift && shift <= XY_SHIFT && thickness >= 0 );

    p0 = v[i];
    for( i = !is_closed; i < count; i++ )
    {
        Point p = v[i];
        ThickLine( img, p0, p, color, thickness, line_type, flags, shift );
        p0 = p;
        flags = 2;
    }
}

/****************************************************************************************\
*                              External functions                                        *
\****************************************************************************************/

void line( Mat& img, Point pt1, Point pt2, const Scalar& color,
           int thickness, int line_type, int shift )
{
    if( line_type == CV_AA && img.depth() != CV_8U )
        line_type = 8;

    CV_Assert( 0 <= thickness && thickness <= 255 );
    CV_Assert( 0 <= shift && shift <= XY_SHIFT );

    double buf[4];
    scalarToRawData( color, buf, img.type(), 0 );
    ThickLine( img, pt1, pt2, buf, thickness, line_type, 3, shift ); 
}

void rectangle( Mat& img, Point pt1, Point pt2,
                const Scalar& color, int thickness,
                int lineType, int shift )
{
    if( lineType == CV_AA && img.depth() != CV_8U )
        lineType = 8;

    CV_Assert( thickness <= 255 );
    CV_Assert( 0 <= shift && shift <= XY_SHIFT );

    double buf[4];
    scalarToRawData(color, buf, img.type(), 0);

    Point pt[4];

    pt[0] = pt1;
    pt[1].x = pt2.x;
    pt[1].y = pt1.y;
    pt[2] = pt2;
    pt[3].x = pt1.x;
    pt[3].y = pt2.y;

    if( thickness >= 0 )
        PolyLine( img, pt, 4, true, buf, thickness, lineType, shift );
    else
        FillConvexPoly( img, pt, 4, buf, lineType, shift );
}

    
void rectangle( Mat& img, Rect rec,
                const Scalar& color, int thickness,
                int lineType, int shift )
{
    CV_Assert( 0 <= shift && shift <= XY_SHIFT );
    if( rec.area() > 0 )
        rectangle( img, rec.tl(), rec.br() - Point(1<<shift,1<<shift),
                   color, thickness, lineType, shift );
}

    
void circle( Mat& img, Point center, int radius,
             const Scalar& color, int thickness, int line_type, int shift )
{
    if( line_type == CV_AA && img.depth() != CV_8U )
        line_type = 8;

    CV_Assert( radius >= 0 && thickness <= 255 &&
        0 <= shift && shift <= XY_SHIFT );

    double buf[4];
    scalarToRawData(color, buf, img.type(), 0);

    if( thickness > 1 || line_type >= CV_AA )
    {
        center.x <<= XY_SHIFT - shift;
        center.y <<= XY_SHIFT - shift;
        radius <<= XY_SHIFT - shift;
        EllipseEx( img, center, Size(radius, radius),
                   0, 0, 360, buf, thickness, line_type );
    }
    else
        Circle( img, center, radius, buf, thickness < 0 );
}


void ellipse( Mat& img, Point center, Size axes,
              double angle, double start_angle, double end_angle,
              const Scalar& color, int thickness, int line_type, int shift )
{
    if( line_type == CV_AA && img.depth() != CV_8U )
        line_type = 8;

    CV_Assert( axes.width >= 0 && axes.height >= 0 &&
        thickness <= 255 && 0 <= shift && shift <= XY_SHIFT );

    double buf[4];
    scalarToRawData(color, buf, img.type(), 0);

    int _angle = cvRound(angle);
    int _start_angle = cvRound(start_angle);
    int _end_angle = cvRound(end_angle);
    center.x <<= XY_SHIFT - shift;
    center.y <<= XY_SHIFT - shift;
    axes.width <<= XY_SHIFT - shift;
    axes.height <<= XY_SHIFT - shift;

    EllipseEx( img, center, axes, _angle, _start_angle,
               _end_angle, buf, thickness, line_type );
}
    
void ellipse(Mat& img, const RotatedRect& box, const Scalar& color,
             int thickness, int lineType)
{
    if( lineType == CV_AA && img.depth() != CV_8U )
        lineType = 8;
    
    CV_Assert( box.size.width >= 0 && box.size.height >= 0 &&
               thickness <= 255 );
    
    double buf[4];
    scalarToRawData(color, buf, img.type(), 0);
    
    int _angle = cvRound(box.angle);
    Point center(cvRound(box.center.x*(1 << XY_SHIFT)),
                 cvRound(box.center.y*(1 << XY_SHIFT)));
    Size axes(cvRound(box.size.width*(1 << (XY_SHIFT - 1))),
              cvRound(box.size.height*(1 << (XY_SHIFT - 1))));
    EllipseEx( img, center, axes, _angle, 0, 360, buf, thickness, lineType );    
}

void fillConvexPoly( Mat& img, const Point* pts, int npts,
                     const Scalar& color, int line_type, int shift )
{
    if( !pts || npts <= 0 )
        return;

    if( line_type == CV_AA && img.depth() != CV_8U )
        line_type = 8;

    double buf[4];
    CV_Assert( 0 <= shift && shift <=  XY_SHIFT );
    scalarToRawData(color, buf, img.type(), 0);
    FillConvexPoly( img, pts, npts, buf, line_type, shift );
}


void fillPoly( Mat& img, const Point** pts, const int* npts, int ncontours,
               const Scalar& color, int line_type,
               int shift, Point offset )
{
    if( line_type == CV_AA && img.depth() != CV_8U )
        line_type = 8;

    CV_Assert( pts && npts && ncontours >= 0 && 0 <= shift && shift <= XY_SHIFT );

    double buf[4];
    scalarToRawData(color, buf, img.type(), 0);

    vector<PolyEdge> edges;

    int i, total = 0;
    for( i = 0; i < ncontours; i++ )
        total += npts[i];

    edges.reserve( total + 1 );
    for( i = 0; i < ncontours; i++ )
        CollectPolyEdges( img, pts[i], npts[i], edges, buf, line_type, shift, offset );

    FillEdgeCollection(img, edges, buf);
}


void polylines( Mat& img, const Point** pts, const int* npts, int ncontours, bool isClosed,
                const Scalar& color, int thickness, int line_type, int shift )
{
    if( line_type == CV_AA && img.depth() != CV_8U )
        line_type = 8;

    CV_Assert( pts && npts && ncontours >= 0 &&
               0 <= thickness && thickness <= 255 &&
               0 <= shift && shift <= XY_SHIFT );

    double buf[4];
    scalarToRawData( color, buf, img.type(), 0 );

    for( int i = 0; i < ncontours; i++ )
        PolyLine( img, pts[i], npts[i], isClosed, buf, thickness, line_type, shift );
}


enum { FONT_SIZE_SHIFT=8, FONT_ITALIC_ALPHA=(1 << 8),
       FONT_ITALIC_DIGIT=(2 << 8), FONT_ITALIC_PUNCT=(4 << 8),
       FONT_ITALIC_BRACES=(8 << 8), FONT_HAVE_GREEK=(16 << 8),
       FONT_HAVE_CYRILLIC=(32 << 8) };

static const int HersheyPlain[] = {
(5 + 4*16) + FONT_HAVE_GREEK,
199, 214, 217, 233, 219, 197, 234, 216, 221, 222, 228, 225, 211, 224, 210, 220,
200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 212, 213, 191, 226, 192,
215, 190, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 193, 84,
194, 85, 86, 87, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
195, 223, 196, 88 };

static const int HersheyPlainItalic[] = {
(5 + 4*16) + FONT_ITALIC_ALPHA + FONT_HAVE_GREEK,
199, 214, 217, 233, 219, 197, 234, 216, 221, 222, 228, 225, 211, 224, 210, 220,
200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 212, 213, 191, 226, 192,
215, 190, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 193, 84,
194, 85, 86, 87, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161,
162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176,
195, 223, 196, 88 };

static const int HersheyComplexSmall[] = {
(6 + 7*16) + FONT_HAVE_GREEK,
1199, 1214, 1217, 1275, 1274, 1271, 1272, 1216, 1221, 1222, 1219, 1232, 1211, 1231, 1210, 1220,
1200, 1201, 1202, 1203, 1204, 1205, 1206, 1207, 1208, 1209, 1212, 2213, 1241, 1238, 1242,
1215, 1273, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011, 1012, 1013,
1014, 1015, 1016, 1017, 1018, 1019, 1020, 1021, 1022, 1023, 1024, 1025, 1026, 1223, 1084,
1224, 1247, 586, 1249, 1101, 1102, 1103, 1104, 1105, 1106, 1107, 1108, 1109, 1110, 1111,
1112, 1113, 1114, 1115, 1116, 1117, 1118, 1119, 1120, 1121, 1122, 1123, 1124, 1125, 1126,
1225, 1229, 1226, 1246 };

static const int HersheyComplexSmallItalic[] = {
(6 + 7*16) + FONT_ITALIC_ALPHA + FONT_HAVE_GREEK,
1199, 1214, 1217, 1275, 1274, 1271, 1272, 1216, 1221, 1222, 1219, 1232, 1211, 1231, 1210, 1220,
1200, 1201, 1202, 1203, 1204, 1205, 1206, 1207, 1208, 1209, 1212, 1213, 1241, 1238, 1242,
1215, 1273, 1051, 1052, 1053, 1054, 1055, 1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063,
1064, 1065, 1066, 1067, 1068, 1069, 1070, 1071, 1072, 1073, 1074, 1075, 1076, 1223, 1084,
1224, 1247, 586, 1249, 1151, 1152, 1153, 1154, 1155, 1156, 1157, 1158, 1159, 1160, 1161,
1162, 1163, 1164, 1165, 1166, 1167, 1168, 1169, 1170, 1171, 1172, 1173, 1174, 1175, 1176,
1225, 1229, 1226, 1246 };

static const int HersheySimplex[] = {
(9 + 12*16) + FONT_HAVE_GREEK,
2199, 714, 717, 733, 719, 697, 734, 716, 721, 722, 728, 725, 711, 724, 710, 720,
700, 701, 702, 703, 704, 705, 706, 707, 708, 709, 712, 713, 691, 726, 692,
715, 690, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511, 512, 513,
514, 515, 516, 517, 518, 519, 520, 521, 522, 523, 524, 525, 526, 693, 584,
694, 2247, 586, 2249, 601, 602, 603, 604, 605, 606, 607, 608, 609, 610, 611,
612, 613, 614, 615, 616, 617, 618, 619, 620, 621, 622, 623, 624, 625, 626,
695, 723, 696, 2246 };

static const int HersheyDuplex[] = {
(9 + 12*16) + FONT_HAVE_GREEK,
2199, 2714, 2728, 2732, 2719, 2733, 2718, 2727, 2721, 2722, 2723, 2725, 2711, 2724, 2710, 2720,
2700, 2701, 2702, 2703, 2704, 2705, 2706, 2707, 2708, 2709, 2712, 2713, 2730, 2726, 2731,
2715, 2734, 2501, 2502, 2503, 2504, 2505, 2506, 2507, 2508, 2509, 2510, 2511, 2512, 2513,
2514, 2515, 2516, 2517, 2518, 2519, 2520, 2521, 2522, 2523, 2524, 2525, 2526, 2223, 2084,
2224, 2247, 587, 2249, 2601, 2602, 2603, 2604, 2605, 2606, 2607, 2608, 2609, 2610, 2611,
2612, 2613, 2614, 2615, 2616, 2617, 2618, 2619, 2620, 2621, 2622, 2623, 2624, 2625, 2626,
2225, 2229, 2226, 2246 };

static const int HersheyComplex[] = {
(9 + 12*16) + FONT_HAVE_GREEK + FONT_HAVE_CYRILLIC,
2199, 2214, 2217, 2275, 2274, 2271, 2272, 2216, 2221, 2222, 2219, 2232, 2211, 2231, 2210, 2220,
2200, 2201, 2202, 2203, 2204, 2205, 2206, 2207, 2208, 2209, 2212, 2213, 2241, 2238, 2242,
2215, 2273, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013,
2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022, 2023, 2024, 2025, 2026, 2223, 2084,
2224, 2247, 587, 2249, 2101, 2102, 2103, 2104, 2105, 2106, 2107, 2108, 2109, 2110, 2111,
2112, 2113, 2114, 2115, 2116, 2117, 2118, 2119, 2120, 2121, 2122, 2123, 2124, 2125, 2126,
2225, 2229, 2226, 2246 };

static const int HersheyComplexItalic[] = {
(9 + 12*16) + FONT_ITALIC_ALPHA + FONT_ITALIC_DIGIT + FONT_ITALIC_PUNCT +
FONT_HAVE_GREEK + FONT_HAVE_CYRILLIC,
2199, 2764, 2778, 2782, 2769, 2783, 2768, 2777, 2771, 2772, 2219, 2232, 2211, 2231, 2210, 2220,
2750, 2751, 2752, 2753, 2754, 2755, 2756, 2757, 2758, 2759, 2212, 2213, 2241, 2238, 2242,
2765, 2273, 2051, 2052, 2053, 2054, 2055, 2056, 2057, 2058, 2059, 2060, 2061, 2062, 2063,
2064, 2065, 2066, 2067, 2068, 2069, 2070, 2071, 2072, 2073, 2074, 2075, 2076, 2223, 2084,
2224, 2247, 587, 2249, 2151, 2152, 2153, 2154, 2155, 2156, 2157, 2158, 2159, 2160, 2161,
2162, 2163, 2164, 2165, 2166, 2167, 2168, 2169, 2170, 2171, 2172, 2173, 2174, 2175, 2176,
2225, 2229, 2226, 2246 };

static const int HersheyTriplex[] = {
(9 + 12*16) + FONT_HAVE_GREEK,
2199, 3214, 3228, 3232, 3219, 3233, 3218, 3227, 3221, 3222, 3223, 3225, 3211, 3224, 3210, 3220,
3200, 3201, 3202, 3203, 3204, 3205, 3206, 3207, 3208, 3209, 3212, 3213, 3230, 3226, 3231,
3215, 3234, 3001, 3002, 3003, 3004, 3005, 3006, 3007, 3008, 3009, 3010, 3011, 3012, 3013,
2014, 3015, 3016, 3017, 3018, 3019, 3020, 3021, 3022, 3023, 3024, 3025, 3026, 2223, 2084,
2224, 2247, 587, 2249, 3101, 3102, 3103, 3104, 3105, 3106, 3107, 3108, 3109, 3110, 3111,
3112, 3113, 3114, 3115, 3116, 3117, 3118, 3119, 3120, 3121, 3122, 3123, 3124, 3125, 3126,
2225, 2229, 2226, 2246 };

static const int HersheyTriplexItalic[] = {
(9 + 12*16) + FONT_ITALIC_ALPHA + FONT_ITALIC_DIGIT +
FONT_ITALIC_PUNCT + FONT_HAVE_GREEK,
2199, 3264, 3278, 3282, 3269, 3233, 3268, 3277, 3271, 3272, 3223, 3225, 3261, 3224, 3260, 3270,
3250, 3251, 3252, 3253, 3254, 3255, 3256, 3257, 3258, 3259, 3262, 3263, 3230, 3226, 3231,
3265, 3234, 3051, 3052, 3053, 3054, 3055, 3056, 3057, 3058, 3059, 3060, 3061, 3062, 3063,
2064, 3065, 3066, 3067, 3068, 3069, 3070, 3071, 3072, 3073, 3074, 3075, 3076, 2223, 2084,
2224, 2247, 587, 2249, 3151, 3152, 3153, 3154, 3155, 3156, 3157, 3158, 3159, 3160, 3161,
3162, 3163, 3164, 3165, 3166, 3167, 3168, 3169, 3170, 3171, 3172, 3173, 3174, 3175, 3176,
2225, 2229, 2226, 2246 };

static const int HersheyScriptSimplex[] = {
(9 + 12*16) + FONT_ITALIC_ALPHA + FONT_HAVE_GREEK,
2199, 714, 717, 733, 719, 697, 734, 716, 721, 722, 728, 725, 711, 724, 710, 720,
700, 701, 702, 703, 704, 705, 706, 707, 708, 709, 712, 713, 691, 726, 692,
715, 690, 551, 552, 553, 554, 555, 556, 557, 558, 559, 560, 561, 562, 563,
564, 565, 566, 567, 568, 569, 570, 571, 572, 573, 574, 575, 576, 693, 584,
694, 2247, 586, 2249, 651, 652, 653, 654, 655, 656, 657, 658, 659, 660, 661,
662, 663, 664, 665, 666, 667, 668, 669, 670, 671, 672, 673, 674, 675, 676,
695, 723, 696, 2246 };

static const int HersheyScriptComplex[] = {
(9 + 12*16) + FONT_ITALIC_ALPHA + FONT_ITALIC_DIGIT + FONT_ITALIC_PUNCT + FONT_HAVE_GREEK,
2199, 2764, 2778, 2782, 2769, 2783, 2768, 2777, 2771, 2772, 2219, 2232, 2211, 2231, 2210, 2220,
2750, 2751, 2752, 2753, 2754, 2755, 2756, 2757, 2758, 2759, 2212, 2213, 2241, 2238, 2242,
2215, 2273, 2551, 2552, 2553, 2554, 2555, 2556, 2557, 2558, 2559, 2560, 2561, 2562, 2563,
2564, 2565, 2566, 2567, 2568, 2569, 2570, 2571, 2572, 2573, 2574, 2575, 2576, 2223, 2084,
2224, 2247, 586, 2249, 2651, 2652, 2653, 2654, 2655, 2656, 2657, 2658, 2659, 2660, 2661,
2662, 2663, 2664, 2665, 2666, 2667, 2668, 2669, 2670, 2671, 2672, 2673, 2674, 2675, 2676,
2225, 2229, 2226, 2246 };

const char* g_HersheyGlyphs[] = {
	"",
	"MWRMNV RMVV PSTS",
	"MWOMOV OMSMUNUPSQ OQSQURUUSVOV",
	"MXVNTMRMPNOPOSPURVTVVU",
	"MWOMOV OMRMTNUPUSTURVOV",
	"MWOMOV OMUM OQSQ OVUV",
	"MVOMOV OMUM OQSQ",
	"MXVNTMRMPNOPOSPURVTVVUVR SRVR",
	"MWOMOV UMUV OQUQ",
	"PTRMRV",
	"NUSMSTRVPVOTOS",
	"MWOMOV UMOS QQUV",
	"MVOMOV OVUV",
	"LXNMNV NMRV VMRV VMVV",
	"MWOMOV OMUV UMUV",
	"MXRMPNOPOSPURVSVUUVSVPUNSMRM",
	"MWOMOV OMSMUNUQSROR",
	"MXRMPNOPOSPURVSVUUVSVPUNSMRM STVW",
	"MWOMOV OMSMUNUQSROR RRUV",
	"MWUNSMQMONOOPPTRUSUUSVQVOU",
	"MWRMRV NMVM",
	"MXOMOSPURVSVUUVSVM",
	"MWNMRV VMRV",
	"LXNMPV RMPV RMTV VMTV",
	"MWOMUV UMOV",
	"MWNMRQRV VMRQ",
	"MWUMOV OMUM OVUV",
	"MWRMNV RMVV PSTS",
	"MWOMOV OMSMUNUPSQ OQSQURUUSVOV",
	"MVOMOV OMUM",
	"MWRMNV RMVV NVVV",
	"MWOMOV OMUM OQSQ OVUV",
	"MWUMOV OMUM OVUV",
	"MWOMOV UMUV OQUQ",
	"MXRMPNOPOSPURVSVUUVSVPUNSMRM QQTR TQQR",
	"PTRMRV",
	"MWOMOV UMOS QQUV",
	"MWRMNV RMVV",
	"LXNMNV NMRV VMRV VMVV",
	"MWOMOV OMUV UMUV",
	"MWOMUM PQTR TQPR OVUV",
	"MXRMPNOPOSPURVSVUUVSVPUNSMRM",
	"MWOMOV UMUV OMUM",
	"MWOMOV OMSMUNUQSROR",
	"MWOMRQOV OMUM OVUV",
	"MWRMRV NMVM",
	"MWNONNOMPMQNRPRV VOVNUMTMSNRP",
	"LXRMRV PONPNSPTTTVSVPTOPO",
	"MWOMUV UMOV",
	"LXRMRV NOOPOSQTSTUSUPVO",
	"MXOVQVOROPPNRMSMUNVPVRTVVV",
	"MWSMMV SMUV OSTS",
	"MWQMNV QMTMVNVPSQPQ SQURUTTURVNV",
	"LXVPUNTMRMPNOONQNSOUPVRVTUUT",
	"MXQMNV QMUMVOVQUTTURVNV",
	"MVQMNV QMVM PQSQ NVSV",
	"MVQMNV QMVM PQSQ",
	"LXVPUNTMRMPNOONQNSOUPVRVTUUSRS",
	"MXQMNV WMTV PQUQ",
	"PUTMQV",
	"OVUMSSRUQVPVOUOT",
	"MVQMNV VMOS RQTV",
	"NVRMOV OVTV",
	"LYPMMV PMQV XMQV XMUV",
	"MXQMNV QMTV WMTV",
	"LXRMPNOONQNSOUPVRVTUUTVRVPUNTMRM",
	"MWQMNV QMUMVNVPUQSRPR",
	"LXRMPNOONQNSOUPVRVTUUTVRVPUNTMRM QVPUPTQSRSSTTVUWVW",
	"MWQMNV QMUMVNVPUQSRPR QRRUSVTVUU",
	"MWVNTMRMPNPPQQTRUSUUSVPVNU",
	"MVSMPV PMVM",
	"LXPMNSNUOVRVTUUSWM",
	"MWOMQV WMQV",
	"KXNMNV SMNV SMSV XMSV",
	"NWQMTV WMNV",
	"NWQMSQQV WMSQ",
	"MWQMWMNVTV",
	"",
	"",
	"",
	"",
	"",
	"",
	"LXNMRV VMRV NMVM",
	"MWNLVX",
	"LXRONU ROVU",
	"MWNVVV",
	"PVRMUQ",
	"MWMMOKQKTMVMWK",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"NWQPTPUQUV URQSPTPUQVSVUU",
	"MWOMOV OSPURVTUUSTQRPPQOS",
	"MWUQSPRPPQOSPURVSVUU",
	"MWUMUV USTQRPPQOSPURVTUUS",
	"MWOSUSTQRPPQOSPURVTV",
	"NVUNTMSMRNRV PPTP",
	"MWUPUVTXRYPY USTQRPPQOSPURVTUUS",
	"MWOMOV OSPQRPTQUSUV",
	"PTRLQMRNSMRL RPRV",
	"PUSLRMSNTMSL SPSXRYQYPX",
	"NWPMPV UPPT RSUV",
	"PTRMRV",
	"KYMPMV MSNQOPPPQQRSRV RSSQTPUPVQWSWV",
	"MWOPOV OSPQRPTQUSUV",
	"MWRPPQOSPURVTUUSTQRP",
	"MWOPOY OSPURVTUUSTQRPPQOS",
	"MWUPUY USTQRPPQOSPURVTUUS",
	"NVPPPV PSQQSPTP",
	"NWUQTPQPPQPRQSTSUTUUTVQVPU",
	"NVRMRUSVTVUU PPTP",
	"MWUPUV OPOSPURVTUUS",
	"NVOPRV UPRV",
	"LXNPPV RPPV RPTV VPTV",
	"MWOPUV UPOV",
	"MWOPRV UPRVQXPYOY",
	"MWOPUPOVUV",
	"MXVPUSTURVPUOSPQRPTQUUVV",
	"MWOTQVSVTUTSSRPQRQTPUOUNTMRMQNPPOTNY",
	"MXNQOPQPRQRSQW VPURSTQWPY",
	"MWTNSMRMQNQORPTQUSTURVPUOSPQRP",
	"NWUQSPQPPQPRQS SSQSPTPUQVSVUU",
	"NWTMSNSOTP UPSPQQPSPUQVSWSXRYQY",
	"LXNQOPPPQQQSPV QSRQTPUPVQVSUVTY",
	"LXNQOPPPQQQURVSVTUUSVPVNUMTMSNSPTRUSWT",
	"OVRPQSQURVSVTU",
	"MWQPOV UPTPRQPS PSQUSVTV",
	"MWOMPMQNRPUV RPOV",
	"LYPPMY UPTSSUQVPVOUOS TSTUUVVVWU",
	"MWNPOPOV UPTSRUOV",
	"NWTMSNSOTP UPSPQQQRRSTS SSQTPUPVQWSXSYRZQZ",
	"MWRPPQOSPURVTUUSTQRP",
	"MXOQQPVP QPQRPV TPTRUV",
	"MWOSPURVTUUSTQRPPQOSNY",
	"MXVPRPPQOSPURVTUUSTQRP",
	"MXOQQPVP SPRV",
	"KXMQNPOPPQPUQVSVTUUSVP",
	"MXPPOQOSPURVSVUUVSVQUPTPSQRSQY",
	"MWOPPPQQSXTYUY UPTRPWOY",
	"KYTMRY MQNPOPPQPUQVTVUUVSWP",
	"LXOPNRNTOVQVRTRR UPVRVTUVSVRT",
	"LWTSSQQPOQNSOUQVSUTS UPTSTUUVVV",
	"MWQMOSPURVTUUSTQRPPQOS",
	"MWUQSPRPPQOSPURVTV",
	"LWTSSQQPOQNSOUQVSUTS VMTSTUUVVV",
	"MWOSTSURUQSPRPPQOSPURVTV",
	"OVVMUMTNSPQVPXOYNY QPUP",
	"MXUSTQRPPQOSPURVTUUS VPTVSXRYPYOX",
	"MVQMNV OSPQQPSPTQTRSTSUTVUV",
	"PUSMSNTNTMSM QPRPSQSRRTRUSVTV",
	"OUSMSNTNTMSM QPRPSQSRRVQXPYOYNX",
	"NVRMOV UPTPRQPS PSQUSVTV",
	"OTSMQSQURVSV",
	"JYKPLPMQMSLV MSNQOPQPRQRSQV RSSQTPVPWQWRVTVUWVXV",
	"MWNPOPPQPSOV PSQQRPTPUQURTTTUUVVV",
	"MWRPPQOSPURVTUUSTQRP",
	"MXNPOPPQPSNY PSQUSVUUVSUQSPQQPS",
	"MXUSTQRPPQOSPURVTUUS VPSY",
	"MVOPPPQQQSPV UQTPSPRQQS",
	"NVTQSPQPPQPRQSRSSTSURVPVOU",
	"NUSMQSQURVSV PPTP",
	"MWNPOPPQPROTOUPVRVSUTS UPTSTUUVVV",
	"MWNPOPPQPROTOUPVRVTUURUP",
	"KYLPMPNQNRMTMUNVPVQURSSP RSRUSVUVVUWRWP",
	"MWOQPPQPRQRUSVTVUU VQUPTPSQQUPVOVNU",
	"MWNPOPPQPROTOUPVRVSUTS UPSVRXQYOYNX",
	"NVUPOV PQQPSPTQ PUQVSVTU",
	"",
	"",
	"",
	"",
	"",
	"",
	"MWUSTQRPPQOSPURVTUUSUPTNRMQM",
	"MWUQSPRPPQOSPURVSVUU OSSS",
	"MWRMQNPPOSOVPWRWSVTTUQUNTMRM PRTR",
	"MWTMQY RPPQOSPURVSVUUVSUQSPRP",
	"MWUQSPQPOQOSPTRUSVSWRXQX",
	"",
	"",
	"KYTPTSUTVTWSWQVOUNSMQMONNOMQMSNUOVQWSWUV TQSPQPPQPSQTSTTS",
	"MWUNORUV",
	"MWONUROV",
	"OUTKQKQYTY",
	"OUPKSKSYPY",
	"OUTKSLRNROSQQRSSRURVSXTY",
	"OUPKQLRNROQQSRQSRURVQXPY",
	"LYPMQNQOPPOPNONNOMPMSNUNWMNV USTTTUUVVVWUWTVSUS",
	"PT",
	"NV",
	"MWRMPNOPOSPURVTUUSUPTNRM",
	"MWPORMRV",
	"MWONQMSMUNUPTROVUV",
	"MWONQMSMUNUPSQ RQSQURUUSVQVOU",
	"MWSMSV SMNSVS",
	"MWPMOQQPRPTQUSTURVQVOU PMTM",
	"MWTMRMPNOPOSPURVTUUSTQRPPQOS",
	"MWUMQV OMUM",
	"MWQMONOPQQSQUPUNSMQM QQOROUQVSVUUURSQ",
	"MWUPTRRSPROPPNRMTNUPUSTURVPV",
	"PURURVSVSURU",
	"PUSVRVRUSUSWRY",
	"PURPRQSQSPRP RURVSVSURU",
	"PURPRQSQSPRP SVRVRUSUSWRY",
	"PURMRR SMSR RURVSVSURU",
	"NWPNRMSMUNUPRQRRSRSQUP RURVSVSURU",
	"PTRMRQ",
	"NVPMPQ TMTQ",
	"NVQMPNPPQQSQTPTNSMQM",
	"MWRKRX UNSMQMONOPQQTRUSUUSVQVOU",
	"MWVLNX",
	"OUTKRNQQQSRVTY",
	"OUPKRNSQSSRVPY",
	"PTRKRY",
	"LXNRVR",
	"LXRNRV NRVR",
	"LXNPVP NTVT",
	"MWOOUU UOOU",
	"MWRORU OPUT UPOT",
	"PURQRRSRSQRQ",
	"PUSMRORQSQSPRP",
	"PUSNRNRMSMSORQ",
	"LXSOVRSU NRVR",
	"MXQLQY TLTY OQVQ OTVT",
	"LXVRURTSSURVOVNUNSORRQSPSNRMPMONOPQSSUUVVV",
	"LXNNOQOSNV VNUQUSVV NNQOSOVN NVQUSUVV",
	"LYRQQPOPNQNSOTQTRSSQTPVPWQWSVTTTSSRQ",
	"",
	"H\\NRMQLRMSNR VRWQXRWSVR",
	"H\\MPLQLRMSNSOROQNPMP MQMRNRNQMQ WPVQVRWSXSYRYQXPWP WQWRXRXQWQ",
	"I[KRYR",
	"",
	"H\\RUJPRTZPRU",
	"",
	"",
	"",
	"",
	"",
	"F^ISJQLPNPPQTTVUXUZT[Q ISJPLONOPPTSVTXTZS[Q IYJWLVNVPWTZV[X[ZZ[W IYJVLUNUPVTYVZXZZY[W",
	"",
	"F^ISJQLPNPPQTTVUXUZT[Q ISJPLONOPPTSVTXTZS[Q IW[W I[[[",
	"",
	"CaGO]OXI L[GU]U",
	"",
	"D`F^^^^FFFF^",
	"",
	"KYQVOUNSNQOOQNSNUOVQVSUUSVQV SVVS QVVQ OUUO NSSN NQQN",
	"",
	"H\\IR[R",
	"H\\IR[R IQ[Q",
	"",
	"LYPFSCSP RDRP OPVP MRXR OVOWNWNVOUQTTTVUWWVYTZQ[O\\N^Na TTUUVWUYTZ N`O_P_S`V`W_ P_SaVaW_W^",
	"LYPFSCSP RDRP OPVP MRXR OVOWNWNVOUQTTTVUWWVYTZ TTUUVWUYTZ RZTZV[W]W^V`TaQaO`N_N^O^O_ TZU[V]V^U`Ta",
	"LYPFSCSP RDRP OPVP MRXR VVVWWWWVVUTTRTPUOVNYN^O`QaTaV`W^W\\VZTYQYN[ RTPVOYO^P`Qa TaU`V^V\\UZTY",
	"LYPFSCSP RDRP OPVP MRXR QTOUNWOYQZTZVYWWVUTTQT QTPUOWPYQZ TZUYVWUUTT QZO[N]N^O`QaTaV`W^W]V[TZ QZP[O]O^P`Qa TaU`V^V]U[TZ",
	"LYOEOFNFNEODQCTCVDWFVHTIQJOKNMNP TCUDVFUHTI NOONPNSOVOWN PNSPVPWNWM MRXR OVOWNWNVOUQTTTVUWWVYTZ TTUUVWUYTZ RZTZV[W]W^V`TaQaO`N_N^O^O_ TZU[V]V^U`Ta",
	"LYOEOFNFNEODQCTCVDWFVHTI TCUDVFUHTI RITIVJWLWMVOTPQPOONNNMOMON TIUJVLVMUOTP MRXR QTOUNWOYQZTZVYWWVUTTQT QTPUOWPYQZ TZUYVWUUTT QZO[N]N^O`QaTaV`W^W]V[TZ QZP[O]O^P`Qa TaU`V^V]U[TZ",
	"LYOCNI OCVC ODSDVC NIOHQGTGVHWJWMVOTPQPOONNNMOMON TGUHVJVMUOTP MRXR QTOUNWOYQZTZVYWWVUTTQT QTPUOWPYQZ TZUYVWUUTT QZO[N]N^O`QaTaV`W^W]V[TZ QZP[O]O^P`Qa TaU`V^V]U[TZ",
	"LYNCNG VERLPP WCTIQP NEPCRCUE NEPDRDUEVE MRXR QTOUNWOYQZTZVYWWVUTTQT QTPUOWPYQZ TZUYVWUUTT QZO[N]N^O`QaTaV`W^W]V[TZ QZP[O]O^P`Qa TaU`V^V]U[TZ",
	"LYOCNI OCVC ODSDVC NIOHQGTGVHWJWMVOTPQPOONNNMOMON TGUHVJVMUOTP MRXR VVVWWWWVVUTTRTPUOVNYN^O`QaTaV`W^W\\VZTYQYN[ RTPVOYO^P`Qa TaU`V^V\\UZTY",
	"LYPFSCSP RDRP OPVP MRXR SVSa TTTa TTM]X] QaVa",
	"LYOEOFNFNEODQCTCVDWFVHTI TCUDVFUHTI RITIVJWLWMVOTPQPOONNNMOMON TIUJVLVMUOTP MRXR SVSa TTTa TTM]X] QaVa",
	"F^YXWZU[R[PZMXKWIWHXHZI[K[MZOWPURQTKWGYFZF[G\\H[IZH[G[FZFYFWGVHTLRPPVNZMZ OPUP",
	"E^P[MZJXHUGRGOHLJIMGPFTFWGYI[L\\O\\R[UYXVZS[P[ NJNW OJOW LJSJVKWMWNVPSQOQ SJUKVMVNUPSQ LWQW SQTRUVVWWWXV SQURVVWW",
	"E^P[MZJXHUGRGOHLJIMGPFTFWGYI[L\\O\\R[UYXVZS[P[ UKVJVNUKSJPJNKMLLOLRMUNVPWSWUVVT PJNLMOMRNUPW",
	"E_IM[M IR[R IW[W K[YI",
	"CaHQGRHSIRHQ RQQRRSSRRQ \\Q[R\\S]R\\Q",
	"",
	"E_NWLTIRLPNM LPJRLT JRZR VWXT[RXPVM XPZRXT",
	"JZWNTLRIPLMN PLRJTL RJRZ WVTXR[PXMV PXRZTX",
	"F^ZJSJOKMLKNJQJSKVMXOYSZZZ SFS^",
	"F^JJQJUKWLYNZQZSYVWXUYQZJZ QFQ^",
	"F^JJQJUKWLYNZQZSYVWXUYQZJZ ORZR",
	"",
	"H\\LBL[ RBR[ XBX[",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"I[RFJ[ RFZ[ MTWT",
	"G\\KFK[ KFTFWGXHYJYLXNWOTP KPTPWQXRYTYWXYWZT[K[",
	"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZV",
	"G\\KFK[ KFRFUGWIXKYNYSXVWXUZR[K[",
	"H[LFL[ LFYF LPTP L[Y[",
	"HZLFL[ LFYF LPTP",
	"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZVZS USZS",
	"G]KFK[ YFY[ KPYP",
	"NVRFR[",
	"JZVFVVUYTZR[P[NZMYLVLT",
	"G\\KFK[ YFKT POY[",
	"HYLFL[ L[X[",
	"F^JFJ[ JFR[ ZFR[ ZFZ[",
	"G]KFK[ KFY[ YFY[",
	"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF",
	"G\\KFK[ KFTFWGXHYJYMXOWPTQKQ",
	"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF SWY]",
	"G\\KFK[ KFTFWGXHYJYLXNWOTPKP RPY[",
	"H\\YIWGTFPFMGKIKKLMMNOOUQWRXSYUYXWZT[P[MZKX",
	"JZRFR[ KFYF",
	"G]KFKULXNZQ[S[VZXXYUYF",
	"I[JFR[ ZFR[",
	"F^HFM[ RFM[ RFW[ \\FW[",
	"H\\KFY[ YFK[",
	"I[JFRPR[ ZFRP",
	"H\\YFK[ KFYF K[Y[",
	"I[RFJ[ RFZ[ MTWT",
	"G\\KFK[ KFTFWGXHYJYLXNWOTP KPTPWQXRYTYWXYWZT[K[",
	"HYLFL[ LFXF",
	"I[RFJ[ RFZ[ J[Z[",
	"H[LFL[ LFYF LPTP L[Y[",
	"H\\YFK[ KFYF K[Y[",
	"G]KFK[ YFY[ KPYP",
	"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF OPUP",
	"NVRFR[",
	"G\\KFK[ YFKT POY[",
	"I[RFJ[ RFZ[",
	"F^JFJ[ JFR[ ZFR[ ZFZ[",
	"G]KFK[ KFY[ YFY[",
	"I[KFYF OPUP K[Y[",
	"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF",
	"G]KFK[ YFY[ KFYF",
	"G\\KFK[ KFTFWGXHYJYMXOWPTQKQ",
	"I[KFRPK[ KFYF K[Y[",
	"JZRFR[ KFYF",
	"I[KKKILGMFOFPGQIRMR[ YKYIXGWFUFTGSIRM",
	"H\\RFR[ PKMLLMKOKRLTMUPVTVWUXTYRYOXMWLTKPK",
	"H\\KFY[ K[YF",
	"G]RFR[ ILJLKMLQMSNTQUSUVTWSXQYMZL[L",
	"H\\K[O[LTKPKLLINGQFSFVGXIYLYPXTU[Y[",
	"G[G[IZLWOSSLVFV[UXSUQSNQLQKRKTLVNXQZT[Y[",
	"F]SHTITLSPRSQUOXMZK[J[IZIWJRKOLMNJPHRGUFXFZG[I[KZMYNWOTP SPTPWQXRYTYWXYWZU[R[PZOX",
	"H\\TLTMUNWNYMZKZIYGWFTFQGOIMLLNKRKVLYMZO[Q[TZVXWV",
	"G^TFRGQIPMOSNVMXKZI[G[FZFXGWIWKXMZP[S[VZXXZT[O[KZHYGWFTFRHRJSMUPWRZT\\U",
	"H\\VJVKWLYLZKZIYGVFRFOGNINLONPOSPPPMQLRKTKWLYMZP[S[VZXXYV",
	"H\\RLPLNKMINGQFTFXG[G]F XGVNTTRXPZN[L[JZIXIVJULUNV QPZP",
	"G^G[IZMVPQQNRJRGQFPFOGNINLONQOUOXNYMZKZQYVXXVZS[O[LZJXIVIT",
	"F^MMKLJJJIKGMFNFPGQIQKPONULYJ[H[GZGX MRVOXN[L]J^H^G]F\\FZHXLVRUWUZV[W[YZZY\\V",
	"IZWVUTSQROQLQIRGSFUFVGWIWLVQTVSXQZO[M[KZJXJVKUMUOV",
	"JYT^R[PVOPOJPGRFTFUGVJVMURR[PaOdNfLgKfKdLaN^P\\SZWX",
	"F^MMKLJJJIKGMFNFPGQIQKPONULYJ[H[GZGX ^I^G]F\\FZGXIVLTNROPO ROSQSXTZU[V[XZYY[V",
	"I\\MRORSQVOXMYKYHXFVFUGTISNRSQVPXNZL[J[IZIXJWLWNXQZT[V[YZ[X",
	"@aEMCLBJBICGEFFFHGIIIKHPGTE[ GTJLLHMGOFPFRGSISKRPQTO[ QTTLVHWGYFZF\\G]I]K\\PZWZZ[[\\[^Z_YaV",
	"E]JMHLGJGIHGJFKFMGNINKMPLTJ[ LTOLQHRGTFVFXGYIYKXPVWVZW[X[ZZ[Y]V",
	"H]TFQGOIMLLNKRKVLYMZO[Q[TZVXXUYSZOZKYHXGVFTFRHRKSNUQWSZU\\V",
	"F_SHTITLSPRSQUOXMZK[J[IZIWJRKOLMNJPHRGUFZF\\G]H^J^M]O\\PZQWQUPTO",
	"H^ULTNSOQPOPNNNLOIQGTFWFYGZIZMYPWSSWPYNZK[I[HZHXIWKWMXPZS[V[YZ[X",
	"F_SHTITLSPRSQUOXMZK[J[IZIWJRKOLMNJPHRGUFYF[G\\H]J]M\\O[PYQVQSPTQUSUXVZX[ZZ[Y]V",
	"H\\H[JZLXOTQQSMTJTGSFRFQGPIPKQMSOVQXSYUYWXYWZT[P[MZKXJVJT",
	"H[RLPLNKMINGQFTFXG[G]F XGVNTTRXPZN[L[JZIXIVJULUNV",
	"E]JMHLGJGIHGJFKFMGNINKMOLRKVKXLZN[P[RZSYUUXMZF XMWQVWVZW[X[ZZ[Y]V",
	"F]KMILHJHIIGKFLFNGOIOKNOMRLVLYM[O[QZTWVTXPYMZIZGYFXFWGVIVKWNYP[Q",
	"C_HMFLEJEIFGHFIFKGLILLK[ UFK[ UFS[ aF_G\\JYNVTS[",
	"F^NLLLKKKILGNFPFRGSISLQUQXRZT[V[XZYXYVXUVU ]I]G\\FZFXGVITLPUNXLZJ[H[GZGX",
	"F]KMILHJHIIGKFLFNGOIOKNOMRLVLXMZN[P[RZTXVUWSYM [FYMVWT]RbPfNgMfMdNaP^S[VY[V",
	"H]ULTNSOQPOPNNNLOIQGTFWFYGZIZMYPWTTWPZN[K[JZJXKWNWPXQYR[R^QaPcNfLgKfKdLaN^Q[TYZV",
	"",
	"",
	"",
	"",
	"",
	"",
	"I[JFR[ ZFR[ JFZF",
	"G]IL[b",
	"E_RJIZ RJ[Z",
	"I[J[Z[",
	"I[J[Z[ZZJZJ[",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"I\\XMX[ XPVNTMQMONMPLSLUMXOZQ[T[VZXX",
	"H[LFL[ LPNNPMSMUNWPXSXUWXUZS[P[NZLX",
	"I[XPVNTMQMONMPLSLUMXOZQ[T[VZXX",
	"I\\XFX[ XPVNTMQMONMPLSLUMXOZQ[T[VZXX",
	"I[LSXSXQWOVNTMQMONMPLSLUMXOZQ[T[VZXX",
	"MYWFUFSGRJR[ OMVM",
	"I\\XMX]W`VaTbQbOa XPVNTMQMONMPLSLUMXOZQ[T[VZXX",
	"I\\MFM[ MQPNRMUMWNXQX[",
	"NVQFRGSFREQF RMR[",
	"MWRFSGTFSERF SMS^RaPbNb",
	"IZMFM[ WMMW QSX[",
	"NVRFR[",
	"CaGMG[ GQJNLMOMQNRQR[ RQUNWMZM\\N]Q][",
	"I\\MMM[ MQPNRMUMWNXQX[",
	"I\\QMONMPLSLUMXOZQ[T[VZXXYUYSXPVNTMQM",
	"H[LMLb LPNNPMSMUNWPXSXUWXUZS[P[NZLX",
	"I\\XMXb XPVNTMQMONMPLSLUMXOZQ[T[VZXX",
	"KXOMO[ OSPPRNTMWM",
	"J[XPWNTMQMNNMPNRPSUTWUXWXXWZT[Q[NZMX",
	"MYRFRWSZU[W[ OMVM",
	"I\\MMMWNZP[S[UZXW XMX[",
	"JZLMR[ XMR[",
	"G]JMN[ RMN[ RMV[ ZMV[",
	"J[MMX[ XMM[",
	"JZLMR[ XMR[P_NaLbKb",
	"J[XMM[ MMXM M[X[",
	"H]QMONMPLRKUKXLZN[P[RZUWWTYPZM QMSMTNUPWXXZY[Z[",
	"I\\UFSGQIOMNPMTLZKb UFWFYHYKXMWNUORO ROTPVRWTWWVYUZS[Q[OZNYMV",
	"I\\JPLNNMOMQNROSRSVR[ ZMYPXRR[P_Ob",
	"I[TMQMONMPLSLVMYNZP[R[TZVXWUWRVOTMRKQIQGRFTFVGXI",
	"JZWOVNTMQMONOPPRSS SSOTMVMXNZP[S[UZWX",
	"JYTFRGQHQIRJUKXK XKTMQONRMUMWNYP[S]T_TaSbQbP`",
	"H\\IQJOLMNMONOPNTL[ NTPPRNTMVMXOXRWWTb",
	"G\\HQIOKMMMNNNPMUMXNZO[Q[SZUWVUWRXMXJWGUFSFRHRJSMUPWRZT",
	"LWRMPTOXOZP[R[TYUW",
	"I[OMK[ YNXMWMUNQROSNS NSPTQUSZT[U[VZ",
	"JZKFMFOGPHX[ RML[",
	"H]OMIb NQMVMYO[Q[SZUXWT YMWTVXVZW[Y[[Y\\W",
	"I[LMOMNSMXL[ YMXPWRUURXOZL[",
	"JZTFRGQHQIRJUKXK UKRLPMOOOQQSTTVT TTPUNVMXMZO\\S^T_TaRbPb",
	"J[RMPNNPMSMVNYOZQ[S[UZWXXUXRWOVNTMRM",
	"G]PML[ UMVSWXX[ IPKNNM[M",
	"I[MSMVNYOZQ[S[UZWXXUXRWOVNTMRMPNNPMSIb",
	"I][MQMONMPLSLVMYNZP[R[TZVXWUWRVOUNSM",
	"H\\SMP[ JPLNOMZM",
	"H\\IQJOLMNMONOPMVMYO[Q[TZVXXTYPYM",
	"G]ONMOKQJTJWKYLZN[Q[TZWXYUZRZOXMVMTORSPXMb",
	"I[KMMMOOU`WbYb ZMYOWRM]K`Jb",
	"F]VFNb GQHOJMLMMNMPLULXMZO[Q[TZVXXUZP[M",
	"F]NMLNJQITIWJZK[M[OZQW RSQWRZS[U[WZYWZTZQYNXM",
	"L\\UUTSRRPRNSMTLVLXMZO[Q[SZTXVRUWUZV[W[YZZY\\V",
	"M[MVOSRNSLTITGSFQGPIOMNTNZO[P[RZTXUUURVVWWYW[V",
	"MXTTTSSRQROSNTMVMXNZP[S[VYXV",
	"L\\UUTSRRPRNSMTLVLXMZO[Q[SZTXZF VRUWUZV[W[YZZY\\V",
	"NXOYQXRWSUSSRRQROSNUNXOZQ[S[UZVYXV",
	"OWOVSQUNVLWIWGVFTGSIQQNZKaJdJfKgMfNcOZP[R[TZUYWV",
	"L[UUTSRRPRNSMTLVLXMZO[Q[SZTY VRTYPdOfMgLfLdMaP^S\\U[XY[V",
	"M\\MVOSRNSLTITGSFQGPIOMNSM[ M[NXOVQSSRURVSVUUXUZV[W[YZZY\\V",
	"PWSMSNTNTMSM PVRRPXPZQ[R[TZUYWV",
	"PWSMSNTNTMSM PVRRLdKfIgHfHdIaL^O\\Q[TYWV",
	"M[MVOSRNSLTITGSFQGPIOMNSM[ M[NXOVQSSRURVSVUTVQV QVSWTZU[V[XZYY[V",
	"OWOVQSTNULVIVGUFSGRIQMPTPZQ[R[TZUYWV",
	"E^EVGSIRJSJTIXH[ IXJVLSNRPRQSQTPXO[ PXQVSSURWRXSXUWXWZX[Y[[Z\\Y^V",
	"J\\JVLSNROSOTNXM[ NXOVQSSRURVSVUUXUZV[W[YZZY\\V",
	"LZRRPRNSMTLVLXMZO[Q[SZTYUWUUTSRRQSQURWTXWXYWZV",
	"KZKVMSNQMUGg MUNSPRRRTSUUUWTYSZQ[ MZO[R[UZWYZV",
	"L[UUTSRRPRNSMTLVLXMZO[Q[SZ VRUUSZPaOdOfPgRfScS\\U[XY[V",
	"MZMVOSPQPSSSTTTVSYSZT[U[WZXYZV",
	"NYNVPSQQQSSVTXTZR[ NZP[T[VZWYYV",
	"OXOVQSSO VFPXPZQ[S[UZVYXV PNWN",
	"L[LVNRLXLZM[O[QZSXUU VRTXTZU[V[XZYY[V",
	"L[LVNRMWMZN[O[RZTXUUUR URVVWWYW[V",
	"I^LRJTIWIYJ[L[NZPX RRPXPZQ[S[UZWXXUXR XRYVZW\\W^V",
	"JZJVLSNRPRQSQZR[U[XYZV WSVRTRSSOZN[L[KZ",
	"L[LVNRLXLZM[O[QZSXUU VRPdOfMgLfLdMaP^S\\U[XY[V",
	"LZLVNSPRRRTTTVSXQZN[P\\Q^QaPdOfMgLfLdMaP^S\\WYZV",
	"J\\K[NZQXSVUSWOXKXIWGUFSGRHQJPOPTQXRZT[V[XZYY",
	"",
	"",
	"",
	"",
	"",
	"I[WUWRVOUNSMQMONMPLSLVMYNZP[R[TZVXWUXPXKWHVGTFRFPGNI",
	"JZWNUMRMPNNPMSMVNYOZQ[T[VZ MTUT",
	"J[TFRGPJOLNOMTMXNZO[Q[SZUWVUWRXMXIWGVFTF NPWP",
	"H\\VFNb QMNNLPKSKVLXNZQ[S[VZXXYUYRXPVNSMQM",
	"I[XOWNTMQMNNMOLQLSMUOWSZT\\T^S_Q_",
	"",
	"",
	"DaWNVLTKQKOLNMMOMRNTOUQVTVVUWS WKWSXUYV[V\\U]S]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYY",
	"F^ZIJRZ[",
	"F^JIZRJ[",
	"KYOBOb OBVB ObVb",
	"KYUBUb NBUB NbUb",
	"KYTBQEPHPJQMSOSPORSTSUQWPZP\\Q_Tb",
	"KYPBSETHTJSMQOQPURQTQUSWTZT\\S_Pb",
	"F^[FYGVHSHPGNFLFJGIIIKKMMMOLPJPHNF [FI[ YTWTUUTWTYV[X[ZZ[X[VYT",
	"NV",
	"JZ",
	"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF",
	"H\\NJPISFS[",
	"H\\LKLJMHNGPFTFVGWHXJXLWNUQK[Y[",
	"H\\MFXFRNUNWOXPYSYUXXVZS[P[MZLYKW",
	"H\\UFKTZT UFU[",
	"H\\WFMFLOMNPMSMVNXPYSYUXXVZS[P[MZLYKW",
	"H\\XIWGTFRFOGMJLOLTMXOZR[S[VZXXYUYTXQVOSNRNOOMQLT",
	"H\\YFO[ KFYF",
	"H\\PFMGLILKMMONSOVPXRYTYWXYWZT[P[MZLYKWKTLRNPQOUNWMXKXIWGTFPF",
	"H\\XMWPURRSQSNRLPKMKLLINGQFRFUGWIXMXRWWUZR[P[MZLX",
	"MWRYQZR[SZRY",
	"MWSZR[QZRYSZS\\R^Q_",
	"MWRMQNROSNRM RYQZR[SZRY",
	"MWRMQNROSNRM SZR[QZRYSZS\\R^Q_",
	"MWRFRT RYQZR[SZRY",
	"I[LKLJMHNGPFTFVGWHXJXLWNVORQRT RYQZR[SZRY",
	"NVRFRM",
	"JZNFNM VFVM",
	"KYQFOGNINKOMQNSNUMVKVIUGSFQF",
	"H\\PBP_ TBT_ YIWGTFPFMGKIKKLMMNOOUQWRXSYUYXWZT[P[MZKX",
	"G][BIb",
	"KYVBTDRGPKOPOTPYR]T`Vb",
	"KYNBPDRGTKUPUTTYR]P`Nb",
	"NVRBRb",
	"E_IR[R",
	"E_RIR[ IR[R",
	"E_IO[O IU[U",
	"G]KKYY YKKY",
	"JZRLRX MOWU WOMU",
	"MWRQQRRSSRRQ",
	"MWSFRGQIQKRLSKRJ",
	"MWRHQGRFSGSIRKQL",
	"E_UMXP[RXTUW IR[R",
	"G]OFOb UFUb JQZQ JWZW",
	"E_\\O\\N[MZMYNXPVUTXRZP[L[JZIYHWHUISJRQNRMSKSIRGPFNGMIMKNNPQUXWZY[[[\\Z\\Y",
	"G]IIJKKOKUJYI[ [IZKYOYUZY[[ IIKJOKUKYJ[I I[KZOYUYYZ[[",
	"F_\\Q[OYNWNUOTPQTPUNVLVJUISIQJOLNNNPOQPTTUUWVYV[U\\S\\Q",
	"KYOBO[ UBU[",
	"F^RBR[ I[[[",
	"F^[BI[[[",
	"E_RIQJRKSJRI IYHZI[JZIY [YZZ[[\\Z[Y",
	"F^RHNLKPJSJUKWMXOXQWRU RHVLYPZSZUYWWXUXSWRU RUQYP\\ RUSYT\\ P\\T\\",
	"F^RNQKPINHMHKIJKJOKRLTNWR\\ RNSKTIVHWHYIZKZOYRXTVWR\\",
	"F^RGPJLOIR RGTJXO[R IRLUPZR] [RXUTZR]",
	"F^RTTWVXXXZW[U[SZQXPVPSQ SQUOVMVKUISHQHOINKNMOOQQ QQNPLPJQISIUJWLXNXPWRT RTQYP\\ RTSYT\\ P\\T\\",
	"F^RRR[Q\\ RVQ\\ RIQHOHNINKONRR RISHUHVIVKUNRR RRNOLNJNIOIQJR RRVOXNZN[O[QZR RRNULVJVIUISJR RRVUXVZV[U[SZR",
	"F^ISJSLTMVMXLZ ISIRJQLQMRNTNWMYLZ RGPIOLOOQUQXPZR\\ RGTIULUOSUSXTZR\\ [S[RZQXQWRVTVWWYXZ [SZSXTWVWXXZ KVYV",
	"",
	"",
	"",
	"PSSRRSQSPRPQQPRPSQSSRUQV QQQRRRRQQQ",
	"PTQPPQPSQTSTTSTQSPQP RQQRRSSRRQ",
	"NVPOTU TOPU NRVR",
	"MWRKQMOPMR RKSMUPWR RMOQ RMUQ ROPQ ROTQ QQSQ MRWR",
	"MWMRMQNOONQMSMUNVOWQWR PNTN OOUO NPVP NQVQ MRWR",
	"LRLFLRRRLF LIPQ LLOR LOMQ",
	"MWRKQMOPMR RKSMUPWR",
	"MWWRWQVOUNSMQMONNOMQMR",
	"G]]R]P\\MZJWHTGPGMHJJHMGPGR",
	"MWMRMSNUOVQWSWUVVUWSWR",
	"LXLPNRQSSSVRXP",
	"RURUTTURTPRO",
	"RVRRUPVNVLUKTK",
	"NRRROPNNNLOKPK",
	"MWWHVGTFQFOGNHMJMLNNOOUSVTWVWXVZU[S\\P\\N[MZ",
	"G]IWHVGTGQHOINKMMMONPOTUUVWWYW[V\\U]S]P\\N[M",
	"G]RRTUUVWWYW[V\\U]S]Q\\O[NYMWMUNTOPUOVMWKWIVHUGSGQHOINKMMMONPORR",
	"H\\KFK[ HF[FQP[Z ZV[Y\\[ ZVZY WYZY WYZZ\\[",
	"KYUARBPCNELHKLKRLUNWQXSXVWXUYR KPLMNKQJSJVKXMYPYVXZV]T_R`Oa",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	">f>RfR",
	"D`D``D",
	"RRR>Rf",
	"D`DD``",
	"D`DR`R",
	"F^FY^K",
	"KYK^YF",
	"",
	"KYKFY^",
	"F^FK^Y",
	"KYKRYR",
	"MWMWWM",
	"",
	"MWMMWW",
	"",
	"",
	"",
	"",
	"D`DOGQKSPTTTYS]Q`O",
	"PUUDSGQKPPPTQYS]U`",
	"OTODQGSKTPTTSYQ]O`",
	"D`DUGSKQPPTPYQ]S`U",
	"KYRJYNKVRZ",
	"JZJRNKVYZR",
	"KYKVKNYVYN",
	"JZLXJPZTXL",
	"JZJ]L]O\\Q[TXUVVSVOULTJSIQIPJOLNONSOVPXS[U\\X]Z]",
	"I]]Z]X\\U[SXPVOSNONLOJPIQISJTLUOVSVVUXT[Q\\O]L]J",
	"JZZGXGUHSIPLONNQNUOXPZQ[S[TZUXVUVQUNTLQIOHLGJG",
	"G[GJGLHOIQLTNUQVUVXUZT[S[QZPXOUNQNNOLPISHUGXGZ",
	"E[EPFRHTJUMVQVUUXSZP[NZLWLSMQNNPLSKVKYL\\M^",
	"EYETHVKWPWSVVTXQYNYLXKVKSLPNNQMTMYN\\P_",
	"OUQOOQOSQUSUUSUQSOQO QPPQPSQTSTTSTQSPQP RQQRRSSRRQ",
	"",
	"D`DRJR ORUR ZR`R",
	"D`DUDO`O`U",
	"JZRDJR RDZR",
	"D`DR`R JYZY P`T`",
	"D`DR`R DRRb `RRb",
	"",
	"",
	"",
	"",
	"",
	"KYQKNLLNKQKSLVNXQYSYVXXVYSYQXNVLSKQK",
	"LXLLLXXXXLLL",
	"KYRJKVYVRJ",
	"LXRHLRR\\XRRH",
	"JZRIPOJOOSMYRUWYUSZOTORI",
	"KYRKRY KRYR",
	"MWMMWW WMMW",
	"MWRLRX MOWU WOMU",
	"",
	"",
	"NVQNOONQNSOUQVSVUUVSVQUOSNQN OQOS PPPT QOQU RORU SOSU TPTT UQUS",
	"NVNNNVVVVNNN OOOU POPU QOQU RORU SOSU TOTU UOUU",
	"MWRLMUWURL ROOT ROUT RRQT RRST",
	"LULRUWUMLR ORTU ORTO RRTS RRTQ",
	"MWRXWOMORX RUUP RUOP RRSP RRQP",
	"OXXROMOWXR URPO URPU RRPQ RRPS",
	"LXRLNWXPLPVWRL RRRL RRLP RRNW RRVW RRXP",
	"",
	"",
	"",
	"MWRLRX OOUO MUOWQXSXUWWU",
	"LXRLRX LQMOWOXQ PWTW",
	"KYMNWX WNMX OLLOKQ ULXOYQ",
	"I[NII[ VI[[ MM[[ WMI[ NIVI MMWM",
	"I[RGRV MJWP WJMP IVL\\ [VX\\ IV[V L\\X\\",
	"G[MJSV KPSL G\\[\\[RG\\",
	"LXPLPPLPLTPTPXTXTTXTXPTPTLPL",
	"KYYPXNVLSKQKNLLNKQKSLVNXQYSYVXXVYT YPWNUMSMQNPOOQOSPUQVSWUWWVYT",
	"KYRJKVYVRJ RZYNKNRZ",
	"G]PIPGQFSFTGTI GZHXJVKTLPLKMJOIUIWJXKXPYTZV\\X]Z GZ]Z QZP[Q\\S\\T[SZ",
	"JZRMRS RSQ\\ RSS\\ Q\\S\\ RMQJPHNG QJNG RMSJTHVG SJVG RMNKLKJM PLLLJM RMVKXKZM TLXLZM RMPNOOOR RMPOOR RMTNUOUR RMTOUR",
	"JZRIRK RNRP RSRU RYQ\\ RYS\\ Q\\S\\ RGQIPJ RGSITJ PJRITJ RKPNNOMN RKTNVOWN NOPORNTOVO RPPSNTLTKRKSLT RPTSVTXTYRYSXT NTPTRSTTVT RUPXOYMZLZKYJWJYLZ RUTXUYWZXZYYZWZYXZ MZOZRYUZWZ",
	"JZRYQ\\ RYS\\ Q\\S\\ RYUZXZZXZUYTWTYRZOYMWLUMVJUHSGQGOHNJOMMLKMJOKRMTKTJUJXLZOZRY",
	"JZRYQ\\ RYS\\ Q\\S\\ RYVXVVXUXRZQZLYIXHVHTGPGNHLHKIJLJQLRLUNVNXRY",
	"I[IPKR LKNP RGRO XKVP [PYR",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"QSRQQRRSSRRQ",
	"PTQPPQPSQTSTTSTQSPQP",
	"NVQNOONQNSOUQVSVUUVSVQUOSNQN",
	"MWQMONNOMQMSNUOVQWSWUVVUWSWQVOUNSMQM",
	"KYQKNLLNKQKSLVNXQYSYVXXVYSYQXNVLSKQK",
	"G]PGMHJJHMGPGTHWJZM\\P]T]W\\ZZ\\W]T]P\\MZJWHTGPG",
	"AcPALBJCGEEGCJBLAPATBXCZE]G_JaLbPcTcXbZa]__]aZbXcTcPbLaJ_G]EZCXBTAPA",
	"<hP<K=G?DAAD?G=K<P<T=Y?]A`DcGeKgPhThYg]e`cc`e]gYhThPgKeGcD`A]?Y=T<P<",
	"){O)I*E+@-;073370;-@+E*I)O)U*[+_-d0i3m7q;t@wEyIzO{U{[z_ydwitmqqmtiwdy_z[{U{OzIyEw@t;q7m3i0d-_+[*U)O)",
	">fRAPCMDJDGCEA>H@JAMAZB]D_G`M`PaRc RATCWDZD]C_AfHdJcMcZb]`_]`W`TaRc",
	"AcRAPCMDJDGCEABGAKAPBTDXG\\L`Rc RATCWDZD]C_AbGcKcPbT`X]\\X`Rc BHbH",
	"H[WPVQWRXQXPVNTMQMNNLPKSKULXNZQ[S[VZXX QMONMPLSLUMXOZQ[ LbXF",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"KYRKMX RNVX RKWX OTTT KXPX TXYX",
	"JZNKNX OKOX LKSKVLWNVPSQ SKULVNUPSQ OQSQVRWTWUVWSXLX SQURVTVUUWSX",
	"KYVLWKWOVLTKQKOLNMMPMSNVOWQXTXVWWU QKOMNPNSOVQX",
	"JZNKNX OKOX LKSKVLWMXPXSWVVWSXLX SKULVMWPWSVVUWSX",
	"JYNKNX OKOX SOSS LKVKVOUK OQSQ LXVXVTUX",
	"JXNKNX OKOX SOSS LKVKVOUK OQSQ LXQX",
	"K[VLWKWOVLTKQKOLNMMPMSNVOWQXTXVW QKOMNPNSOVQX TXUWVU VSVX WSWX TSYS",
	"J[NKNX OKOX VKVX WKWX LKQK TKYK OQVQ LXQX TXYX",
	"NWRKRX SKSX PKUK PXUX",
	"LXSKSURWQX TKTUSWQXPXNWMUNTOUNV QKVK",
	"JZNKNX OKOX WKOS QQVX RQWX LKQK TKYK LXQX TXYX",
	"KXOKOX PKPX MKRK MXWXWTVX",
	"I\\MKMX NNRX NKRU WKRX WKWX XKXX KKNK WKZK KXOX UXZX",
	"JZNKNX OMVX OKVV VKVX LKOK TKXK LXPX",
	"KZQKOLNMMPMSNVOWQXTXVWWVXSXPWMVLTKQK QKOMNPNSOVQX TXVVWSWPVMTK",
	"JYNKNX OKOX LKSKVLWNWOVQSROR SKULVNVOUQSR LXQX",
	"KZQKOLNMMPMSNVOWQXTXVWWVXSXPWMVLTKQK QKOMNPNSOVQX TXVVWSWPVMTK PWPUQTSTTUUZV[W[XZ TUUXVZW[",
	"JZNKNX OKOX LKSKVLWNWOVQSROR SKULVNVOUQSR LXQX SRTSUWVXWXXW SRUSVWWX",
	"KZVMWKWOVMULSKQKOLNMNOOPQQTRVSWT NNOOQPTQVRWSWVVWTXRXPWOVNTNXOV",
	"KZRKRX SKSX NKMOMKXKXOWK PXUX",
	"J[NKNUOWQXTXVWWUWK OKOUPWQX LKQK UKYK",
	"KYMKRX NKRU WKRX KKPK TKYK",
	"I[LKOX MKOT RKOX RKUX SKUT XKUX JKOK VKZK",
	"KZNKVX OKWX WKNX LKQK TKYK LXQX TXYX",
	"LYNKRRRX OKSR WKSRSX LKQK TKYK PXUX",
	"LYVKNX WKOX OKNONKWK NXWXWTVX",
	"KYRKMX RNVX RKWX OTTT KXPX TXYX",
	"JZNKNX OKOX LKSKVLWNVPSQ SKULVNUPSQ OQSQVRWTWUVWSXLX SQURVTVUUWSX",
	"KXOKOX PKPX MKWKWOVK MXRX",
	"KYRKLX RMWX RKXX MWVW LXXX",
	"JYNKNX OKOX SOSS LKVKVOUK OQSQ LXVXVTUX",
	"LYVKNX WKOX OKNONKWK NXWXWTVX",
	"J[NKNX OKOX VKVX WKWX LKQK TKYK OQVQ LXQX TXYX",
	"KZQKOLNMMPMSNVOWQXTXVWWVXSXPWMVLTKQK QKOMNPNSOVQX TXVVWSWPVMTK QOQT TOTT QQTQ QRTR",
	"NWRKRX SKSX PKUK PXUX",
	"JZNKNX OKOX WKOS QQVX RQWX LKQK TKYK LXQX TXYX",
	"KYRKMX RNVX RKWX KXPX TXYX",
	"I\\MKMX NNRX NKRU WKRX WKWX XKXX KKNK WKZK KXOX UXZX",
	"JZNKNX OMVX OKVV VKVX LKOK TKXK LXPX",
	"JZMJLM XJWM PPOS UPTS MVLY XVWY MKWK MLWL PQTQ PRTR MWWW MXWX",
	"KZQKOLNMMPMSNVOWQXTXVWWVXSXPWMVLTKQK QKOMNPNSOVQX TXVVWSWPVMTK",
	"J[NKNX OKOX VKVX WKWX LKYK LXQX TXYX",
	"JYNKNX OKOX LKSKVLWNWOVQSROR SKULVNVOUQSR LXQX",
	"K[MKRQ NKSQMX MKWKXOVK NWWW MXWXXTVX",
	"KZRKRX SKSX NKMOMKXKXOWK PXUX",
	"KZMONLOKPKQLRORX XOWLVKUKTLSOSX MONMOLPLQMRO XOWMVLULTMSO PXUX",
	"KZRKRX SKSX QNNOMQMRNTQUTUWTXRXQWOTNQN QNOONQNROTQU TUVTWRWQVOTN PKUK PXUX",
	"KZNKVX OKWX WKNX LKQK TKYK LXQX TXYX",
	"J[RKRX SKSX LPMONOOSQU TUVSWOXOYP MONROTQUTUVTWRXO PKUK PXUX",
	"KZMVNXQXMRMONMOLQKTKVLWMXOXRTXWXXV OUNRNOOMQK TKVMWOWRVU NWPW UWWW",
	"KYTKKX SMTX TKUX NTTT IXNX RXWX",
	"JYPKLX QKMX NKUKWLWNVPSQ UKVLVNUPSQ OQRQTRUSUUTWQXJX RQTSTUSWQX",
	"KXVLWLXKWNVLTKRKPLOMNOMRMUNWPXRXTWUU RKPMOONRNVPX",
	"JYPKLX QKMX NKTKVLWNWQVTUVTWQXJX TKULVNVQUTTVSWQX",
	"JYPKLX QKMX SORS NKXKWNWK OQRQ JXTXUUSX",
	"JXPKLX QKMX SORS NKXKWNWK OQRQ JXOX",
	"KYVLWLXKWNVLTKRKPLOMNOMRMUNWPXRXTWUVVS RKPMOONRNVPX RXTVUS SSXS",
	"J[PKLX QKMX XKTX YKUX NKSK VK[K OQVQ JXOX RXWX",
	"NWTKPX UKQX RKWK NXSX",
	"LXUKRUQWPX VKSURWPXOXMWLUMTNUMV SKXK",
	"JZPKLX QKMX YKOR RPTX SPUX NKSK VK[K JXOX RXWX",
	"KXQKMX RKNX OKTK KXUXVUTX",
	"I\\OKKX OMPX PKQV YKPX YKUX ZKVX MKPK YK\\K IXMX SXXX",
	"JZPKLX PKTX QKTU XKTX NKQK VKZK JXNX",
	"KYRKPLOMNOMRMUNWPXRXTWUVVTWQWNVLTKRK RKPMOONRNVPX RXTVUTVQVMTK",
	"JYPKLX QKMX NKUKWLXMXOWQTROR UKWMWOVQTR JXOX",
	"KYRKPLOMNOMRMUNWPXRXTWUVVTWQWNVLTKRK RKPMOONRNVPX RXTVUTVQVMTK OWOVPUQURVRZS[T[UZ RVSZT[",
	"JZPKLX QKMX NKUKWLXMXOWQTROR UKWMWOVQTR SRTWUXVXWW SRTSUWVX JXOX",
	"KZWLXLYKXNWLUKRKPLOMOOPPUSVT ONPOURVSVVUWSXPXNWMULXMWNW",
	"KZTKPX UKQX PKNNOKZKYNYK NXSX",
	"J[PKMUMWOXSXUWVUYK QKNUNWOX NKSK WK[K",
	"KYOKPX PKQV YKPX MKRK VK[K",
	"I[NKMX OKNV TKMX TKSX UKTV ZKSX LKQK XK\\K",
	"KZPKTX QKUX YKLX NKSK VK[K JXOX RXWX",
	"LYPKRQPX QKSQ YKSQQX NKSK VK[K NXSX",
	"LYXKLX YKMX QKONPKYK LXUXVUTX",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"KZMHX\\",
	"JZRMLW RMXW",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"LZQOPPPQOQOPQOTOVQVWWXXX TOUQUWWX URRSPTOUOWPXSXTWUU RSPUPWQX",
	"JYNKNX OKOX ORPPROTOVPWRWUVWTXRXPWOU TOUPVRVUUWTX LKOK",
	"LXVQUQURVRVQUPSOQOOPNRNUOWQXSXUWVV QOPPOROUPWQX",
	"L[VKVX WKWX VRUPSOQOOPNRNUOWQXSXUWVU QOPPOROUPWQX TKWK VXYX",
	"LXOSVSVRUPSOQOOPNRNUOWQXSXUWVV USUQSO QOPPOROUPWQX",
	"LWTKULUMVMVLTKRKPMPX RKQMQX NOSO NXSX",
	"LYQOOQOSQUSUUSUQSOQO QOPQPSQU SUTSTQSO TPUOVO PTOUOXPYTYVZ OWPXTXVYV[T\\P\\N[NYPX",
	"J[NKNX OKOX ORPPROTOVPWRWX TOUPVRVX LKOK LXQX TXYX",
	"NWRKRLSLSKRK RORX SOSX POSO PXUX",
	"NWSKSLTLTKSK SOSZR\\ TOTZR\\P\\O[OZPZP[O[ QOTO",
	"JZNKNX OKOX WOOU RSVX SSWX LKOK TOYO LXQX TXYX",
	"NWRKRX SKSX PKSK PXUX",
	"F_JOJX KOKX KRLPNOPORPSRSX POQPRRRX SRTPVOXOZP[R[X XOYPZRZX HOKO HXMX PXUX XX]X",
	"J[NONX OOOX ORPPROTOVPWRWX TOUPVRVX LOOO LXQX TXYX",
	"LYQOOPNRNUOWQXTXVWWUWRVPTOQO QOPPOROUPWQX TXUWVUVRUPTO",
	"JYNON\\ OOO\\ ORPPROTOVPWRWUVWTXRXPWOU TOUPVRVUUWTX LOOO L\\Q\\",
	"KYUOU\\ VOV\\ URTPROPONPMRMUNWPXRXTWUU POOPNRNUOWPX S\\X\\",
	"KXOOOX POPX PRQPSOUOVPVQUQUPVP MOPO MXRX",
	"LYTOUPUQVQVPTOQOOPORQSTTVU OQQRTSVTVWTXQXOWOVPVPWQX",
	"LWPKPVRXTXUWUV QKQVRX NOTO",
	"J[NONUOWQXSXUWVU OOOUPWQX VOVX WOWX LOOO TOWO VXYX",
	"KYNORX OORV VORX LOQO TOXO",
	"I[LOOX MOOU ROOX ROUX SOUU XOUX JOOO VOZO",
	"KYNOUX OOVX VONX LOQO TOXO LXPX SXXX",
	"KYNORX OORV VORXP[N\\M\\L[LZMZM[L[ LOQO TOXO",
	"LXUONX VOOX OONQNOVO NXVXVVUX",
	"K[QOOPNQMSMUNWPXQXSWUUWRXO QOOQNSNUOWPX QOSOUPWWXX SOTPVWXXYX",
	"KXRKPMOOMUK\\ QLPNNTL\\ RKTKVLVNUPRQ TKULUNTPRQ RQTRUTUVTWRXQXOWNT RQSRTTTVRX",
	"KYLQNOPORPSSSXR\\ LQNPPPRQSS WOVRSXQ\\",
	"KYSOQOOPNQMSMUNWPXRXTWUVVTVRUPRNQLQKRJTJUKVM QOOQNSNVPX RXTVUTUQSO QLRKTKVM",
	"LXVPTOQOOPOQPRRS QOPPPQRS RSOTNUNWPXSXUW RSPTOUOWPX",
	"LWRKQLQMSNVNVMSNPOOPNRNTOVPWRXSYS[R\\P\\O[ SNQOPPOROTPVRX",
	"IYJRKPLONOOPOQMX MONPNQLX OQPPROTOVPVRS\\ TOUPURR\\",
	"IYJSKQLPNPOQOVPX MPNQNUOWPXQXSWTVUTVQVNULTKRKQLQNRPURWS QXSVTTUQUNTK",
	"NWROPVPWQXSXUWVU SOQVQWRX",
	"KYOOLX POMX UOVPWPVOTORQOR ORPSRWTXVWWU ORQSSWTX",
	"LXLKNKPLWX NKOLVX RPMX RPNX",
	"KZOOK\\ POL\\ NUNWOXQXSWTV VOTVTWUXWXXWYU WOUVUWVX",
	"JYNOMX OONUMX VRVOWOVRTUQWNXMX LOOO",
	"MXRKQLQMSNVN TNQOPPPRRSUS TNROQPQRRS SSPTOUOWQXSYTZT[S\\Q\\ SSQTPUPWQX",
	"KXQOOPNQMSMUNWPXRXTWUVVTVRUPSOQO QOOQNSNVPX RXTVUTUQSO",
	"IZPPMX PPNX TPSX TPTX KQMOXO KQMPXP",
	"JXSOQOOPNQMSJ\\ QOOQNSK\\ SOUPVRVTUVTWRXPXNWMU SOUQUTTVRX",
	"K[YOQOOPNQMSMUNWPXRXTWUVVTVRUPYP QOOQNSNVPX RXTVUTUQSO",
	"KZSPQX SPRX MQOOXO MQOPXP",
	"JXKRLPMOOOPPPROUOWPX NOOPORNUNWPXQXSWUUVRVOUOVP",
	"KZOPNQMSMUNWPXRXUWWUXRXPWOUOTPSRRUO\\ MUNVPWRWUVWTXR XQWPUPSR RUQXP\\",
	"KXMONOPPS[T\\ NOOPR[T\\U\\ VOTRNYL\\",
	"I[TKQ\\ UKP\\ JRKPLONOOPOVPWSWUVWT MONPNTOWPXSXUWWTXRYO",
	"JZNPPPPONPMQLSLUMWNXPXQWRUSR LUNWPWRU RRRWSXUXWVXTXRWPVOVPWP RUSWUWWV",
	"KZVOTVTWUXWXXWYU WOUVUWVX USUQSOQOOPNQMSMUNWPXRXTV QOOQNSNVPX",
	"JXOKMR PKNRNVPX NROPQOSOUPVRVTUVTWRXPXNWMUMR SOUQUTTVRX MKPK",
	"KXUPUQVQUPSOQOOPNQMSMUNWPXRXTWUV QOOQNSNVPX",
	"KZWKTVTWUXWXXWYU XKUVUWVX USUQSOQOOPNQMSMUNWPXRXTV QOOQNSNVPX UKXK",
	"KWNURTTSURUPSOQOOPNQMSMUNWPXRXTWUV QOOQNSNVPX",
	"MXWKXLXKVKTLSNPYO[N\\ VKULTNQYP[N\\L\\L[M\\ POVO",
	"KYVOTVSYR[ WOUVTYR[P\\M\\L[M[N\\ USUQSOQOOPNQMSMUNWPXRXTV QOOQNSNVPX",
	"KZPKLX QKMX OQPPROTOVPVRUUUWVX TOUPURTUTWUXWXXWYU NKQK",
	"MWSKSLTLTKSK NROPPOROSPSRRURWSX QORPRRQUQWRXTXUWVU",
	"MWTKTLULUKTK ORPPQOSOTPTRRYQ[O\\M\\M[N\\ ROSPSRQYP[O\\",
	"KXPKLX QKMX VPUQVQVPUOTORQPROR ORPSQWRXTXUWVU ORQSRWSX NKQK",
	"NVSKPVPWQXSXTWUU TKQVQWRX QKTK",
	"F^GRHPIOKOLPLQJX JOKPKQIX LQMPOOQOSPSQQX QORPRQPX SQTPVOXOZPZRYUYWZX XOYPYRXUXWYX[X\\W]U",
	"J[KRLPMOOOPPPQNX NOOPOQMX PQQPSOUOWPWRVUVWWX UOVPVRUUUWVXXXYWZU",
	"KXQOOPNQMSMUNWPXRXTWUVVTVRUPSOQO QOOQNSNVPX RXTVUTUQSO",
	"JYKRLPMOOOPPPQM\\ NOOPOQL\\ PQROTOVPWRWTVVUWSXQXOVOT TOVQVTUVSX J\\O\\",
	"KYVOR\\ WOS\\ USUQSOQOOPNQMSMUNWPXRXTV QOOQNSNVPX P\\U\\",
	"LXMRNPOOQORPRQPX POQPQQOX RQSPUOVOWPWQVQWP",
	"LYVPVQWQVPTOQOOPORQSTTVU OQQRTSVTVWTXQXOWNVOVOW",
	"NWSKPVPWQXSXTWUU TKQVQWRX POUO",
	"IZJRKPLONOOPORNUNWOX MONPNRMUMWOXQXSWTV VOTVTWUXWXXWYU WOUVUWVX",
	"JXKRLPMOOOPPPROUOWPX NOOPORNUNWPXQXSWUUVRVOUOVP",
	"H\\IRJPKOMONPNRMUMWNX LOMPMRLULWNXOXQWRV TORVRWTX UOSVSWTXUXWWYUZRZOYOZP",
	"JZMRNPPOROSPSR QORPRRQUPWNXMXLWLVMVLW XPWQXQXPWOVOTPSRRURWSX QUQWRXTXVWWU",
	"IYJRKPLONOOPORNUNWOX MONPNRMUMWOXQXSWTV VOTVSYR[ WOUVTYR[P\\M\\L[M[N\\",
	"KYWOWPVQNVMWMX NQOOROUQ OPRPUQVQ NVOVRWUW OVRXUXVV",
	"H[RKSLSMTMTLRKOKMLLNLX OKNLMNMX XKYLYMZMZLXKVKTMTX VKUMUX JOWO JXOX RXWX",
	"J[UKVLWLWKQKOLNNNX QKPLONOX VOVX WOWX LOWO LXQX TXYX",
	"J[WKQKOLNNNX QKPLONOX UKVLVX WKWX LOVO LXQX TXYX",
	"F_PKQLQMRMRLPKMKKLJNJX MKLLKNKX YKZL[L[KUKSLRNRX UKTLSNSX ZOZX [O[X HO[O HXMX PXUX XX]X",
	"F_PKQLQMRMRLPKMKKLJNJX MKLLKNKX [KUKSLRNRX UKTLSNSX YKZLZX [K[X HOZO HXMX PXUX XX]X",
	"NWRORX SOSX POSO PXUX",
	"",
	"LXVPTOROPPOQNSNUOWQXSXUW ROPQOSOVQX OSSS",
	"LYSKQLPMOONRNUOWPXRXTWUVVTWQWNVLUKSK SKQMPOOSOVPX RXTVUTVPVMUK OQVQ",
	"KZTKQ\\ UKP\\ QONPMRMUNWQXTXWWXUXRWPTOQO QOOPNRNUOWQX TXVWWUWRVPTO",
	"LXUPVRVQUPSOQOOPNRNTOVRX QOOQOTPVRXSYS[R\\P\\",
	"",
	"",
	"",
	"I[VKWLXLVKSKQLPMOOLYK[J\\ SKQMPOMYL[J\\H\\H[I\\ ZK[L[KYKWLVNSYR[Q\\ YKXLWNTYS[Q\\O\\O[P\\ LOYO",
	"IZVKWLXLXKSKQLPMOOLYK[J\\ SKQMPOMYL[J\\H\\H[I\\ VOTVTWUXWXXWYU WOUVUWVX LOWO",
	"IZVKWL XKSKQLPMOOLYK[J\\ SKQMPOMYL[J\\H\\H[I\\ WKTVTWUXWXXWYU XKUVUWVX LOVO",
	"F^SKTLTM ULSKPKNLMMLOIYH[G\\ PKNMMOJYI[G\\E\\E[F\\ ZK[L\\L\\KWKUL TMSOPYO[N\\ WKUMTOQYP[N\\L\\L[M\\ ZOXVXWYX[X\\W]U [OYVYWZX IO[O",
	"F^SKTLTM ULSKPKNLMMLOIYH[G\\ PKNMMOJYI[G\\E\\E[F\\ ZK[L \\KWKUL TMSOPYO[N\\ WKUMTOQYP[N\\L\\L[M\\ [KXVXWYX[X\\W]U \\KYVYWZX IOZO",
	"MWNROPPOROSPSRRURWSX QORPRRQUQWRXTXUWVU",
	"",
	"OU",
	"LX",
	"LYQKOLNONTOWQXTXVWWTWOVLTKQK QKPLOOOTPWQX TXUWVTVOULTK",
	"LYPNSKSX RLRX OXVX",
	"LYOMONNNNMOLQKTKVLWNVPTQQROSNUNX TKULVNUPTQ NWOVPVSWVWWV PVSXVXWVWU",
	"LYOMONNNNMOLQKTKVLWNVPTQ TKULVNUPTQ RQTQVRWTWUVWTXQXOWNVNUOUOV TQURVTVUUWTX",
	"LYSMSX TKTX TKMTXT QXVX",
	"LYOKNQ OKVK OLSLVK NQOPQOTOVPWRWUVWTXQXOWNVNUOUOV TOUPVRVUUWTX",
	"LYVMVNWNWMVLTKRKPLOMNPNUOWQXTXVWWUWSVQTPQPNR RKPMOPOUPWQX TXUWVUVSUQTP",
	"LYNKNO VMRTPX WKTQQX NMPKRKUM NMPLRLUMVM",
	"LYQKOLNNOPQQTQVPWNVLTKQK QKPLONPPQQ TQUPVNULTK QQORNTNUOWQXTXVWWUWTVRTQ QQPROTOUPWQX TXUWVUVTURTQ",
	"LYOVOUNUNVOWQXSXUWVVWSWNVLTKQKOLNNNPORQSTSWQ SXUVVSVNULTK QKPLONOPPRQS",
	"NVRVQWRXSWRV",
	"NVSWRXQWRVSWSYQ[",
	"NVROQPRQSPRO RVQWRXSWRV",
	"NVROQPRQSPRO SWRXQWRVSWSYQ[",
	"NVRKQLRSSLRK RLRO RVQWRXSWRV",
	"LYNNONOONONNOLQKTKVLWNWOVQSRRSRTST TKVMVPUQSR RWRXSXSWRW",
	"OVRKRP SKRP",
	"LXOKOP PKOP UKUP VKUP",
	"MWQKPLPNQOSOTNTLSKQK",
	"MWRJRP OKUO UKOO",
	"KZXHM\\",
	"MWUHSJQMPPPTQWSZU\\ SJRLQPQTRXSZ",
	"MWOHQJSMTPTTSWQZO\\ QJRLSPSTRXQZ",
	"MWPHP\\ QHQ\\ PHUH P\\U\\",
	"MWSHS\\ THT\\ OHTH O\\T\\",
	"LWSHRIQKQMRORPPRRTRUQWQYR[S\\ RIQM QKRO RUQY QWR[",
	"MXQHRISKSMRORPTRRTRUSWSYR[Q\\ RISM SKRO RUSY SWR[",
	"MWTHPRT\\",
	"MWPHTRP\\",
	"OURHR\\",
	"MWPHP\\ THT\\",
	"I[LRXR",
	"I[RLRX LRXR",
	"JZRMRX MRWR MXWX",
	"JZRMRX MMWM MRWR",
	"JZMMWW WMMW",
	"NVRQQRRSSRRQ",
	"I[RLQMRNSMRL LRXR RVQWRXSWRV",
	"I[LPXP LTXT",
	"I[WLMX LPXP LTXT",
	"I[LNXN LRXR LVXV",
	"JZWLMRWX",
	"JZMLWRMX",
	"JZWKMOWS MTWT MXWX",
	"JZMKWOMS MTWT MXWX",
	"H[YUWUUTTSRPQOONNNLOKQKRLTNUOUQTRSTPUOWNYN",
	"JZLTLRMPOPUSWSXR LRMQOQUTWTXRXP",
	"JZMSRPWS MSRQWS",
	"NVSKPO SKTLPO",
	"NVQKTO QKPLTO",
	"LXNKOMQNSNUMVK NKONQOSOUNVK",
	"NVSLRMQLRKSLSNQP",
	"NVSKQMQORPSORNQO",
	"NVQLRMSLRKQLQNSP",
	"NVQKSMSORPQORNSO",
	"",
	"JZWMQMONNOMQMSNUOVQWWW",
	"JZMMMSNUOVQWSWUVVUWSWM",
	"JZMMSMUNVOWQWSVUUVSWMW",
	"JZMWMQNOONQMSMUNVOWQWW",
	"JZWMQMONNOMQMSNUOVQWWW MRUR",
	"I[TOUPXRUTTU UPWRUT LRWR",
	"MWRMRX OPPORLTOUP PORMTO",
	"I[POOPLROTPU OPMROT MRXR",
	"MWRLRW OTPURXTUUT PURWTU",
	"KYVSUPSOQOOPNQMSMUNWPXRXTWUVVTWQWNVLTKQKPLQLRK QOOQNSNVPX RXTVUTVQVNULTK",
	"JZLKRX MKRV XKRX LKXK NLWL",
	"G[IOLORW KORX [FRX",
	"I[XIXJYJYIXHVHTJSLROQUPYO[ UITKSORUQXPZN\\L\\K[KZLZL[",
	"I[XIXJYJYIXHVHTJSLROQUPYO[ UITKSORUQXPZN\\L\\K[KZLZL[ QNOONQNSOUQVSVUUVSVQUOSNQN",
	"H\\ZRYTWUVUTTSSQPPONNMNKOJQJRKTMUNUPTQSSPTOVNWNYOZQZR",
	"JZXKLX OKPLPNOOMOLNLLMKOKSLVLXK UTTUTWUXWXXWXUWTUT",
	"J[YPXPXQYQYPXOWOVPUTTVSWQXOXMWLVLTMSORRPSNSLRKPKOLONPQUWWXXXYW OXMVMTOR ONPPVWWX",
	"J[UPSOQOPQPRQTSTUS UOUSVTXTYRYQXNVLSKRKOLMNLQLRMUOWRXSXVW",
	"KZQHQ\\ THT\\ WLVLVMWMWLUKPKNLNNOPVSWT NNOOVRWTWVVWTXQXOWNVNUOUOVNV",
	"KYPKP[ TKT[ MQWQ MUWU",
	"LXTLSLSMTMTLSKQKPLPNQPTRUS PNQOTQUSUUSW QPOROTPVSXTY OTPUSWTYT[S\\Q\\P[PZQZQ[P[",
	"LXRKQLRMSLRK RMRQ RQQSRVSSRQ RVR\\ POONNOOPPOTOUNVOUPTO",
	"LXRMSLRKQLRMRQQRSURV RQSRQURVRZQ[R\\S[RZ POONNOOPPOTOUNVOUPTO PXOWNXOYPXTXUWVXUYTX",
	"LYVKVX NKVK QQVQ NXVX",
	"",
	"H\\QKNLLNKQKSLVNXQYSYVXXVYSYQXNVLSKQK RQQRRSSRRQ",
	"LYQKPLPMQN TKULUMTN RNPOOQORPTRUSUUTVRVQUOSNRN RURY SUSY OWVW",
	"LYRKPLONOOPQRRSRUQVOVNULSKRK RRRX SRSX OUVU",
	"H\\QKNLLNKQKSLVNXQYSYVXXVYSYQXNVLSKQK RKRY KRYR",
	"JYRRPQOQMRLTLUMWOXPXRWSUSTRR WMRR RMWMWR RMVNWR",
	"JZLLMKOKQLRNRPQRPSNT OKPLQNQQPS VKUX WKTX NTXT",
	"JYNKNU OKNR NROPQOSOUPVQVTTVTXUYVYWX SOUQUTTV LKOK",
	"LYONRKRQ VNSKSQ RQPROTOUPWRXSXUWVUVTURSQ RTRUSUSTRT",
	"JZRKRY MKMPNRPSTSVRWPWK LMMKNM QMRKSM VMWKXM OVUV",
	"JYNKNX OKOX LKSKVLWNWOVQSROR SKULVNVOUQSR LXVXVUUX",
	"LYWKTKQLONNQNSOVQXTYWY WKTLRNQQQSRVTXWY",
	"JZRRPQOQMRLTLUMWOXPXRWSUSTRR SLQQ WMRR XQSS",
	"KYPMTW TMPW MPWT WPMT",
	"J[OUMULVLXMYOYPXPVNTMRMONMOLQKTKVLWMXOXRWTUVUXVYXYYXYVXUVU NMPLULWM",
	"J[OOMOLNLLMKOKPLPNNPMRMUNWOXQYTYVXWWXUXRWPUNULVKXKYLYNXOVO NWPXUXWW",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"F^KHK\\ LHL\\ XHX\\ YHY\\ HH\\H H\\O\\ U\\\\\\",
	"H]KHRQJ\\ JHQQ JHYHZMXH K[X[ J\\Y\\ZWX\\",
	"KYVBTDRGPKOPOTPYR]T`Vb TDRHQKPPPTQYR\\T`",
	"KYNBPDRGTKUPUTTYR]P`Nb PDRHSKTPTTSYR\\P`",
	"KYOBOb PBPb OBVB ObVb",
	"KYTBTb UBUb NBUB NbUb",
	"JYTBQEPHPJQMSOSPORSTSUQWPZP\\Q_Tb RDQGQKRN RVQYQ]R`",
	"KZPBSETHTJSMQOQPURQTQUSWTZT\\S_Pb RDSGSKRN RVSYS]R`",
	"KYU@RCPFOIOLPOSVTYT\\S_Ra RCQEPHPKQNTUUXU[T^RaOd",
	"KYO@RCTFUIULTOQVPYP\\Q_Ra RCSETHTKSNPUOXO[P^RaUd",
	"AXCRGRR` GSRa FSRb X:Rb",
	"F^[CZD[E\\D\\C[BYBWCUETGSJRNPZO^N` VDUFTJRVQZP]O_MaKbIbHaH`I_J`Ia",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"H\\RFK[ RFY[ RIX[ MUVU I[O[ U[[[",
	"G]LFL[ MFM[ IFUFXGYHZJZLYNXOUP UFWGXHYJYLXNWOUP MPUPXQYRZTZWYYXZU[I[ UPWQXRYTYWXYWZU[",
	"G\\XIYLYFXIVGSFQFNGLIKKJNJSKVLXNZQ[S[VZXXYV QFOGMILKKNKSLVMXOZQ[",
	"G]LFL[ MFM[ IFSFVGXIYKZNZSYVXXVZS[I[ SFUGWIXKYNYSXVWXUZS[",
	"G\\LFL[ MFM[ SLST IFYFYLXF MPSP I[Y[YUX[",
	"G[LFL[ MFM[ SLST IFYFYLXF MPSP I[P[",
	"G^XIYLYFXIVGSFQFNGLIKKJNJSKVLXNZQ[S[VZXX QFOGMILKKNKSLVMXOZQ[ XSX[ YSY[ US\\S",
	"F^KFK[ LFL[ XFX[ YFY[ HFOF UF\\F LPXP H[O[ U[\\[",
	"MXRFR[ SFS[ OFVF O[V[",
	"KZUFUWTZR[P[NZMXMVNUOVNW TFTWSZR[ QFXF",
	"F\\KFK[ LFL[ YFLS QOY[ POX[ HFOF UF[F H[O[ U[[[",
	"I[NFN[ OFO[ KFRF K[Z[ZUY[",
	"F_KFK[ LFRX KFR[ YFR[ YFY[ ZFZ[ HFLF YF]F H[N[ V[][",
	"G^LFL[ MFYY MHY[ YFY[ IFMF VF\\F I[O[",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF QFOGMILKKOKRLVMXOZQ[ S[UZWXXVYRYOXKWIUGSF",
	"G]LFL[ MFM[ IFUFXGYHZJZMYOXPUQMQ UFWGXHYJYMXOWPUQ I[P[",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF QFOGMILKKOKRLVMXOZQ[ S[UZWXXVYRYOXKWIUGSF NYNXOVQURUTVUXV_W`Y`Z^Z] UXV\\W^X_Y_Z^",
	"G]LFL[ MFM[ IFUFXGYHZJZLYNXOUPMP UFWGXHYJYLXNWOUP I[P[ RPTQURXYYZZZ[Y TQUSWZX[Z[[Y[X",
	"H\\XIYFYLXIVGSFPFMGKIKKLMMNOOUQWRYT KKMMONUPWQXRYTYXWZT[Q[NZLXKUK[LX",
	"I\\RFR[ SFS[ LFKLKFZFZLYF O[V[",
	"F^KFKULXNZQ[S[VZXXYUYF LFLUMXOZQ[ HFOF VF\\F",
	"H\\KFR[ LFRX YFR[ IFOF UF[F",
	"F^JFN[ KFNV RFN[ RFV[ SFVV ZFV[ GFNF WF]F",
	"H\\KFX[ LFY[ YFK[ IFOF UF[F I[O[ U[[[",
	"H]KFRQR[ LFSQS[ ZFSQ IFOF VF\\F O[V[",
	"H\\XFK[ YFL[ LFKLKFYF K[Y[YUX[",
	"H\\RFK[ RFY[ RIX[ MUVU I[O[ U[[[",
	"G]LFL[ MFM[ IFUFXGYHZJZLYNXOUP UFWGXHYJYLXNWOUP MPUPXQYRZTZWYYXZU[I[ UPWQXRYTYWXYWZU[",
	"I[NFN[ OFO[ KFZFZLYF K[R[",
	"H\\RFJ[ RFZ[ RIY[ KZYZ J[Z[",
	"G\\LFL[ MFM[ SLST IFYFYLXF MPSP I[Y[YUX[",
	"H\\XFK[ YFL[ LFKLKFYF K[Y[YUX[",
	"F^KFK[ LFL[ XFX[ YFY[ HFOF UF\\F LPXP H[O[ U[\\[",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF QFOGMILKKOKRLVMXOZQ[ S[UZWXXVYRYOXKWIUGSF OMOT UMUT OPUP OQUQ",
	"MXRFR[ SFS[ OFVF O[V[",
	"F\\KFK[ LFL[ YFLS QOY[ POX[ HFOF UF[F H[O[ U[[[",
	"H\\RFK[ RFY[ RIX[ I[O[ U[[[",
	"F_KFK[ LFRX KFR[ YFR[ YFY[ ZFZ[ HFLF YF]F H[N[ V[][",
	"G^LFL[ MFYY MHY[ YFY[ IFMF VF\\F I[O[",
	"G]KEJJ ZEYJ ONNS VNUS KWJ\\ ZWY\\ KGYG KHYH OPUP OQUQ KYYY KZYZ",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF QFOGMILKKOKRLVMXOZQ[ S[UZWXXVYRYOXKWIUGSF",
	"F^KFK[ LFL[ XFX[ YFY[ HF\\F H[O[ U[\\[",
	"G]LFL[ MFM[ IFUFXGYHZJZMYOXPUQMQ UFWGXHYJYMXOWPUQ I[P[",
	"H]KFRPJ[ JFQP JFYFZLXF KZXZ J[Y[ZUX[",
	"I\\RFR[ SFS[ LFKLKFZFZLYF O[V[",
	"I\\KKKILGMFOFPGQIRMR[ KIMGOGQI ZKZIYGXFVFUGTISMS[ ZIXGVGTI O[V[",
	"H]RFR[ SFS[ PKMLLMKOKRLTMUPVUVXUYTZRZOYMXLUKPK PKNLMMLOLRMTNUPV UVWUXTYRYOXMWLUK OFVF O[V[",
	"H\\KFX[ LFY[ YFK[ IFOF UF[F I[O[ U[[[",
	"G^RFR[ SFS[ IMJLLMMQNSOTQU JLKMLQMSNTQUTUWTXSYQZM[L TUVTWSXQYM[L\\M OFVF O[V[",
	"G]JXK[O[MWKSJPJLKIMGPFTFWGYIZLZPYSWWU[Y[ZX MWLTKPKLLINGPF TFVGXIYLYPXTWW KZNZ VZYZ",
	"H\\UFH[ UFV[ THU[ LUUU F[L[ R[X[",
	"F^OFI[ PFJ[ LFWFZG[I[KZNYOVP WFYGZIZKYNXOVP MPVPXQYSYUXXVZR[F[ VPWQXSXUWXUZR[",
	"H]ZH[H\\F[L[JZHYGWFTFQGOIMLLOKSKVLYMZP[S[UZWXXV TFRGPINLMOLSLVMYNZP[",
	"F]OFI[ PFJ[ LFUFXGYHZKZOYSWWUYSZO[F[ UFWGXHYKYOXSVWTYRZO[",
	"F]OFI[ PFJ[ TLRT LF[FZLZF MPSP F[U[WVT[",
	"F\\OFI[ PFJ[ TLRT LF[FZLZF MPSP F[M[",
	"H^ZH[H\\F[L[JZHYGWFTFQGOIMLLOKSKVLYMZP[R[UZWXYT TFRGPINLMOLSLVMYNZP[ R[TZVXXT UT\\T",
	"E_NFH[ OFI[ [FU[ \\FV[ KFRF XF_F LPXP E[L[ R[Y[",
	"LYUFO[ VFP[ RFYF L[S[",
	"I[XFSWRYQZO[M[KZJXJVKULVKW WFRWQYO[ TF[F",
	"F]OFI[ PFJ[ ]FLS SOW[ ROV[ LFSF YF_F F[M[ S[Y[",
	"H\\QFK[ RFL[ NFUF H[W[YUV[",
	"E`NFH[ NFO[ OFPY \\FO[ \\FV[ ]FW[ KFOF \\F`F E[K[ S[Z[",
	"F_OFI[ OFVX OIV[ \\FV[ LFOF YF_F F[L[",
	"G]SFPGNILLKOJSJVKYLZN[Q[TZVXXUYRZNZKYHXGVFSF SFQGOIMLLOKSKVLYN[ Q[SZUXWUXRYNYKXHVF",
	"F]OFI[ PFJ[ LFXF[G\\I\\K[NYPUQMQ XFZG[I[KZNXPUQ F[M[",
	"G]SFPGNILLKOJSJVKYLZN[Q[TZVXXUYRZNZKYHXGVFSF SFQGOIMLLOKSKVLYN[ Q[SZUXWUXRYNYKXHVF LYLXMVOUPURVSXS_T`V`W^W] SXT^U_V_W^",
	"F^OFI[ PFJ[ LFWFZG[I[KZNYOVPMP WFYGZIZKYNXOVP RPTQURVZW[Y[ZYZX URWYXZYZZY F[M[",
	"G^ZH[H\\F[L[JZHYGVFRFOGMIMKNMONVRXT MKOMVQWRXTXWWYVZS[O[LZKYJWJUI[JYKY",
	"H]UFO[ VFP[ OFLLNF]F\\L\\F L[S[",
	"F_NFKQJUJXKZN[R[UZWXXU\\F OFLQKUKXLZN[ KFRF YF_F",
	"H\\NFO[ OFPY \\FO[ LFRF XF^F",
	"E_MFK[ NFLY UFK[ UFS[ VFTY ]FS[ JFQF ZF`F",
	"G]NFU[ OFV[ \\FH[ LFRF XF^F F[L[ R[X[",
	"H]NFRPO[ OFSPP[ ]FSP LFRF YF_F L[S[",
	"G][FH[ \\FI[ OFLLNF\\F H[V[XUU[",
	"H\\KILKXWYYY[ LLXX KIKKLMXYY[ PPLTKVKXLZK[ KVMZ LTLVMXMZK[ SSXN VIVLWNYNYLWKVI VIWLYN",
	"H\\QIK[ SIY[ RIX[ MUVU I[O[ U[[[ QBOCNENGOIQJSJUIVGVEUCSBQB",
	"",
	"",
	"",
	"",
	"",
	"G]IB[b",
	"F^RJIZ RJ[Z",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"I]NONPMPMONNPMTMVNWOXQXXYZZ[ WOWXXZZ[[[ WQVRPSMTLVLXMZP[S[UZWX PSNTMVMXNZP[",
	"G\\LFL[ MFM[ MPONQMSMVNXPYSYUXXVZS[Q[OZMX SMUNWPXSXUWXUZS[ IFMF",
	"H[WPVQWRXQXPVNTMQMNNLPKSKULXNZQ[S[VZXX QMONMPLSLUMXOZQ[",
	"H]WFW[ XFX[ WPUNSMQMNNLPKSKULXNZQ[S[UZWX QMONMPLSLUMXOZQ[ TFXF W[[[",
	"H[LSXSXQWOVNTMQMNNLPKSKULXNZQ[S[VZXX WSWPVN QMONMPLSLUMXOZQ[",
	"KXUGTHUIVHVGUFSFQGPIP[ SFRGQIQ[ MMUM M[T[",
	"I\\QMONNOMQMSNUOVQWSWUVVUWSWQVOUNSMQM ONNPNTOV UVVTVPUN VOWNYMYNWN NUMVLXLYM[P\\U\\X]Y^ LYMZP[U[X\\Y^Y_XaUbObLaK_K^L\\O[",
	"G]LFL[ MFM[ MPONRMTMWNXPX[ TMVNWPW[ IFMF I[P[ T[[[",
	"MXRFQGRHSGRF RMR[ SMS[ OMSM O[V[",
	"MXSFRGSHTGSF TMT_SaQbObNaN`O_P`Oa SMS_RaQb PMTM",
	"G\\LFL[ MFM[ WMMW RSX[ QSW[ IFMF TMZM I[P[ T[Z[",
	"MXRFR[ SFS[ OFSF O[V[",
	"BcGMG[ HMH[ HPJNMMOMRNSPS[ OMQNRPR[ SPUNXMZM]N^P^[ ZM\\N]P][ DMHM D[K[ O[V[ Z[a[",
	"G]LML[ MMM[ MPONRMTMWNXPX[ TMVNWPW[ IMMM I[P[ T[[[",
	"H\\QMNNLPKSKULXNZQ[S[VZXXYUYSXPVNSMQM QMONMPLSLUMXOZQ[ S[UZWXXUXSWPUNSM",
	"G\\LMLb MMMb MPONQMSMVNXPYSYUXXVZS[Q[OZMX SMUNWPXSXUWXUZS[ IMMM IbPb",
	"H\\WMWb XMXb WPUNSMQMNNLPKSKULXNZQ[S[UZWX QMONMPLSLUMXOZQ[ Tb[b",
	"IZNMN[ OMO[ OSPPRNTMWMXNXOWPVOWN KMOM K[R[",
	"J[WOXMXQWOVNTMPMNNMOMQNRPSUUWVXW MPNQPRUTWUXVXYWZU[Q[OZNYMWM[NY",
	"KZPFPWQZS[U[WZXX QFQWRZS[ MMUM",
	"G]LMLXMZP[R[UZWX MMMXNZP[ WMW[ XMX[ IMMM TMXM W[[[",
	"I[LMR[ MMRY XMR[ JMPM TMZM",
	"F^JMN[ KMNX RMN[ RMV[ SMVX ZMV[ GMNM WM]M",
	"H\\LMW[ MMX[ XML[ JMPM TMZM J[P[ T[Z[",
	"H[LMR[ MMRY XMR[P_NaLbKbJaK`La JMPM TMZM",
	"I[WML[ XMM[ MMLQLMXM L[X[XWW[",
	"G^QMNNLPKRJUJXKZN[P[RZUWWTYPZM QMONMPLRKUKXLZN[ QMSMUNVPXXYZZ[ SMTNUPWXXZZ[[[",
	"G\\TFQGOIMMLPKTJZIb TFRGPINMMPLTKZJb TFVFXGYHYKXMWNTOPO VFXHXKWMVNTO POTPVRWTWWVYUZR[P[NZMYLV POSPURVTVWUYTZR[",
	"H\\IPKNMMOMQNROSRSVRZOb JOLNPNRO ZMYPXRSYP^Nb YMXPWRSY",
	"I\\VNTMRMONMQLTLWMYNZP[R[UZWWXTXQWOSJRHRFSEUEWFYH RMPNNQMTMXNZ R[TZVWWTWPVNTKSISGTFVFYH",
	"I[XPVNTMPMNNNPPRSS PMONOPQRSS SSNTLVLXMZP[S[UZWX SSOTMVMXNZP[",
	"I[TFRGQHQIRJUKZKZJWKSMPOMRLULWMYP[S]T_TaSbQbPa ULQONRMUMWNYP[",
	"G]HQIOKMNMONOPNTL[ MMNNNPMTK[ NTPPRNTMVMXNYOYRXWUb VMXOXRWWTb",
	"F]GQHOJMMMNNNPMUMXNZO[ LMMNMPLULXMZO[Q[SZUXWUXRYMYIXGVFTFRHRJSMUPWRZT SZUWVUWRXMXIWGVF",
	"LXRMPTOXOZP[S[UYVW SMQTPXPZQ[",
	"H\\NMJ[ OMK[ XMYNZNYMWMUNQROSMS OSQTSZT[ OSPTRZS[U[WZYW",
	"H\\KFMFOGPHQJWXXZY[ MFOHPJVXWZY[Z[ RMJ[ RMK[",
	"F]MMGb NMHb MPLVLYN[P[RZTXVU XMUXUZV[Y[[Y\\W YMVXVZW[",
	"H\\NML[ OMNSMXL[ YMXQVU ZMYPXRVUTWQYOZL[ KMOM",
	"IZTFRGQHQIRJUKXK UKQLOMNONQPSSTVT UKRLPMOOOQQSST STOUMVLXLZN\\S^T_TaRbPb STPUNVMXMZO\\S^",
	"I[RMONMQLTLWMYNZP[R[UZWWXTXQWOVNTMRM RMPNNQMTMXNZ R[TZVWWTWPVN",
	"G]PNL[ PNM[ VNV[ VNW[ IPKNNM[M IPKONN[N",
	"H[LVMYNZP[R[UZWWXTXQWOVNTMRMONMQLTHb R[TZVWWTWPVN RMPNNQMTIb",
	"H][MQMNNLQKTKWLYMZO[Q[TZVWWTWQVOUNSM QMONMQLTLXMZ Q[SZUWVTVPUN UN[N",
	"H\\SNP[ SNQ[ JPLNOMZM JPLOONZN",
	"H\\IQJOLMOMPNPPNVNYP[ NMONOPMVMYNZP[Q[TZVXXUYRYOXMWNXOYR XUYO",
	"G]ONMOKQJTJWKYLZN[Q[TZWXYUZRZOXMVMTORSPXMb JWLYNZQZTYWWYU ZOXNVNTPRSPYNb",
	"I[KMMMONPPU_VaWb MMNNOPT_UaWbYb ZMYOWRM]K`Jb",
	"F]UFOb VFNb GQHOJMMMNNNPMUMXOZRZTYWVYS LMMNMPLULXMZO[R[TZVXXUYS[M",
	"F]JQLOONNMLNJQITIWJZK[M[OZQWRT IWJYKZMZOYQW QTQWRZS[U[WZYWZTZQYNXMWNYOZQ QWRYSZUZWYYW",
	"H]XMVTUXUZV[Y[[Y\\W YMWTVXVZW[ VTVQUNSMQMNNLQKTKWLYMZO[Q[SZUWVT QMONMQLTLXMZ",
	"H[PFLSLVMYNZ QFMS MSNPPNRMTMVNWOXQXTWWUZR[P[NZMWMS VNWPWTVWTZR[ MFQF",
	"I[WPWQXQXPWNUMRMONMQLTLWMYNZP[R[UZWW RMPNNQMTMXNZ",
	"H]ZFVTUXUZV[Y[[Y\\W [FWTVXVZW[ VTVQUNSMQMNNLQKTKWLYMZO[Q[SZUWVT QMONMQLTLXMZ WF[F",
	"I[MVQUTTWRXPWNUMRMONMQLTLWMYNZP[R[UZWX RMPNNQMTMXNZ",
	"KZZGYHZI[H[GZFXFVGUHTJSMP[O_Na XFVHUJTNRWQ[P^O`NaLbJbIaI`J_K`Ja OMYM",
	"H\\YMU[T^RaObLbJaI`I_J^K_J` XMT[S^QaOb VTVQUNSMQMNNLQKTKWLYMZO[Q[SZUWVT QMONMQLTLXMZ",
	"H]PFJ[ QFK[ MTOPQNSMUMWNXOXQVWVZW[ UMWOWQUWUZV[Y[[Y\\W MFQF",
	"LYUFTGUHVGUF MQNOPMSMTNTQRWRZS[ RMSNSQQWQZR[U[WYXW",
	"LYVFUGVHWGVF NQOOQMTMUNUQR[Q^P`OaMbKbJaJ`K_L`Ka SMTNTQQ[P^O`Mb",
	"H\\PFJ[ QFK[ XNWOXPYOYNXMWMUNQROSMS OSQTSZT[ OSPTRZS[U[WZYW MFQF",
	"MYUFQTPXPZQ[T[VYWW VFRTQXQZR[ RFVF",
	"AbBQCOEMHMINIPHTF[ GMHNHPGTE[ HTJPLNNMPMRNSOSQP[ PMRORQO[ RTTPVNXMZM\\N]O]Q[W[Z\\[ ZM\\O\\QZWZZ[[^[`YaW",
	"F]GQHOJMMMNNNPMTK[ LMMNMPLTJ[ MTOPQNSMUMWNXOXQVWVZW[ UMWOWQUWUZV[Y[[Y\\W",
	"I[RMONMQLTLWMYNZP[R[UZWWXTXQWOVNTMRM RMPNNQMTMXNZ R[TZVWWTWPVN",
	"G\\HQIOKMNMONOPNTJb MMNNNPMTIb NTOQQNSMUMWNXOYQYTXWVZS[Q[OZNWNT WNXPXTWWUZS[ FbMb",
	"H\\XMRb YMSb VTVQUNSMQMNNLQKTKWLYMZO[Q[SZUWVT QMONMQLTLXMZ ObVb",
	"IZJQKOMMPMQNQPPTN[ OMPNPPOTM[ PTRPTNVMXMYNYOXPWOXN",
	"J[XOXPYPYOXNUMRMONNONQORVVWW NPOQVUWVWYVZS[P[MZLYLXMXMY",
	"KYTFPTOXOZP[S[UYVW UFQTPXPZQ[ NMWM",
	"F]GQHOJMMMNNNQLWLYN[ LMMNMQKWKYLZN[P[RZTXVT XMVTUXUZV[Y[[Y\\W YMWTVXVZW[",
	"H\\IQJOLMOMPNPQNWNYP[ NMONOQMWMYNZP[Q[TZVXXUYQYMXMYO",
	"C`DQEOGMJMKNKQIWIYK[ IMJNJQHWHYIZK[M[OZQXRV TMRVRYSZU[W[YZ[X\\V]R]M\\M]O UMSVSYU[",
	"H\\KQMNOMRMSOSR QMRORRQVPXNZL[K[JZJYKXLYKZ QVQYR[U[WZYW YNXOYPZOZNYMXMVNTPSRRVRYS[",
	"G\\HQIOKMNMONOQMWMYO[ MMNNNQLWLYMZO[Q[SZUXWT ZMV[U^SaPbMbKaJ`J_K^L_K` YMU[T^RaPb",
	"H\\YMXOVQNWLYK[ LQMOOMRMVO MOONRNVOXO LYNYRZUZWY NYR[U[WYXW",
	"G^VGUHVIWHWGUFRFOGMILLL[ RFPGNIMLM[ \\G[H\\I]H]G\\FZFXGWIW[ ZFYGXIX[ IM[M I[P[ T[[[",
	"G]WGVHWIXHWGUFRFOGMILLL[ RFPGNIMLM[ WMW[ XMX[ IMXM I[P[ T[[[",
	"G]VGUHVIWHWGUF XFRFOGMILLL[ RFPGNIMLM[ WHW[ XFX[ IMWM I[P[ T[[[",
	"BcRGQHRISHRGPFMFJGHIGLG[ MFKGIIHLH[ ]G\\H]I^H]G[FXFUGSIRLR[ XFVGTISLS[ ]M][ ^M^[ DM^M D[K[ O[V[ Z[a[",
	"BcRGQHRISHRGPFMFJGHIGLG[ MFKGIIHLH[ \\G[H\\I]H]G[F ^FXFUGSIRLR[ XFVGTISLS[ ]H][ ^F^[ DM]M D[K[ O[V[ Z[a[",
	"MXRMR[ SMS[ OMSM O[V[",
	"",
	"IZWNUMRMONMPLSLVMYNZQ[T[VZ RMPNNPMSMVNYOZQ[ MTUT",
	"I\\TFQGOJNLMOLTLXMZO[Q[TZVWWUXRYMYIXGVFTF TFRGPJOLNOMTMXNZO[ Q[SZUWVUWRXMXIWGVF NPWP",
	"G]UFOb VFNb QMMNKPJSJVKXMZP[S[WZYXZUZRYPWNTMQM QMNNLPKSKVLXNZP[ S[VZXXYUYRXPVNTM",
	"I[TMVNXPXOWNTMQMNNMOLQLSMUOWSZ QMONNOMQMSNUSZT\\T^S_Q_",
	"",
	"",
	"G]LMKNJPJRKUOYP[ JRKTOXP[P]O`MbLbKaJ_J\\KXMTOQRNTMVMYNZPZTYXWZU[T[SZSXTWUXTY VMXNYPYTXXWZ",
	"E_YGXHYIZHYGWFTFQGOINKMNLRJ[I_Ha TFRGPIOKNNLWK[J^I`HaFbDbCaC`D_E`Da _G^H_I`H`G_F]F[GZHYJXMU[T_Sa ]F[HZJYNWWV[U^T`SaQbObNaN`O_P`Oa IM^M",
	"F^[GZH[I\\H[GXFUFRGPIOKNNMRK[J_Ia UFSGQIPKONMWL[K^J`IaGbEbDaD`E_F`Ea YMWTVXVZW[Z[\\Y]W ZMXTWXWZX[ JMZM",
	"F^YGXHYIZHZGXF \\FUFRGPIOKNNMRK[J_Ia UFSGQIPKONMWL[K^J`IaGbEbDaD`E_F`Ea [FWTVXVZW[Z[\\Y]W \\FXTWXWZX[ JMYM",
	"@cTGSHTIUHTGRFOFLGJIIKHNGRE[D_Ca OFMGKIJKINGWF[E^D`CaAb?b>a>`?_@`?a `G_H`IaH`G]FZFWGUITKSNRRP[O_Na ZFXGVIUKTNRWQ[P^O`NaLbJbIaI`J_K`Ja ^M\\T[X[Z\\[_[aYbW _M]T\\X\\Z][ DM_M",
	"@cTGSHTIUHTGRFOFLGJIIKHNGRE[D_Ca OFMGKIJKINGWF[E^D`CaAb?b>a>`?_@`?a ^G]H^I_H_G]F aFZFWGUITKSNRRP[O_Na ZFXGVIUKTNRWQ[P^O`NaLbJbIaI`J_K`Ja `F\\T[X[Z\\[_[aYbW aF]T\\X\\Z][ DM^M",
	"LYMQNOPMSMTNTQRWRZS[ RMSNSQQWQZR[U[WYXW",
	"",
	"NV",
	"JZ",
	"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF QFOGNHMJLOLRMWNYOZQ[ S[UZVYWWXRXOWJVHUGSF",
	"H\\NJPISFS[ RGR[ N[W[",
	"H\\LJMKLLKKKJLHMGPFTFWGXHYJYLXNUPPRNSLUKXK[ TFVGWHXJXLWNTPPR KYLXNXSZVZXYYX NXS[W[XZYXYV",
	"H\\LJMKLLKKKJLHMGPFTFWGXIXLWNTOQO TFVGWIWLVNTO TOVPXRYTYWXYWZT[P[MZLYKWKVLUMVLW WQXTXWWYVZT[",
	"H\\THT[ UFU[ UFJUZU Q[X[",
	"H\\MFKP KPMNPMSMVNXPYSYUXXVZS[P[MZLYKWKVLUMVLW SMUNWPXSXUWXUZS[ MFWF MGRGWF",
	"H\\WIVJWKXJXIWGUFRFOGMILKKOKULXNZQ[S[VZXXYUYTXQVOSNRNOOMQLT RFPGNIMKLOLUMXOZQ[ S[UZWXXUXTWQUOSN",
	"H\\KFKL KJLHNFPFUIWIXHYF LHNGPGUI YFYIXLTQSSRVR[ XLSQRSQVQ[",
	"H\\PFMGLILLMNPOTOWNXLXIWGTFPF PFNGMIMLNNPO TOVNWLWIVGTF POMPLQKSKWLYMZP[T[WZXYYWYSXQWPTO PONPMQLSLWMYNZP[ T[VZWYXWXSWQVPTO",
	"H\\XMWPURRSQSNRLPKMKLLINGQFSFVGXIYLYRXVWXUZR[O[MZLXLWMVNWMX QSORMPLMLLMIOGQF SFUGWIXLXRWVVXTZR[",
	"MWRYQZR[SZRY",
	"MWR[QZRYSZS\\R^Q_",
	"MWRMQNROSNRM RYQZR[SZRY",
	"MWRMQNROSNRM R[QZRYSZS\\R^Q_",
	"MWRFQHRTSHRF RHRN RYQZR[SZRY",
	"I[MJNKMLLKLJMHNGPFSFVGWHXJXLWNVORQRT SFUGVHWJWLVNTP RYQZR[SZRY",
	"NVRFQM SFQM",
	"JZNFMM OFMM VFUM WFUM",
	"KYQFOGNINKOMQNSNUMVKVIUGSFQF",
	"JZRFRR MIWO WIMO",
	"G][BIb",
	"KYVBTDRGPKOPOTPYR]T`Vb TDRHQKPPPTQYR\\T`",
	"KYNBPDRGTKUPUTTYR]P`Nb PDRHSKTPTTSYR\\P`",
	"KYOBOb PBPb OBVB ObVb",
	"KYTBTb UBUb NBUB NbUb",
	"JYTBQEPHPJQMSOSPORSTSUQWPZP\\Q_Tb RDQGQKRN RVQYQ]R`",
	"KZPBSETHTJSMQOQPURQTQUSWTZT\\S_Pb RDSGSKRN RVSYS]R`",
	"KYUBNRUb",
	"KYOBVROb",
	"NVRBRb",
	"KYOBOb UBUb",
	"E_IR[R",
	"E_RIR[ IR[R",
	"F^RJR[ JRZR J[Z[",
	"F^RJR[ JJZJ JRZR",
	"G]KKYY YKKY",
	"MWQQQSSSSQQQ RQRS QRSR",
	"E_RIQJRKSJRI IR[R RYQZR[SZRY",
	"E_IO[O IU[U",
	"E_YIK[ IO[O IU[U",
	"E_IM[M IR[R IW[W",
	"F^ZIJRZ[",
	"F^JIZRJ[",
	"F^ZFJMZT JVZV J[Z[",
	"F^JFZMJT JVZV J[Z[",
	"F_[WYWWVUTRPQOONMNKOJQJSKUMVOVQURTUPWNYM[M",
	"F^IUISJPLONOPPTSVTXTZS[Q ISJQLPNPPQTTVUXUZT[Q[O",
	"G]JTROZT JTRPZT",
	"LXTFOL TFUGOL",
	"LXPFUL PFOGUL",
	"H\\KFLHNJQKSKVJXHYF KFLINKQLSLVKXIYF",
	"MWRHQGRFSGSIRKQL",
	"MWSFRGQIQKRLSKRJ",
	"MWRHSGRFQGQIRKSL",
	"MWQFRGSISKRLQKRJ",
	"E[HMLMRY KMR[ [BR[",
	"F^ZJSJOKMLKNJQJSKVMXOYSZZZ",
	"F^JJJQKULWNYQZSZVYXWYUZQZJ",
	"F^JJQJUKWLYNZQZSYVWXUYQZJZ",
	"F^JZJSKOLMNKQJSJVKXMYOZSZZ",
	"F^ZJSJOKMLKNJQJSKVMXOYSZZZ JRVR",
	"E_XP[RXT UMZRUW IRZR",
	"JZPLRITL MORJWO RJR[",
	"E_LPIRLT OMJROW JR[R",
	"JZPXR[TX MURZWU RIRZ",
	"I\\XRWOVNTMRMONMQLTLWMYNZP[R[UZWXXUYPYKXHWGUFRFPGOHOIPIPH RMPNNQMTMXNZ R[TZVXWUXPXKWHUF",
	"H\\JFR[ KFRY ZFR[ JFZF KGYG",
	"AbDMIMRY HNR[ b:R[",
	"F^[CZD[E\\D\\C[BYBWCUETGSJRNPZO^N` VDUFTJRVQZP]O_MaKbIbHaH`I_J`Ia",
	"F^[CZD[E\\D\\C[BYBWCUETGSJRNPZO^N` VDUFTJRVQZP]O_MaKbIbHaH`I_J`Ia QKNLLNKQKSLVNXQYSYVXXVYSYQXNVLSKQK",
	"F_\\S[UYVWVUUTTQPPONNLNJOIQISJULVNVPUQTTPUOWNYN[O\\Q\\S",
	"F^[FI[ NFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F WTUUTWTYV[X[ZZ[X[VYTWT",
	"F_[NZO[P\\O\\N[MZMYNXPVUTXRZP[M[JZIXIUJSPORMSKSIRGPFNGMIMKNNPQUXWZZ[[[\\Z\\Y M[KZJXJUKSMQ MKNMVXXZZ[",
	"E`WNVLTKQKOLNMMPMSNUPVSVUUVS QKOMNPNSOUPV WKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX XKWSWUXV",
	"H\\PBP_ TBT_ XIWJXKYJYIWGTFPFMGKIKKLMMNOOUQWRYT KKMMONUPWQXRYTYXWZT[P[MZKXKWLVMWLX",
	"G]OFOb UFUb JQZQ JWZW",
	"JZUITJUKVJVIUGSFQFOGNINKOMQOVR OMTPVRWTWVVXTZ PNNPMRMTNVPXU[ NVSYU[V]V_UaSbQbOaN_N^O]P^O_",
	"JZRFQHRJSHRF RFRb RQQTRbSTRQ LMNNPMNLLM LMXM TMVNXMVLTM",
	"JZRFQHRJSHRF RFRT RPQRSVRXQVSRRP RTRb R^Q`RbS`R^ LMNNPMNLLM LMXM TMVNXMVLTM L[N\\P[NZL[ L[X[ T[V\\X[VZT[",
	"I\\XFX[ KFXF PPXP K[X[",
	"",
	"E`QFNGKIILHOHRIUKXNZQ[T[WZZX\\U]R]O\\LZIWGTFQF ROQPQQRRSRTQTPSORO RPRQSQSPRP",
	"J[PFNGOIQJ PFOGOI UFWGVITJ UFVGVI QJOKNLMNMQNSOTQUTUVTWSXQXNWLVKTJQJ RUR[ SUS[ NXWX",
	"I\\RFOGMILLLMMPORRSSSVRXPYMYLXIVGSFRF RSR[ SSS[ NWWW",
	"D`PFMGJIHLGOGSHVJYM[P\\T\\W[ZY\\V]S]O\\LZIWGTFPF RFR\\ GQ]Q",
	"G`PMMNKPJSJTKWMYPZQZTYVWWTWSVPTNQMPM ]GWG[HUN ]G]M\\IVO \\HVN",
	"F\\IIJGLFOFQGRIRLQOPQNSKU OFPGQIQMPPNS VFT[ WFS[ KUYU",
	"I\\MFMU NFMQ MQNOONQMTMWNXPXRWTUV TMVNWPWRTXTZU[W[YY KFNF",
	"I\\RNOOMQLTLUMXOZR[S[VZXXYUYTXQVOSNRN RHNJRFRN SHWJSFSN RSQTQURVSVTUTTSSRS RTRUSUSTRT",
	"G^QHRFR[ THSFS[ JHKFKMLPNRQSRS MHLFLNMQ [HZFZMYPWRTSSS XHYFYNXQ NWWW",
	"G]LFL[ MFM[ IFUFXGYHZJZMYOXPUQMQ UFWGXHYJYMXOWPUQ I[Y[YVX[",
	"H[YGUGQHNJLMKPKSLVNYQ[U\\Y\\ YGVHSJQMPPPSQVSYV[Y\\",
	"F_OQMQKRJSIUIWJYKZM[O[QZRYSWSURSQROQ SHPQ ZJRR \\QST",
	"H\\OKUY UKOY KOYU YOKU",
	"F^NVLUKUIVHXHYI[K\\L\\N[OYOXNVKRJOJMKJMHPGTGWHYJZMZOYRVVUXUYV[X\\Y\\[[\\Y\\X[VYUXUVV JMKKMIPHTHWIYKZM",
	"F^NMLNKNIMHKHJIHKGLGNHOJOKNMKQJTJVKYM[P\\T\\W[YYZVZTYQVMUKUJVHXGYG[H\\J\\K[MYNXNVM JVKXMZP[T[WZYXZV",
	"I[KYYK QLULYKXOXS ULXLXO",
	"I[YKKY LQLUKYOXSX LULXOX",
	"I[YYKK SLOLKKLOLS OLLLLO",
	"I[KKYY QXUXYYXUXQ UXXXXU",
	"",
	"F_JMILIJJHLGNGPHQIRKSP IJKHMHOIPJQLRPR[ [M\\L\\J[HYGWGUHTISKRP \\JZHXHVIUJTLSPS[",
	"F^IGJKKMMOPPTPWOYMZK[G IGJJKLMNPOTOWNYLZJ[G PONPMQLSLVMXOZQ[S[UZWXXVXSWQVPTO PPNQMSMVNY VYWVWSVQTP",
	"F^MJMV NKNU VKVU WJWV IGKIMJPKTKWJYI[G IYKWMVPUTUWVYW[Y",
	"F^[ILIJJILINJPLQNQPPQNQLPJ[J IMJOKPMQ QMPKOJMI IXXXZW[U[SZQXPVPTQSSSUTWIW [TZRYQWP STTVUWWX",
	"F]OUMTLTJUIWIXJZL[M[OZPXPWOUJPINIKJILHOGSGWHYJZLZOYRVUUWUYV[X[YZZX MSKPJNJKKILH SGVHXJYLYOXRVU",
	"G_HKKHMKMV JILLLV MKPHRKRU OIQLQU RKUHWKW[ TIVLV[ WKZH[J\\M\\P[SZUXWUYP[ YIZJ[M[PZSYUWWTYP[",
	"F^ISMSLRKOKMLJNHQGSGVHXJYMYOXRWS[S ITOTMRLOLMMJOHQG SGUHWJXMXOWRUT[T KXYX KYYY",
	"F_GLJIMLMX IJLMLX MLPISLSX OJRMRX SLVIYLYW[Y UJXMXXZZ]W",
	"G]ZIJY ZIWJQJ XKUKQJ ZIYLYR XKXNYR QRJR PSMSJR QRQY PSPVQY",
	"F^HOJKOU JMOWRPWPZO[M[KZIXHWHUITKTMUPVRWUWXUZ WHVIUKUMWQXTXWWYUZ",
	"F^IOLLPN KMOORLUN QMTOWLYN VMXO[L IULRPT KSOURRUT QSTUWRYT VSXU[R",
	"F^JHNJPLQOQRPUNWJY JHMIOJQLRO RRQUOWMXJY ZHWIUJSLRO RRSUUWWXZY ZHVJTLSOSRTUVWZY IP[P IQ[Q",
	"",
	"",
	"",
	"",
	"NVQQQSSSSQQQ QQSS SQQS",
	"JZMPQRTTVVWYW[V]U^ MQST MRPSTUVWWY",
	"JZWKVMTOPQMR SPMS UFVGWIWKVNTPQRMT",
	"H\\SMONLPKRKTLVNWQWUVXTYRYPXNVMSM XNSM VMQNLP ONKR LVQW NWSVXT UVYR",
	"H\\SMONLPKRKTLVNWQWUVXTYRYPXNVMSM XNSM VMQNLP ONKR LVQW NWSVXT UVYR",
	"J[SMPNNPMRMTNVPWRWUVWTXRXPWNUMSM OPUM NRVN MTWO NUXP OVWR PWVT",
	"JZOGO^ UFU] MNWL MOWM MWWU MXWV",
	"JZNFNX VLV^ NNVL NOVM NWVU NXVV",
	"JZNBNW NNQLTLVMWOWQVSSUQVNW NNQMTMVN UMVOVQUSSU",
	"E_HIHL \\I\\L HI\\I HJ\\J HK\\K HL\\L",
	"JZMNMQ WNWQ MNWN MOWO MPWP MQWQ",
	"JZMLWX MLONQOTOVNWMWKUKUMTO ONTO QOWM VKVN ULWL WXUVSUPUNVMWMYOYOWPU UVPU SUMW NVNY MXOX",
	"JZPOOMOKMKMMNNPOSOUNWL NKNN MLOL MMSO POUN WLWY",
	"A^GfHfIeIdHcGcFdFfGhIiKiNhPfQdR`RUQ;Q4R/S-U,V,X-Y/Y3X6W8U;P?JCHEFHEJDNDREVGYJ[N\\R\\V[XZZW[T[PZMYKWITHPHMIKKJNJRKUMW GdGeHeHdGd U;Q?LCIFGIFKENERFVGXJ[ R\\U[WZYWZTZPYMXKVITH",
	"EfNSOUQVSVUUVSVQUOSNQNOONPMSMVNYP[S\\V\\Y[[Y\\W]T]P\\MZJXIUHRHOIMJKLIOHSHXI]KaMcPeTfYf]e`cba KLJNIRIXJ\\L`NbQdUeYe]d_cba POTO OPUP NQVQ NRVR NSVS OTUT PUTU aLaNcNcLaL bLbN aMcM aVaXcXcVaV bVbX aWcW",
	"D`H@Hd M@Md W@Wd \\@\\d MMWK MNWL MOWM MWWU MXWV MYWW",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"NVQQQSSSSQQQ QQSS SQQS",
	"JZMPQRTTVVWYW[V]U^ MQST MRPSTUVWWY",
	"JZWKVMTOPQMR SPMS UFVGWIWKVNTPQRMT",
	"H\\PMMNLOKQKSLUMVPWTWWVXUYSYQXOWNTMPM MNLPLSMUNVPW WVXTXQWOVNTM",
	"H\\SMONLPKRKTLVNWQWUVXTYRYPXNVMSM XNSM VMQNLP ONKR LVQW NWSVXT UVYR",
	"J[SMPNNPMRMTNVPWRWUVWTXRXPWNUMSM OPUM NRVN MTWO NUXP OVWR PWVT",
	"JZOGO^ UFU] MNWL MOWM MWWU MXWV",
	"JZNFNX VLV^ NNVL NOVM NWVU NXVV",
	"JZNBNW NNQLTLVMWOWQVSSUQVNW NNQMTMVN UMVOVQUSSU",
	"E_HIHL \\I\\L HI\\I HJ\\J HK\\K HL\\L",
	"JZMNMQ WNWQ MNWN MOWO MPWP MQWQ",
	"JZQCVMRTRU ULQS TITKPRRUUY W\\UYSXQXOYN[N]O_Ra W\\UZSYOYO]P_Ra SXPZN]",
	"JZPOOMOKMKMMNNPOSOUNWL NKNN MLOL MMSO POUN WLSY",
	"A^GfHfIeIdHcGcFdFfGhIiKiNhPfQdR`RUQ;Q4R/S-U,V,X-Y/Y3X6W8U;P?JCHEFHEJDNDREVGYJ[N\\R\\V[XZZW[T[PZMYKWITHPHMIKKJNJRKUMW GdGeHeHdGd U;Q?LCIFGIFKENERFVGXJ[ R\\U[WZYWZTZPYMXKVITH",
	"IjNQOOQNSNUOVQVSUUSVQVOUNTMQMNNKPISHWH[I^K`NaRaW`[_]]`ZcVfQiMk WHZI]K_N`R`W_[^]\\`YcTgQi POTO OPUP NQVQ NRVR NSVS OTUT PUTU eLeNgNgLeL fLfN eMgM eVeXgXgVeV fVfX eWgW",
	"D`H>Hf I>If M>Mf QBSBSDQDQAR?T>W>Y?[A\\D\\I[LYNWOUOSNRLQNOQNROSQVRXSVUUWUYV[X\\[\\`[cYeWfTfReQcQ`S`SbQb RBRD QCSC Y?ZA[D[IZLYN RLRNPQNRPSRVRX YVZX[[[`ZcYe R`Rb QaSa",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"AcHBHb IBIb [B[b \\B\\b DB`B DbMb Wb`b",
	"BaGBQPFb FBPP EBPQ EB\\B^I[B Ga\\a Fb\\b^[[b",
	"I[X+U1R8P=OANFMNMVN^OcPgRlUsXy U1S6Q<P@OFNNNVO^PdQhSnUs",
	"I[L+O1R8T=UAVFWNWVV^UcTgRlOsLy O1Q6S<T@UFVNVVU^TdShQnOs",
	"I[M+MRMy N+NRNy M+X+ MyXy",
	"I[V+VRVy W+WRWy L+W+ LyWy",
	"H[V+R1P5O:O>PBTJTLSNROMRRUSVTXTZPbOfOjPoRsVy T.R2Q5P:P>QCRF R^QaPfPjQoRrTv",
	"I\\N+R1T5U:U>TBPJPLQNROWRRUQVPXPZTbUfUjToRsNy P.R2S5T:T>SCRF R^SaTfTjSoRrPv",
	"I[V.S1Q4O8N=NCOIPMSXT\\UbUgTlSoQs S1Q5P8O=OBPHQLTWU[VaVgUlSpQsNv",
	"I[N.Q1S4U8V=VCUITMQXP\\ObOgPlQoSs Q1S5T8U=UBTHSLPWO[NaNgOlQpSsVv",
	"7Z:RARRo @RQo ?RRr Z\"VJRr",
	"Ca].\\.[/[0\\1]1^0^.],[+Y+W,U.T0S3R:QJQjPsOv \\/\\0]0]/\\/ R:Rj U.T1S:SZRjQqPtOvMxKyIyGxFvFtGsHsItIuHvGv GtGuHuHtGt",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"H\\RFJ[ RIK[J[ RIY[Z[ RFZ[ MUWU LVXV",
	"H\\LFL[ MGMZ LFTFWGXHYJYMXOWPTQ MGTGWHXJXMWOTP MPTPWQXRYTYWXYWZT[L[ MQTQWRXTXWWYTZMZ",
	"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZV ZKYKXIWHUGQGOHMKLNLSMVOYQZUZWYXXYVZV",
	"H]LFL[ MGMZ LFSFVGXIYKZNZSYVXXVZS[L[ MGSGVHWIXKYNYSXVWXVYSZMZ",
	"I\\MFM[ NGNZ MFYF NGYGYF NPTPTQ NQTQ NZYZY[ M[Y[",
	"I[MFM[ NGN[M[ MFYF NGYGYF NPTPTQ NQTQ",
	"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZVZRUR ZKYKXIWHUGQGOHNIMKLNLSMVNXOYQZUZWYXXYVYSUSUR",
	"G]KFK[ KFLFL[K[ YFXFX[Y[ YFY[ LPXP LQXQ",
	"NWRFR[S[ RFSFS[",
	"J[VFVVUYSZQZOYNVMV VFWFWVVYUZS[Q[OZNYMV",
	"H]LFL[M[ LFMFM[ ZFYFMR ZFMS POY[Z[ QOZ[",
	"IZMFM[ MFNFNZ NZYZY[ M[Y[",
	"F^JFJ[ KKK[J[ KKR[ JFRX ZFRX YKR[ YKY[Z[ ZFZ[",
	"G]KFK[ LIL[K[ LIY[ KFXX XFXX XFYFY[",
	"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF QGNHLKKNKSLVNYQZSZVYXVYSYNXKVHSGQG",
	"H\\LFL[ MGM[L[ LFUFWGXHYJYMXOWPUQMQ MGUGWHXJXMWOUPMP",
	"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF QGNHLKKNKSLVNYQZSZVYXVYSYNXKVHSGQG SXX]Y] SXTXY]",
	"H\\LFL[ MGM[L[ LFTFWGXHYJYMXOWPTQMQ MGTGWHXJXMWOTPMP RQX[Y[ SQY[",
	"H\\YIWGTFPFMGKIKKLMMNOOTQVRWSXUXXWYTZPZNYMXKX YIWIVHTGPGMHLILKMMONTPVQXSYUYXWZT[P[MZKX",
	"J[RGR[ SGS[R[ LFYFYG LFLGYG",
	"G]KFKULXNZQ[S[VZXXYUYF KFLFLUMXNYQZSZVYWXXUXFYF",
	"H\\JFR[ JFKFRX ZFYFRX ZFR[",
	"E_GFM[ GFHFMX RFMX RIM[ RIW[ RFWX ]F\\FWX ]FW[",
	"H\\KFX[Y[ KFLFY[ YFXFK[ YFL[K[",
	"I\\KFRPR[S[ KFLFSP ZFYFRP ZFSPS[",
	"H\\XFK[ YFL[ KFYF KFKGXG LZYZY[ K[Y[",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"E\\XFVHTKQPOSLWIZG[E[DZDXEWFXEY XFWJUTT[ XFU[ T[TYSVRTPRNQLQKRKTLWOZR[V[XZ",
	"F^UGTHSJQOOUNWLZJ[ THSKQSPVOXMZJ[H[GZGXHWIXHY OLNNMOKOJNJLKJMHOGRFXFZG[I[KZMXNTORO XFYGZIZKYMXN TOWPXQYSYVXYWZU[S[RZRXSU TOVPWQXSXVWYU[",
	"H]KHJJJLKNNOQOUNWMYKZIZGYFWFTGQJOMMQLULXMZP[R[UZWXXVXTWRURSSRU WFUGRJPMNQMUMXNZP[",
	"F]UGTHSJQOOUNWLZJ[ THSKQSPVOXMZJ[H[GZGXHWJWLXNZP[S[UZWXYTZOZLYIWGUFPFMGKIJKJMKNMNNMOK",
	"I\\WIVJVLWMYMZKZIYGWFTFRGQHPJPLQNSO TFRHQJQMSO SOQONPLRKTKWLYMZO[R[UZWXXVXTWRURSSRU QOOPMRLTLXMZ",
	"G\\WHVJTORUQWOZM[ QLPNNOLOKMKKLINGQF[FXGWHVKTSSVRXPZM[K[IZHYHXIWJXIY SFWGXG OSPRRQVQXPZMXT",
	"G]JIIKIMJOLPOPROTNWKXHXGWFVFTGRIQKPNPQQSSTUTWSYQZO WFUGSIRKQNQRST ZOYSWWUYSZO[L[JZIXIWJVKWJX YSWVUXRZO[",
	"F^LLKKKILGOFRFOQMWLYKZI[G[FZFXGWHXGY RFOONRLWKYI[ JTKSMRVOXN[L]J^H^G]F\\FZGXJWLURTVTYV[W[YZ[X \\FZHXLVRUVUYV[",
	"IYWHUKSPQUPWNZL[ YLWNTOQOONNLNJOHQGUFYFWHVJTPRVQXOZL[J[IZIXJWKXJY",
	"IZYFWHUKSPPYN] YMWOTPQPOONMNKOIQGUFYFWIVKSTQXPZN]M^K_J^J\\KZMXOWRVVU",
	"F^LLKKKIMGPFRFOQMWLYKZI[G[FZFXGWHXGY RFOONRLWKYI[ ZGWKUMSNPO ]G\\H]I^H^G]F\\FZGWLVMTNPO POSPTRUYV[ PORPSRTYV[W[YZ[X",
	"I[MILKLMMOOPRPUOWNZK[H[GZFYFWGVHTKPUOWMZK[ VHTLRSQVPXNZK[I[HZHXIWKWMXPZR[U[WZYX",
	"D`RFNOKUIXGZE[C[BZBXCWDXCY RFPMOQNVNZP[ RFQJPOOVOZP[ [FWORXP[ [FYMXQWVWZY[Z[\\Z^X [FZJYOXVXZY[",
	"G^RFQJOPMULWJZH[F[EZEXFWGXFY RFRKSVT[ RFSKTVT[ `G_H`IaHaG`F^F\\GZJYLWQUWT[",
	"H]SFQGOIMLLNKRKVLYMZO[Q[TZVXXUYSZOZKYHXGWGUHSJQNPSPV QGOJMNLRLVMYO[",
	"F]UGTHSJQOOUNWLZJ[ THSKQSPVOXMZJ[H[GZGXHWIXHY OLNNMOKOJNJLKJMHOGRFVFYGZH[J[MZOYPVQTQRP VFXGYHZJZMYOXPVQ",
	"H]UJULTNSOQPOPNNNLOIQGTFWFYGZIZMYPWSSWPYNZK[I[HZHXIWKWMXPZS[V[XZZX WFXGYIYMXPVSSVOYK[",
	"F^UGTHSJQOOUNWLZJ[ THSKQSPVOXMZJ[H[GZGXHWIXHY OLNNMOKOJNJLKJMHOGRFWFZG[I[KZMYNVORO WFYGZIZKYMXNVO ROUPVRWYX[ ROTPURVYX[Y[[Z]X",
	"H\\NIMKMMNOPPSPVOXN[K\\H\\G[FZFXGWHVJUMSTRWPZN[ VJUNTUSXQZN[K[IZHXHWIVJWIX",
	"I[YHXJVOTUSWQZO[ SLRNPONOMMMKNIPGSF\\FZGYHXKVSUVTXRZO[M[KZJYJXKWLXKY UFYGZG",
	"G]HJJGLFMFOHOKNNKVKYL[ MFNHNKKSJVJYL[N[PZSWUTVR ZFVRUVUYW[X[ZZ\\X [FWRVVVYW[",
	"G\\HJJGLFMFOHOKNOLVLYM[ MFNHNKLRKVKYM[N[QZTWVTXPYMZIZGYFXFWGVIVLWNYP[Q]Q",
	"F]ILHLGKGIHGJFNFMHLLKUJ[ LLLUK[ VFTHRLOUMYK[ VFUHTLSUR[ TLTUS[ `F^G\\IZLWUUYS[",
	"H\\PKOLMLLKLIMGOFQFSGTITLSPQUOXMZJ[H[GZGXHWIXHY QFRGSISLRPPUNXLZJ[ ]G\\H]I^H^G]F[FYGWIULSPRURXSZT[U[WZYX",
	"G]JJLGNFOFQGQIOOORPT OFPGPINONRPTRTUSWQYNZL \\FZLWTUX ]F[LYQWUUXSZP[L[JZIXIWJVKWJX",
	"G\\ZHYJWOVRUTSWQYOZL[ SLRNPONOMMMKNIPGSF]F[GZHYKXOVUTXQZL[H[GZGXHWJWLXOZQ[T[WZYX VFZG[G",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"H\\WMW[X[ WMXMX[ WPUNSMPMNNLPKSKULXNZP[S[UZWX WPSNPNNOMPLSLUMXNYPZSZWX",
	"H\\LFL[M[ LFMFM[ MPONQMTMVNXPYSYUXXVZT[Q[OZMX MPQNTNVOWPXSXUWXVYTZQZMX",
	"I[XPVNTMQMONMPLSLUMXOZQ[T[VZXX XPWQVOTNQNOONPMSMUNXOYQZTZVYWWXX",
	"H\\WFW[X[ WFXFX[ WPUNSMPMNNLPKSKULXNZP[S[UZWX WPSNPNNOMPLSLUMXNYPZSZWX",
	"I[MTXTXQWOVNTMQMONMPLSLUMXOZQ[T[VZXX MSWSWQVOTNQNOONPMSMUNXOYQZTZVYWWXX",
	"LZWFUFSGRJR[S[ WFWGUGSH TGSJS[ OMVMVN OMONVN",
	"H\\XMWMW\\V_U`SaQaO`N_L_ XMX\\W_UaSbPbNaL_ WPUNSMPMNNLPKSKULXNZP[S[UZWX WPSNPNNOMPLSLUMXNYPZSZWX",
	"H\\LFL[M[ LFMFM[ MQPNRMUMWNXQX[ MQPORNTNVOWQW[X[",
	"NWRFQGQHRISITHTGSFRF RGRHSHSGRG RMR[S[ RMSMS[",
	"NWRFQGQHRISITHTGSFRF RGRHSHSGRG RMRbSb RMSMSb",
	"H[LFL[M[ LFMFM[ XMWMMW XMMX PTV[X[ QSX[",
	"NWRFR[S[ RFSFS[",
	"CbGMG[H[ GMHMH[ HQKNMMPMRNSQS[ HQKOMNONQORQR[S[ SQVNXM[M]N^Q^[ SQVOXNZN\\O]Q][^[",
	"H\\LML[M[ LMMMM[ MQPNRMUMWNXQX[ MQPORNTNVOWQW[X[",
	"I\\QMONMPLSLUMXOZQ[T[VZXXYUYSXPVNTMQM QNOONPMSMUNXOYQZTZVYWXXUXSWPVOTNQN",
	"H\\LMLbMb LMMMMb MPONQMTMVNXPYSYUXXVZT[Q[OZMX MPQNTNVOWPXSXUWXVYTZQZMX",
	"H\\WMWbXb WMXMXb WPUNSMPMNNLPKSKULXNZP[S[UZWX WPSNPNNOMPLSLUMXNYPZSZWX",
	"KYOMO[P[ OMPMP[ PSQPSNUMXM PSQQSOUNXNXM",
	"J[XPWNTMQMNNMPNRPSUUWV VUWWWXVZ WYTZQZNY OZNXMX XPWPVN WOTNQNNO ONNPOR NQPRUTWUXWXXWZT[Q[NZMX",
	"MXRFR[S[ RFSFS[ OMVMVN OMONVN",
	"H\\LMLWMZO[R[TZWW LMMMMWNYPZRZTYWW WMW[X[ WMXMX[",
	"JZLMR[ LMMMRY XMWMRY XMR[",
	"F^IMN[ IMJMNX RMNX RPN[ RPV[ RMVX [MZMVX [MV[",
	"I[LMW[X[ LMMMX[ XMWML[ XMM[L[",
	"JZLMR[ LMMMRY XMWMRYNb XMR[ObNb",
	"I[VNL[ XMNZ LMXM LMLNVN NZXZX[ L[X[",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"K[UUTSRRPRNSMTLVLXMZO[Q[SZTX PRNTMVMYO[ VRTXTZV[XZYY[V WRUXUZV[",
	"LZLVNSPO SFMXMZO[P[RZTXUUURVVWWXWZV TFNXNZO[",
	"LXTSSTTTTSSRQROSNTMVMXNZP[S[VYXV QROTNVNYP[",
	"K[UUTSRRPRNSMTLVLXMZO[Q[SZTX PRNTMVMYO[ ZFTXTZV[XZYY[V [FUXUZV[",
	"LXOYQXRWSUSSRRQROSNTMVMXNZP[S[VYXV QROTNVNYP[",
	"OXRRUOWLXIXGWFUGTIKdKfLgNfOcPZQ[S[UZVYXV TISNRRO[M`Kd",
	"K[UUTSRRPRNSMTLVLXMZO[Q[SZTX PRNTMVMYO[ VRPd WRT[R`PdOfMgLfLdMaO_R]V[YY[V",
	"L[LVNSPO SFL[ TFM[ OUQSSRTRVSVUUXUZV[ TRUSUUTXTZV[XZYY[V",
	"NVSLRMSNTMSL QROXOZQ[SZTYVV RRPXPZQ[",
	"NVSLRMSNTMSL QRKd RRO[M`KdJfHgGfGdHaJ_M]Q[TYVV",
	"LZLVNSPO SFL[ TFM[ URUSVSURTRRTOU OURVSZT[ OUQVRZT[U[XYZV",
	"NVNVPSRO UFOXOZQ[SZTYVV VFPXPZQ[",
	"E^EVGSIRKSKUI[ IRJSJUH[ KUMSORPRRSRUP[ PRQSQUO[ RUTSVRWRYSYUXXXZY[ WRXSXUWXWZY[[Z\\Y^V",
	"I[IVKSMROSOUM[ MRNSNUL[ OUQSSRTRVSVUUXUZV[ TRUSUUTXTZV[XZYY[V",
	"KYRRPRNSMTLVLXMZO[Q[SZTYUWUUTSRRQSQURWTXVXXWYV PRNTMVMYO[",
	"L[LVNSPO QLHg RLIg OUQSSRTRVSVUUXUZV[ TRUSUUTXTZV[XZYY[V",
	"K[UUTSRRPRNSMTLVLXMZO[Q[SZ PRNTMVMYO[ VRPdPfQgSfTcT[V[YY[V WRT[R`Pd",
	"LZLVNSPRRSRUP[ PRQSQUO[ RUTSVRWRVU VRVUWWXWZV",
	"NZNVPSQQQSTUUWUYTZR[ QSSUTWTYR[ NZP[U[XYZV",
	"NVNVPSRO UFOXOZQ[SZTYVV VFPXPZQ[ PNVN",
	"K[NRLXLZN[O[QZSXUU ORMXMZN[ VRTXTZV[XZYY[V WRUXUZV[",
	"KZNRMTLWLZN[O[RZTXUUUR ORNTMWMZN[ URVVWWXWZV",
	"H]LRJTIWIZK[L[NZPX MRKTJWJZK[ RRPXPZR[S[UZWXXUXR SRQXQZR[ XRYVZW[W]V",
	"JZJVLSNRPRQSQUPXOZM[L[KZKYLYKZ WSVTWTWSVRURSSRUQXQZR[U[XYZV QSRU SSQU PXQZ QXOZ",
	"K[NRLXLZN[O[QZSXUU ORMXMZN[ VRPd WRT[R`PdOfMgLfLdMaO_R]V[YY[V",
	"LYLVNSPRRRTSTVSXPZN[ RRSSSVRXPZ N[P\\Q^QaPdNfLgKfKdLaO^R\\VYYV N[O\\P^PaOdNf",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"NV",
	"JZ",
	"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF OGMJLOLRMWOZ NYQZSZVY UZWWXRXOWJUG VHSGQGNH",
	"H\\NJPISFS[ NJNKPJRHR[S[",
	"H\\LKLJMHNGPFTFVGWHXJXLWNUQL[ LKMKMJNHPGTGVHWJWLVNTQK[ LZYZY[ K[Y[",
	"H\\MFXFQO MFMGWG WFPO QNSNVOXQYTYUXXVZS[P[MZLYKWLW POSOVPXS TOWQXTXUWXTZ XVVYSZPZMYLW OZLX",
	"H\\UIU[V[ VFV[ VFKVZV UILV LUZUZV",
	"H\\MFLO NGMN MFWFWG NGWG MNPMSMVNXPYSYUXXVZS[P[MZLYKWLW LOMOONSNVOXR TNWPXSXUWXTZ XVVYSZPZMYLW OZLX",
	"H\\VGWIXIWGTFRFOGMJLOLTMXOZR[S[VZXXYUYTXQVOSNRNOOMQ WHTGRGOH PGNJMOMTNXQZ MVOYRZSZVYXV TZWXXUXTWQTO XSVPSOROOPMS QONQMT",
	"H\\KFYFO[ KFKGXG XFN[O[",
	"H\\PFMGLILKMMNNPOTPVQWRXTXWWYTZPZMYLWLTMRNQPPTOVNWMXKXIWGTFPF NGMIMKNMPNTOVPXRYTYWXYWZT[P[MZLYKWKTLRNPPOTNVMWKWIVG WHTGPGMH LXOZ UZXX",
	"H\\WPURRSQSNRLPKMKLLINGQFRFUGWIXMXRWWUZR[P[MZLXMXNZ WMVPSR WNUQRRQRNQLN PRMPLMLLMIPG LKNHQGRGUHWK SGVIWMWRVWTZ UYRZPZMY",
	"MXRXQYQZR[S[TZTYSXRX RYRZSZSYRY",
	"MXTZS[R[QZQYRXSXTYT\\S^Q_ RYRZSZSYRY S[T\\ TZS^",
	"MXRMQNQORPSPTOTNSMRM RNROSOSNRN RXQYQZR[S[TZTYSXRX RYRZSZSYRY",
	"MXRMQNQORPSPTOTNSMRM RNROSOSNRN TZS[R[QZQYRXSXTYT\\S^Q_ RYRZSZSYRY S[T\\ TZS^",
	"MXRFRTST RFSFST RXQYQZR[S[TZTYSXRX RYRZSZSYRY",
	"I\\LKLJMHNGQFTFWGXHYJYLXNWOUPRQ LKMKMJNHQGTGWHXJXLWNUORP MIPG UGXI XMTP RPRTSTSP RXQYQZR[S[TZTYSXRX RYRZSZSYRY",
	"MXTFRGQIQLRMSMTLTKSJRJQK RKRLSLSKRK RGQK QIRJ",
	"MXTHSIRIQHQGRFSFTGTJSLQM RGRHSHSGRG SITJ THSL",
	"F_\\MZMXNWPUVTXSYQZMZKYJWJUKSLRQOSMTKTISGQFPFNGMIMKNNPQUWXZZ[\\[ \\M\\NZNWP ZMXPVVUXSZQ[M[KZJYIWIUJSLQQNRMSKSIRG SHQGPGNH OGNINKONQQVWXYZZ\\Z\\[",
	"I\\RBR_S_ RBSBS_ WIYIWGTFQFNGLILKMMNNVRWSXUXWWYTZQZOYNX WIVHTGQGNHMIMKNMVQXSYUYWXYWZT[Q[NZLXNX XXUZ",
	"G^[BIbJb [B\\BJb",
	"KYUBSDQGOKNPNTOYQ]S`UbVb UBVBTDRGPKOPOTPYR]T`Vb",
	"KYNBPDRGTKUPUTTYR]P`NbOb NBOBQDSGUKVPVTUYS]Q`Ob",
	"JZRFQGSQRR RFRR RFSGQQRR MINIVOWO MIWO MIMJWNWO WIVINOMO WIMO WIWJMNMO",
	"F_JQ[Q[R JQJR[R",
	"F_RIRZSZ RISISZ JQ[Q[R JQJR[R",
	"F_JM[M[N JMJN[N JU[U[V JUJV[V",
	"NWSFRGRM SGRM SFTGRM",
	"I[NFMGMM NGMM NFOGMM WFVGVM WGVM WFXGVM",
	"KYQFOGNINKOMQNSNUMVKVIUGSFQF QFNIOMSNVKUGQF SFOGNKQNUMVISF",
	"F^ZIJRZ[ ZIZJLRZZZ[",
	"F^JIZRJ[ JIJJXRJZJ[",
	"G^OFObPb OFPFPb UFUbVb UFVFVb JP[P[Q JPJQ[Q JW[W[X JWJX[X",
	"F^[FYGVHSHPGNFLFJGIIIKKMMMOLPJPHNF [FH[I[ [F\\FI[ YTWTUUTWTYV[X[ZZ[X[VYT NFJGIKMMPJNF LFIIKMOLPHLF YTUUTYX[[XYT WTTWV[ZZ[VWT",
	"E`WMTKQKOLNMMOMRNTOUQVTVWT WMTLQLOMNONROTQUTUWT VKVSWUYVZV\\U]S]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[U[YZ VKWKWSXUZV YV[U\\S\\O[LZJYIWHTGQGNHLIKJJLIOIRJUKWLXNYQZUZYYYZ",
	"E_JPLONOPPSTTUVVXVZU[S[QZOXNVNTOSPPTNULUJT ZPXOVOTPQTPUNVLVJUISIQJOLNNNPOQPTTVUXUZT KOJQJSKU YUZSZQYO",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"NV",
	"JZ",
	"H]TFQGOIMLLOKSKVLYMZO[Q[TZVXXUYRZNZKYHXGVFTF TFRGPINLMOLSLVMYO[ Q[SZUXWUXRYNYKXHVF",
	"H]TJO[ VFP[ VFSIPKNL UIQKNL",
	"H]OJPKOLNKNJOHPGSFVFYGZIZKYMWOTQPSMUKWI[ VFXGYIYKXMVOPS JYKXMXRZUZWYXW MXR[U[WZXW",
	"H]OJPKOLNKNJOHPGSFVFYGZIZKYMVOSP VFXGYIYKXMVO QPSPVQWRXTXWWYVZS[O[LZKYJWJVKULVKW SPUQVRWTWWVYUZS[",
	"H]XGR[ YFS[ YFJUZU",
	"H]QFLP QF[F QGVG[F LPMOPNSNVOWPXRXUWXUZR[O[LZKYJWJVKULVKW SNUOVPWRWUVXTZR[",
	"H]YIXJYKZJZIYGWFTFQGOIMLLOKSKWLYMZO[R[UZWXXVXSWQVPTOQOOPMRLT TFRGPINLMOLSLXMZ R[TZVXWVWRVP",
	"H]NFLL [FZIXLSRQUPWO[ XLRRPUOWN[ MIPFRFWI NHPGRGWIYIZH[F",
	"H]SFPGOHNJNMOOQPTPXOYNZLZIYGVFSF SFQGPHOJOMPOQP TPWOXNYLYIXGVF QPMQKSJUJXKZN[R[VZWYXWXTWRVQTP QPNQLSKUKXLZN[ R[UZVYWWWSVQ",
	"H]YMXOVQTRQROQNPMNMKNIPGSFVFXGYHZJZNYRXUVXTZQ[N[LZKXKWLVMWLX OQNONKOIQGSF XGYIYNXRWUUXSZQ[",
	"MXPYOZP[QZPY",
	"MXP[OZPYQZQ[P]N_",
	"MXSMRNSOTNSM PYOZP[QZ",
	"MXSMRNSOTNSM P[OZPYQZQ[P]N_",
	"MXUFTGRS UGRS UFVGRS PYOZP[QZPY",
	"H]OJPKOLNKNJOHPGSFWFZG[I[KZMYNSPQQQSRTTT WFYGZIZKYMXNVO PYOZP[QZPY",
	"MXVFTHSJSKTLUKTJ",
	"MXUHTGUFVGVHUJSL",
	"E_\\N[O\\P]O]N\\M[MYNWPRXPZN[K[HZGXGVHTISKRPPROTMUKUITGRFPGOIOLPRQUSXUZW[Y[ZYZX K[IZHXHVITJSPP OLPQQTSWUYWZYZZY",
	"H]TBL_ YBQ_ ZJYKZL[K[JZHYGVFRFOGMIMKNMONVRXT MKOMVQWRXTXWWYVZS[O[LZKYJWJVKULVKW",
	"G]_BEb",
	"KZZBVESHQKOONTNXO]P`Qb VESIQMPPOUOZP_Qb",
	"JYSBTDUGVLVPUUSYQ\\N_Jb SBTEUJUOTTSWQ[N_",
	"J[TFTR OIYO YIOO",
	"E_IR[R",
	"E_RIR[ IR[R",
	"E_IO[O IU[U",
	"NWUFSM VFSM",
	"I[PFNM QFNM YFWM ZFWM",
	"KZSFQGPIPKQMSNUNWMXKXIWGUFSF",
	"F^ZIJRZ[",
	"F^JIZRJ[",
	"H]SFLb YFRb LQZQ KWYW",
	"E_^F\\GXHUHQGOFMFKGJIJKLMNMPLQJQHOF ^FF[ XTVTTUSWSYU[W[YZZXZVXT",
	"E`WNVLTKQKOLNMMPMSNUPVSVUUVS QKOMNPNSOUPV WKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX XKWSWUXV",
	"F_\\S[UYVWVUUTTQPPONNLNJOIQISJULVNVPUQTTPUOWNYN[O\\Q\\S",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"H\\RFK[ RFY[ RIX[ MUVU I[O[ U[[[",
	"G]LFL[ MFM[ IFYFYLXF MPUPXQYRZTZWYYXZU[I[ UPWQXRYTYWXYWZU[",
	"G]LFL[ MFM[ IFUFXGYHZJZLYNXOUP UFWGXHYJYLXNWOUP MPUPXQYRZTZWYYXZU[I[ UPWQXRYTYWXYWZU[",
	"I[NFN[ OFO[ KFZFZLYF K[R[",
	"F^NFNLMTLXKZJ[ XFX[ YFY[ KF\\F G[\\[ G[Gb H[Gb [[\\b \\[\\b",
	"G\\LFL[ MFM[ SLST IFYFYLXF MPSP I[Y[YUX[",
	"CbRFR[ SFS[ OFVF GGHHGIFHFGGFHFIGJIKMLONPWPYOZM[I\\G]F^F_G_H^I]H^G NPLQKSJXIZH[ NPMQLSKXJZI[G[FZEX WPYQZS[X\\Z][ WPXQYSZX[Z\\[^[_Z`X O[V[",
	"H\\LIKFKLLINGPFTFWGXIXLWNTOQO TFVGWIWLVNTO TOVPXRYTYWXYWZT[O[MZLYKWKVLUMVLW WQXTXWWYVZT[",
	"F^KFK[ LFL[ XFX[ YFY[ HFOF UF\\F XHLY H[O[ U[\\[",
	"F^KFK[ LFL[ XFX[ YFY[ HFOF UF\\F XHLY H[O[ U[\\[ N@N?M?M@NBPCTCVBW@",
	"F^KFK[ LFL[ HFOF LPSPUOVMWIXGYFZF[G[HZIYHZG SPUQVSWXXZY[ SPTQUSVXWZX[Z[[Z\\X H[O[",
	"E^MFMLLTKXJZI[H[GZGYHXIYHZ XFX[ YFY[ JF\\F U[\\[",
	"F_KFK[ LFRX KFR[ YFR[ YFY[ ZFZ[ HFLF YF]F H[N[ V[][",
	"F^KFK[ LFL[ XFX[ YFY[ HFOF UF\\F LPXP H[O[ U[\\[",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF QFOGMILKKOKRLVMXOZQ[ S[UZWXXVYRYOXKWIUGSF",
	"F^KFK[ LFL[ XFX[ YFY[ HF\\F H[O[ U[\\[",
	"G]LFL[ MFM[ IFUFXGYHZJZMYOXPUQMQ UFWGXHYJYMXOWPUQ I[P[",
	"G\\XIYLYFXIVGSFQFNGLIKKJNJSKVLXNZQ[S[VZXXYV QFOGMILKKNKSLVMXOZQ[",
	"I\\RFR[ SFS[ LFKLKFZFZLYF O[V[",
	"H]KFRV LFSV ZFSVQYPZN[M[LZLYMXNYMZ IFOF VF\\F",
	"F_RFR[ SFS[ OFVF PILJJLIOIRJULWPXUXYW[U\\R\\O[LYJUIPI PIMJKLJOJRKUMWPX UXXWZU[R[OZLXJUI O[V[",
	"H\\KFX[ LFY[ YFK[ IFOF UF[F I[O[ U[[[",
	"F^KFK[ LFL[ XFX[ YFY[ HFOF UF\\F H[\\[ [[\\b \\[\\b",
	"F]KFKQLSOTRTUSWQ LFLQMSOT WFW[ XFX[ HFOF TF[F T[[[",
	"BcGFG[ HFH[ RFR[ SFS[ ]F][ ^F^[ DFKF OFVF ZFaF D[a[",
	"BcGFG[ HFH[ RFR[ SFS[ ]F][ ^F^[ DFKF OFVF ZFaF D[a[ `[ab a[ab",
	"F`PFP[ QFQ[ IFHLHFTF QPXP[Q\\R]T]W\\Y[ZX[M[ XPZQ[R\\T\\W[YZZX[",
	"CaHFH[ IFI[ EFLF IPPPSQTRUTUWTYSZP[E[ PPRQSRTTTWSYRZP[ [F[[ \\F\\[ XF_F X[_[",
	"H]MFM[ NFN[ JFQF NPUPXQYRZTZWYYXZU[J[ UPWQXRYTYWXYWZU[",
	"H]LIKFKLLINGQFSFVGXIYKZNZSYVXXVZS[P[MZLYKWKVLUMVLW SFUGWIXKYNYSXVWXUZS[ PPYP",
	"CbHFH[ IFI[ EFLF E[L[ VFSGQIPKOOORPVQXSZV[X[[Z]X^V_R_O^K]I[GXFVF VFTGRIQKPOPRQVRXTZV[ X[ZZ\\X]V^R^O]K\\IZGXF IPOP",
	"G]WFW[ XFX[ [FOFLGKHJJJLKNLOOPWP OFMGLHKJKLLNMOOP RPPQORLYKZJZIY PQOSMZL[J[IYIX T[[[",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"I]NONPMPMONNPMTMVNWOXQXXYZZ[ WOWXXZZ[[[ WQVRPSMTLVLXMZP[S[UZWX PSNTMVMXNZP[",
	"H\\XFWGQINKLNKQKULXNZQ[S[VZXXYUYSXPVNSMQMNNLPKS XFWHUIQJNLLN QMONMPLSLUMXOZQ[ S[UZWXXUXSWPUNSM",
	"H\\MMM[ NMN[ JMUMXNYPYQXSUT UMWNXPXQWSUT NTUTXUYWYXXZU[J[ UTWUXWXXWZU[",
	"HZMMM[ NMN[ JMXMXRWM J[Q[",
	"F]NMNQMWLZK[ WMW[ XMX[ KM[M I[H`H[[[[`Z[",
	"H[LSXSXQWOVNTMQMNNLPKSKULXNZQ[S[VZXX WSWPVN QMONMPLSLUMXOZQ[",
	"E`RMR[ SMS[ OMVM JNIOHNIMJMKNMRNSPTUTWSXRZN[M\\M]N\\O[N PTNUMVKZJ[ PTNVLZK[I[HZGX UTWUXVZZ[[ UTWVYZZ[\\[]Z^X O[V[",
	"I[MOLMLQMONNPMTMWNXPXQWSTT TMVNWPWQVSTT QTTTWUXWXXWZT[P[MZLXLWMVNWMX TTVUWWWXVZT[",
	"G]LML[ MMM[ WMW[ XMX[ IMPM TM[M I[P[ T[[[ WNMZ",
	"G]LML[ MMM[ WMW[ XMX[ IMPM TM[M I[P[ T[[[ WNMZ OGOFNFNGOIQJSJUIVG",
	"H\\MMM[ NMN[ JMQM NTPTSSTRVNWMXMYNXOWN PTSUTVVZW[ PTRUSVUZV[X[YZZX J[Q[",
	"G]NMNQMWLZK[J[IZJYKZ WMW[ XMX[ KM[M T[[[",
	"G^LML[ LMR[ MMRY XMR[ XMX[ YMY[ IMMM XM\\M I[O[ U[\\[",
	"G]LML[ MMM[ WMW[ XMX[ IMPM TM[M MTWT I[P[ T[[[",
	"H\\QMNNLPKSKULXNZQ[S[VZXXYUYSXPVNSMQM QMONMPLSLUMXOZQ[ S[UZWXXUXSWPUNSM",
	"G]LML[ MMM[ WMW[ XMX[ IM[M I[P[ T[[[",
	"G\\LMLb MMMb MPONQMSMVNXPYSYUXXVZS[Q[OZMX SMUNWPXSXUWXUZS[ IMMM IbPb",
	"H[WPVQWRXQXPVNTMQMNNLPKSKULXNZQ[S[VZXX QMONMPLSLUMXOZQ[",
	"I\\RMR[ SMS[ MMLRLMYMYRXM O[V[",
	"I[LMR[ MMRY XMR[P_NaLbKbJaK`La JMPM TMZM",
	"H]RFRb SFSb OFSF RPQNPMNMLNKQKWLZN[P[QZRX NMMNLQLWMZN[ WMXNYQYWXZW[ SPTNUMWMYNZQZWYZW[U[TZSX ObVb",
	"H\\LMW[ MMX[ XML[ JMPM TMZM J[P[ T[Z[",
	"G]LML[ MMM[ WMW[ XMX[ IMPM TM[M I[[[[`Z[",
	"G]LMLTMVPWRWUVWT MMMTNVPW WMW[ XMX[ IMPM TM[M T[[[",
	"CbHMH[ IMI[ RMR[ SMS[ \\M\\[ ]M][ EMLM OMVM YM`M E[`[",
	"CbHMH[ IMI[ RMR[ SMS[ \\M\\[ ]M][ EMLM OMVM YM`M E[`[``_[",
	"H]QMQ[ RMR[ LMKRKMUM RTVTYUZWZXYZV[N[ VTXUYWYXXZV[",
	"E_JMJ[ KMK[ GMNM KTOTRUSWSXRZO[G[ OTQURWRXQZO[ YMY[ ZMZ[ VM]M V[][",
	"J[OMO[ PMP[ LMSM PTTTWUXWXXWZT[L[ TTVUWWWXVZT[",
	"I\\MOLMLQMONNPMSMVNXPYSYUXXVZS[P[NZLXLWMVNWMX SMUNWPXSXUWXUZS[ RTXT",
	"DaIMI[ JMJ[ FMMM F[M[ VMSNQPPSPUQXSZV[X[[Z]X^U^S]P[NXMVM VMTNRPQSQURXTZV[ X[ZZ\\X]U]S\\PZNXM JTPT",
	"G\\VMV[ WMW[ ZMOMLNKPKQLSOTVT OMMNLPLQMSOT TTQUPVNZM[ TTRUQVOZN[L[KZJX S[Z[",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"H\\RFKZ QIW[ RIX[ RFY[ MUVU I[O[ T[[[ KZJ[ KZM[ WZU[ WYV[ XYZ[",
	"G]LFL[ MGMZ NFN[ IFUFXGYHZJZLYNXOUP XHYJYLXN UFWGXIXMWOUP NPUPXQYRZTZWYYXZU[I[ XRYTYWXY UPWQXSXXWZU[ JFLG KFLH OFNH PFNG LZJ[ LYK[ NYO[ NZP[",
	"G\\XIYFYLXIVGTFQFNGLIKKJNJSKVLXNZQ[T[VZXXYV MILKKNKSLVMX QFOGMJLNLSMWOZQ[",
	"G]LFL[ MGMZ NFN[ IFSFVGXIYKZNZSYVXXVZS[I[ WIXKYNYSXVWX SFUGWJXNXSWWUZS[ JFLG KFLH OFNH PFNG LZJ[ LYK[ NYO[ NZP[",
	"G\\LFL[ MGMZ NFN[ IFYFYL NPTP TLTT I[Y[YU JFLG KFLH OFNH PFNG TFYG VFYH WFYI XFYL TLSPTT TNRPTR TOPPTQ LZJ[ LYK[ NYO[ NZP[ T[YZ V[YY W[YX X[YU",
	"G[LFL[ MGMZ NFN[ IFYFYL NPTP TLTT I[Q[ JFLG KFLH OFNH PFNG TFYG VFYH WFYI XFYL TLSPTT TNRPTR TOPPTQ LZJ[ LYK[ NYO[ NZP[",
	"G^XIYFYLXIVGTFQFNGLIKKJNJSKVLXNZQ[T[VZXZY[YS MILKKNKSLVMX QFOGMJLNLSMWOZQ[ XTXY WSWYVZ TS\\S USWT VSWU ZSYU [SYT",
	"F^KFK[ LGLZ MFM[ WFW[ XGXZ YFY[ HFPF TF\\F MPWP H[P[ T[\\[ IFKG JFKH NFMH OFMG UFWG VFWH ZFYH [FYG KZI[ KYJ[ MYN[ MZO[ WZU[ WYV[ YYZ[ YZ[[",
	"LXQFQ[ RGRZ SFS[ NFVF N[V[ OFQG PFQH TFSH UFSG QZO[ QYP[ SYT[ SZU[",
	"JZSFSWRZQ[ TGTWSZ UFUWTZQ[O[MZLXLVMUNUOVOWNXMX MVMWNWNVMV PFXF QFSG RFSH VFUH WFUG",
	"F\\KFK[ LGLZ MFM[ XGMR PPW[ QPX[ QNY[ HFPF UF[F H[P[ T[[[ IFKG JFKH NFMH OFMG WFXG ZFXG KZI[ KYJ[ MYN[ MZO[ WYU[ WYZ[",
	"I[NFN[ OGOZ PFP[ KFSF K[Z[ZU LFNG MFNH QFPH RFPG NZL[ NYM[ PYQ[ PZR[ U[ZZ W[ZY X[ZX Y[ZU",
	"E_JFJZ JFQ[ KFQX LFRX XFQ[ XFX[ YGYZ ZFZ[ GFLF XF]F G[M[ U[][ HFJG [FZH \\FZG JZH[ JZL[ XZV[ XYW[ ZY[[ ZZ\\[",
	"F^KFKZ KFY[ LFXX MFYX YGY[ HFMF VF\\F H[N[ IFKG WFYG [FYG KZI[ KZM[",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF MILKKNKSLVMX WXXVYSYNXKWI QFOGMJLNLSMWOZQ[ S[UZWWXSXNWJUGSF",
	"G]LFL[ MGMZ NFN[ IFUFXGYHZJZMYOXPUQNQ XHYJYMXO UFWGXIXNWPUQ I[Q[ JFLG KFLH OFNH PFNG LZJ[ LYK[ NYO[ NZP[",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF MILKKNKSLVMX WXXVYSYNXKWI QFOGMJLNLSMWOZQ[ S[UZWWXSXNWJUGSF NXOVQURUTVUXV^W`Y`Z^Z\\ V\\W^X_Y_ UXW]X^Y^Z]",
	"G]LFL[ MGMZ NFN[ IFUFXGYHZJZLYNXOUPNP XHYJYLXN UFWGXIXMWOUP RPTQUSWYX[Z[[Y[W WWXYYZZZ TQURXXYYZY[X I[Q[ JFLG KFLH OFNH PFNG LZJ[ LYK[ NYO[ NZP[",
	"H\\XIYFYLXIVGSFPFMGKIKLLNOPURWSXUXXWZ LLMNOOUQWRXT MGLILKMMONUPXRYTYWXYWZT[Q[NZLXKUK[LX",
	"H\\JFJL QFQ[ RGRZ SFS[ ZFZL JFZF N[V[ KFJL LFJI MFJH OFJG UFZG WFZH XFZI YFZL QZO[ QYP[ SYT[ SZU[",
	"F^KFKULXNZQ[S[VZXXYUYG LGLVMX MFMVNYOZQ[ HFPF VF\\F IFKG JFKH NFMH OFMG WFYG [FYG",
	"H\\KFR[ LFRXR[ MFSX YGR[ IFPF UF[F JFLH NFMH OFMG WFYG ZFYG",
	"F^JFN[ KFNVN[ LFOV RFOVN[ RFV[ SFVVV[ TFWV ZGWVV[ GFOF RFTF WF]F HFKG IFKH MFLH NFLG XFZG \\FZG",
	"H\\KFW[ LFX[ MFY[ XGLZ IFPF UF[F I[O[ T[[[ JFMH NFMH OFMG VFXG ZFXG LZJ[ LZN[ WZU[ WYV[ WYZ[",
	"G]JFQQQ[ KFRQRZ LFSQS[ YGSQ HFOF VF\\F N[V[ IFKG NFLG WFYG [FYG QZO[ QYP[ SYT[ SZU[",
	"H\\YFKFKL WFK[ XFL[ YFM[ K[Y[YU LFKL MFKI NFKH PFKG T[YZ V[YY W[YX X[YU",
	"H\\RFKZ QIW[ RIX[ RFY[ MUVU I[O[ T[[[ KZJ[ KZM[ WZU[ WYV[ XYZ[",
	"G]LFL[ MGMZ NFN[ IFUFXGYHZJZLYNXOUP XHYJYLXN UFWGXIXMWOUP NPUPXQYRZTZWYYXZU[I[ XRYTYWXY UPWQXSXXWZU[ JFLG KFLH OFNH PFNG LZJ[ LYK[ NYO[ NZP[",
	"I[NFN[ OGOZ PFP[ KFZFZL K[S[ LFNG MFNH QFPH RFPG UFZG WFZH XFZI YFZL NYM[ NZL[ PYQ[ PZR[",
	"H\\RFJ[ QIX[ RIY[ RFZ[ KYXY KZXZ J[Z[",
	"G\\LFL[ MGMZ NFN[ IFYFYL NPTP TLTT I[Y[YU JFLG KFLH OFNH PFNG TFYG VFYH WFYI XFYL TLSPTT TNRPTR TOPPTQ LZJ[ LYK[ NYO[ NZP[ T[YZ V[YY W[YX X[YU",
	"H\\YFKFKL WFK[ XFL[ YFM[ K[Y[YU LFKL MFKI NFKH PFKG T[YZ V[YY W[YX X[YU",
	"F^KFK[ LGLZ MFM[ WFW[ XGXZ YFY[ HFPF TF\\F MPWP H[P[ T[\\[ IFKG JFKH NFMH OFMG UFWG VFWH ZFYH [FYG KZI[ KYJ[ MYN[ MZO[ WZU[ WYV[ YYZ[ YZ[[",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF MILKKNKSLVMX WXXVYSYNXKWI QFOGMJLNLSMWOZQ[ S[UZWWXSXNWJUGSF OMOT UMUT OPUP OQUQ ONPP OOQP UNTP UOSP PQOS QQOR SQUR TQUS",
	"LXQFQ[ RGRZ SFS[ NFVF N[V[ OFQG PFQH TFSH UFSG QZO[ QYP[ SYT[ SZU[",
	"F\\KFK[ LGLZ MFM[ XGMR PPW[ QPX[ QNY[ HFPF UF[F H[P[ T[[[ IFKG JFKH NFMH OFMG WFXG ZFXG KZI[ KYJ[ MYN[ MZO[ WYU[ WYZ[",
	"H\\RFKZ QIW[ RIX[ RFY[ I[O[ T[[[ KZJ[ KZM[ WZU[ WYV[ XYZ[",
	"E_JFJZ JFQ[ KFQX LFRX XFQ[ XFX[ YGYZ ZFZ[ GFLF XF]F G[M[ U[][ HFJG [FZH \\FZG JZH[ JZL[ XZV[ XYW[ ZY[[ ZZ\\[",
	"F^KFKZ KFY[ LFXX MFYX YGY[ HFMF VF\\F H[N[ IFKG WFYG [FYG KZI[ KZM[",
	"G]JEJL ZEZL OMOT UMUT JUJ\\ ZUZ\\ JGZG JHZH JIZI OPUP OQUQ JXZX JYZY JZZZ JFMH ZFWH KIJK LIJJ XIZJ YIZK ONPP OOQP UNTP UOSP PQOS QQOR SQUR TQUS JVKX JWLX ZWXX ZVYX MYJ[ WYZ[",
	"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF MILKKNKSLVMX WXXVYSYNXKWI QFOGMJLNLSMWOZQ[ S[UZWWXSXNWJUGSF",
	"F^KFK[ LGLZ MFM[ WFW[ XGXZ YFY[ HF\\F H[P[ T[\\[ IFKG JFKH NFMH OFMG UFWG VFWH ZFYH [FYG KZI[ KYJ[ MYN[ MZO[ WZU[ WYV[ YYZ[ YZ[[",
	"G]LFL[ MGMZ NFN[ IFUFXGYHZJZMYOXPUQNQ XHYJYMXO UFWGXIXNWPUQ I[Q[ JFLG KFLH OFNH PFNG LZJ[ LYK[ NYO[ NZP[",
	"G]IFPPQQ JFQP KFRPI[ IFYFZLYIWF VFYH TFYG KYYY JZYZ I[Y[ZUYXWY",
	"H\\JFJL QFQ[ RGRZ SFS[ ZFZL JFZF N[V[ KFJL LFJI MFJH OFJG UFZG WFZH XFZI YFZL QZO[ QYP[ SYT[ SZU[",
	"H\\JMKILGMFOFPGQIRM LHMGOGPH JMKJMHOHPIQMQ[ RMR[ ZMYJWHUHTISMS[ XHWGUGTH ZMYIXGWFUFTGSIRM N[V[ QYP[ QZO[ SZU[ SYT[",
	"G]QFQ[ RGRZ SFS[ NFVF N[V[ OFQG PFQH TFSH UFSG QZO[ QYP[ SYT[ SZU[ OKLLKMJOJRKTLUOVUVXUYTZRZOYMXLUKOK LMKOKRLT XTYRYOXM OKMLLOLRMUOV UVWUXRXOWLUK",
	"H\\KFW[ LFX[ MFY[ XGLZ IFPF UF[F I[O[ T[[[ JFMH NFMH OFMG VFXG ZFXG LZJ[ LZN[ WZU[ WYV[ WYZ[",
	"F^QFQ[ RGRZ SFS[ NFVF N[V[ OFQG PFQH TFSH UFSG QZO[ QYP[ SYT[ SZU[ HMIMJNKQLSMTPUTUWTXSYQZN[M\\M LRKNJLILKN HMIKJKKLLPMSNTPU YN[LZLYNXR TUVTWSXPYLZK[K\\M",
	"G]NYKYJWK[O[MVKRJOJLKIMGPFTFWGYIZLZOYRWVU[Y[ZWYYVY LSKOKLLI XIYLYOXS O[MULPLKMHNGPF TFVGWHXKXPWUU[ KZNZ VZYZ",
	"H\\UFIZ SJT[ THUZ UFUHVYV[ LUTU F[L[ Q[X[ IZG[ IZK[ TZR[ TYS[ VYW[",
	"F^OFI[ PFJ[ QFK[ LFWFZG[I[KZNYOVP YGZIZKYNXO WFXGYIYKXNVP NPVPXQYSYUXXVZR[F[ WQXSXUWXUZ VPWRWUVXTZR[ MFPG NFOH RFPH SFPG JZG[ JYH[ KYL[ JZM[",
	"H]ZH[H\\F[L[JZHYGWFTFQGOIMLLOKSKVLYMZP[S[UZWXXV QHOJNLMOLSLWMY TFRGPJOLNOMSMXNZP[",
	"F]OFI[ PFJ[ QFK[ LFUFXGYHZKZOYSWWUYSZO[F[ WGXHYKYOXSVWTY UFWHXKXOWSUWRZO[ MFPG NFOH RFPH SFPG JZG[ JYH[ KYL[ JZM[",
	"F]OFI[ PFJ[ QFK[ ULST LF[FZL NPTP F[U[WV MFPG NFOH RFPH SFPG WFZG XFZH YFZI ZFZL ULSPST TNRPSR TOQPSQ JZG[ JYH[ KYL[ JZM[ P[UZ R[UY UYWV",
	"F\\OFI[ PFJ[ QFK[ ULST LF[FZL NPTP F[N[ MFPG NFOH RFPH SFPG WFZG XFZH YFZI ZFZL ULSPST TNRPSR TOQPSQ JZG[ JYH[ KYL[ JZM[",
	"H^ZH[H\\F[L[JZHYGWFTFQGOIMLLOKSKVLYMZP[R[UZWXYT QHOJNLMOLSLWMY VXWWXT TFRGPJOLNOMSMXNZP[ R[TZVWWT TT\\T UTWU VTWW ZTXV [TXU",
	"E_NFH[ OFI[ PFJ[ ZFT[ [FU[ \\FV[ KFSF WF_F LPXP E[M[ Q[Y[ LFOG MFNH QFOH RFOG XF[G YFZH ]F[H ^F[G IZF[ IYG[ JYK[ IZL[ UZR[ UYS[ VYW[ UZX[",
	"KYTFN[ UFO[ VFP[ QFYF K[S[ RFUG SFTH WFUH XFUG OZL[ OYM[ PYQ[ OZR[",
	"I\\WFRWQYO[ XFTSSVRX YFUSSXQZO[M[KZJXJVKULUMVMWLXKX KVKWLWLVKV TF\\F UFXG VFWH ZFXH [FXG",
	"F]OFI[ PFJ[ QFK[ \\GMR QOU[ ROV[ SNWZ LFTF YF_F F[N[ R[Y[ MFPG NFOH RFPH SFPG ZF\\G ^F\\G JZG[ JYH[ KYL[ JZM[ UZS[ UYT[ VYX[",
	"H\\QFK[ RFL[ SFM[ NFVF H[W[YU OFRG PFQH TFRH UFRG LZI[ LYJ[ MYN[ LZO[ R[WZ T[XX V[YU",
	"D`MFGZ MGNYN[ NFOY OFPX [FPXN[ [FU[ \\FV[ ]FW[ JFOF [F`F D[J[ R[Z[ KFMG LFMH ^F\\H _F\\G GZE[ GZI[ VZS[ VYT[ WYX[ VZY[",
	"F_OFIZ OFV[ PFVX QFWX \\GWXV[ LFQF YF_F F[L[ MFPG NFPH ZF\\G ^F\\G IZG[ IZK[",
	"G]SFPGNILLKOJSJVKYLZN[Q[TZVXXUYRZNZKYHXGVFSF OIMLLOKSKWLY UXWUXRYNYJXH SFQGOJNLMOLSLXMZN[ Q[SZUWVUWRXNXIWGVF",
	"F]OFI[ PFJ[ QFK[ LFXF[G\\I\\K[NYPUQMQ ZG[I[KZNXP XFYGZIZKYNWPUQ F[N[ MFPG NFOH RFPH SFPG JZG[ JYH[ KYL[ JZM[",
	"G]SFPGNILLKOJSJVKYLZN[Q[TZVXXUYRZNZKYHXGVFSF OIMLLOKSKWLY UXWUXRYNYJXH SFQGOJNLMOLSLXMZN[ Q[SZUWVUWRXNXIWGVF LXMVOUPURVSXT]U^V^W] T^U_V_ SXS_T`V`W]W\\",
	"F^OFI[ PFJ[ QFK[ LFWFZG[I[KZNYOVPNP YGZIZKYNXO WFXGYIYKXNVP RPTQURWXXYYYZX WYXZYZ URVZW[Y[ZXZW F[N[ MFPG NFOH RFPH SFPG JZG[ JYH[ KYL[ JZM[",
	"G^ZH[H\\F[L[JZHYGVFRFOGMIMLNNPPVSWUWXVZ NLONVRWT OGNINKOMUPWRXTXWWYVZS[O[LZKYJWJUI[JYKY",
	"G]TFN[ UFO[ VFP[ MFKL ]F\\L MF]F K[S[ NFKL PFLI RFMG YF\\G ZF\\H [F\\I \\F\\L OZL[ OYM[ PYQ[ OZR[",
	"F_NFKQJUJXKZN[R[UZWXXU\\G OFLQKUKYLZ PFMQLULYN[ KFSF YF_F LFOG MFNH QFOH RFOG ZF\\G ^F\\G",
	"H\\NFNHOYO[ OGPX PFQW [GO[ LFSF XF^F MFNH QFPH RFOG YF[G ]F[G",
	"E_MFMHKYK[ NGLX OFMW UFMWK[ UFUHSYS[ VGTX WFUW ]GUWS[ JFRF UFWF ZF`F KFNG LFMH PFNI QFNG [F]G _F]G",
	"G]NFT[ OFU[ PFV[ [GIZ LFSF XF^F F[L[ Q[X[ MFOH QFPH RFPG YF[G ]F[G IZG[ IZK[ TZR[ TYS[ UYW[",
	"G]MFQPN[ NFRPO[ OFSPP[ \\GSP KFRF YF_F K[S[ LFNG PFOH QFNG ZF\\G ^F\\G OZL[ OYM[ PYQ[ OZR[",
	"G]ZFH[ [FI[ \\FJ[ \\FNFLL H[V[XU OFLL PFMI RFNG R[VZ T[WX U[XU",
	"",
	"",
	"",
	"",
	"",
	"",
	"H\\JFR[ KFRX LFSX JFZFR[ LGYG LHYH",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"I]NPNOOOOQMQMONNPMTMVNWOXQXXYZZ[ VOWQWXXZ TMUNVPVXWZZ[[[ VRUSPTMULWLXMZP[S[UZVX NUMWMXNZ USQTOUNWNXOZP[",
	"G\\LFL[MZOZ MGMY IFNFNZ NPONQMSMVNXPYSYUXXVZS[Q[OZNX WPXRXVWX SMUNVOWRWVVYUZS[ JFLG KFLH",
	"H[WQWPVPVRXRXPVNTMQMNNLPKSKULXNZQ[S[VZXX MPLRLVMX QMONNOMRMVNYOZQ[",
	"H]VFV[[[ WGWZ SFXFX[ VPUNSMQMNNLPKSKULXNZQ[S[UZVX MPLRLVMX QMONNOMRMVNYOZQ[ TFVG UFVH XYY[ XZZ[",
	"H[MSXSXQWOVNSMQMNNLPKSKULXNZQ[S[VZXX WRWQVO MPLRLVMX VSVPUNSM QMONNOMRMVNYOZQ[",
	"KYWHWGVGVIXIXGWFTFRGQHPKP[ RHQKQZ TFSGRIR[ MMVM M[U[ PZN[ PYO[ RYS[ RZT[",
	"I\\XNYOZNYMXMVNUO QMONNOMQMSNUOVQWSWUVVUWSWQVOUNSMQM OONQNSOU UUVSVQUO QMPNOPOTPVQW SWTVUTUPTNSM NUMVLXLYM[N\\Q]U]X^Y_ N[Q\\U\\X] LYMZP[U[X\\Y^Y_XaUbObLaK_K^L\\O[ ObMaL_L^M\\O[",
	"G^LFL[ MGMZ IFNFN[ NQOOPNRMUMWNXOYRY[ WOXRXZ UMVNWQW[ I[Q[ T[\\[ JFLG KFLH LZJ[ LYK[ NYO[ NZP[ WZU[ WYV[ YYZ[ YZ[[",
	"LXQFQHSHSFQF RFRH QGSG QMQ[ RNRZ NMSMS[ N[V[ OMQN PMQO QZO[ QYP[ SYT[ SZU[",
	"KXRFRHTHTFRF SFSH RGTG RMR^QaPb SNS]R` OMTMT]S`RaPbMbLaL_N_NaMaM` PMRN QMRO",
	"G]LFL[ MGMZ IFNFN[ WNNW RSY[ RTX[ QTW[ TM[M I[Q[ T[[[ JFLG KFLH UMWN ZMWN LZJ[ LYK[ NYO[ NZP[ WYU[ VYZ[",
	"LXQFQ[ RGRZ NFSFS[ N[V[ OFQG PFQH QZO[ QYP[ SYT[ SZU[",
	"AcFMF[ GNGZ CMHMH[ HQIOJNLMOMQNROSRS[ QORRRZ OMPNQQQ[ SQTOUNWMZM\\N]O^R^[ \\O]R]Z ZM[N\\Q\\[ C[K[ N[V[ Y[a[ DMFN EMFO FZD[ FYE[ HYI[ HZJ[ QZO[ QYP[ SYT[ SZU[ \\ZZ[ \\Y[[ ^Y_[ ^Z`[",
	"G^LML[ MNMZ IMNMN[ NQOOPNRMUMWNXOYRY[ WOXRXZ UMVNWQW[ I[Q[ T[\\[ JMLN KMLO LZJ[ LYK[ NYO[ NZP[ WZU[ WYV[ YYZ[ YZ[[",
	"H\\QMNNLPKSKULXNZQ[S[VZXXYUYSXPVNSMQM MPLRLVMX WXXVXRWP QMONNOMRMVNYOZQ[ S[UZVYWVWRVOUNSM",
	"G\\LMLb MNMa IMNMNb NPONQMSMVNXPYSYUXXVZS[Q[OZNX WPXRXVWX SMUNVOWRWVVYUZS[ IbQb JMLN KMLO LaJb L`Kb N`Ob NaPb",
	"H\\VNVb WOWa UNWNXMXb VPUNSMQMNNLPKSKULXNZQ[S[UZVX MPLRLVMX QMONNOMRMVNYOZQ[ Sb[b VaTb V`Ub X`Yb XaZb",
	"IZNMN[ ONOZ KMPMP[ WOWNVNVPXPXNWMUMSNQPPS K[S[ LMNN MMNO NZL[ NYM[ PYQ[ PZR[",
	"J[WOXMXQWOVNTMPMNNMOMQNSPTUUWVXY NNMQ NRPSUTWU XVWZ MONQPRUSWTXVXYWZU[Q[OZNYMWM[NY",
	"KZPHPVQYRZT[V[XZYX QHQWRY PHRFRWSZT[ MMVM",
	"G^LMLVMYNZP[S[UZVYWW MNMWNY IMNMNWOZP[ WMW[\\[ XNXZ TMYMY[ JMLN KMLO YYZ[ YZ[[",
	"I[LMR[ MMRY NMSY XNSYR[ JMQM TMZM KMNO PMNN VMXN YMXN",
	"F^JMN[ KMNX LMOX RMOXN[ RMV[ SMVX RMTMWX ZNWXV[ GMOM WM]M HMKN NMLN XMZN \\MZN",
	"H\\LMV[ MMW[ NMX[ WNMZ JMQM TMZM J[P[ S[Z[ KMMN PMNN UMWN YMWN MZK[ MZO[ VZT[ WZY[",
	"H[LMR[ MMRY NMSY XNSYP_NaLbJbIaI_K_KaJaJ` JMQM TMZM KMNO PMNN VMXN YMXN",
	"I[VML[ WMM[ XMN[ XMLMLQ L[X[XW MMLQ NMLP OMLO QMLN S[XZ U[XY V[XX W[XW",
	"G^[MZQYTWXUZR[P[MZKXJUJSKPMNPMRMUNVOWQYXZZ[[\\[ ZMYQXTWVUYTZR[ LXKVKRLP P[NZMYLVLRMONNPM RMTNUOVQXXYZ[[",
	"G\\QFNGMHLJKNKb NHMJLNLa QFOGNIMNMb QFSFVGWHXJXLWNVOSP PPTPWQXRYTYWXYWZT[Q[OZNYMW VHWJWLVN WRXTXWWY SFUGVIVMUOSP TPVQWSWXVZT[ KbMb",
	"F\\HRINKMMMONPOQRRYSb IOKNMNOOPP HRIPKOMOOPPQQTRYRa XMWPVRTUSWR[Qb YMWQ ZMYOWRTVSXR[ XMZM QbSb",
	"H\\SMQMNNLPKSKULXNZQ[S[VZXXYUYSXPVNSMPLNKMJMHNGPFSFWH MPLSLUMX WXXUXSWP QMONNOMRMVNYOZQ[ S[UZVYWVWRVOUNOKNJNIOHQGTGWH",
	"I[SMUNVOWOVNSMQMMNLOLQMRQS SSQSMTKVKXMZP[S[VZXXWXVZ NNMOMQNR MULVLXMY QMONNONQORQS QSNTMVMXNZP[",
	"I[QHRGRFQFPGPIQJTKXKYKYJXJUKSLPNNPMRLULWMYNZP[S\\U]V_VaUbSbRaR`S`Sa POOPNRMUMWNYOZ UKRMQNOQNTNWOYQ[S\\",
	"G]JMKNLPL[ KMLNMPMZ HPINJMLMMNNPN[ UMVNWQWb WOXRXa NQOOPNRMUMWNXOYRYb L[N[ WbYb",
	"F]IMJNKPKTLWMYNZQ[S[VZWYXWYRYOXJVGTFRFPGOIOKPMSOVP[Q JMKNLPLTMWNY VYWWXRXOWJVHTG GPHNIMKMLNMPMTNXOZQ[ S[UZVXWSWNVJUHSGQGOI",
	"KZNMONPPPXQZS[U[WZXX OMPNQPQXRZ LPMNNMPMQNRPRXSZT[",
	"G]JMKNLPL[ KMLNMPMZ HPINJMLMMNNPN[ SOUNWNXOXPZPZNXMVMTNQQOTNW XNYOYP PSQSWYYYZX TWWZYZ RTUZV[X[YZZX L[N[",
	"H\\JGKFMFOGQIXXYZZ[ OHPIWXXY MFNGOIVXXZZ[[[ RMJZJ[K[RM",
	"G]KMKb LNLa MMMb VMVXWZX[Z[[Z\\X WNWXXZY[ XMXXYZZ[ MXNZP[R[TZUYVW KMMM VMXM KbMb",
	"G]JMKNLPMTN[ KMLNMPNTOZ HPINJMLMMNNPOTPZ VVWTXQXMYMZNYQXSVVTXQZN[ XRYOYM",
	"JZPGSFRFPGOHOIPJSKVLWKVJSKPLNMMOMQNRPSSTVUWTVSSTOUMVLXLZM[O\\S]U^V_VaTbRbOaPaRb OMNONQOR NVMXMZN[ VKSKQLPMOOOQQSST VTSTPUOVNXNZP\\S]",
	"H\\QMNNLPKSKULXNZQ[S[VZXXYUYSXPVNSMQM MPLRLVMX WXXVXRWP QMONNOMRMVNYOZQ[ S[UZVYWVWRVOUNSM",
	"G]IQJOKNMM[M KOMNZN IQJPLO[O OONZM[LZMWOO UOVZW[XZWWUO [M[O OOMZ UOWZ",
	"G\\QMNNLPKTKb MPLTLa QMONNOMSMb MWNYOZQ[S[VZXXYUYSXPVNSMQM WXXVXRWP S[UZVYWVWRVOUNSM KbMb",
	"G]PMMNKPJSJUKXMZP[R[UZWXXUXSWPUNRM LPKRKVLX VXWVWRVP PMNNMOLRLVMYNZP[ R[TZUYVVVRUOTNRM RMZO[N[MPM RMZN",
	"H\\JQKOLNNMZM LONNYN JQKPMOZO ROQZR[SZRO ZMZO RORZ",
	"G\\JMKNLPLUMXOZQ[S[UZWXXVYRYNXMWMXPXSWWUZ KMLNMPMUNX WMXNXO HPINJMLMMNNPNVOYQ[",
	"G]RQQNPMNMLNKOJRJUKXMZP[T[WZYXZUZRYOXNVMTMSNRQ LOKRKULX XXYUYRXO NMMNLQLVMYNZP[ T[VZWYXVXQWNVM RQQb RQRa RQSb QbSb",
	"H\\LMMNNPT_VaXbZb[a NOOPU_V` INJMLMNNPPV_WaXb VSXPYMZMYOVSN\\K`JbKbL_N\\",
	"F]HNINJPJUKXMZP[T[VZXXYVZRZNYMXMYPYSXWVZ JNKPKULX XMYNYO GPHNIMJMKNLPLVMYNZP[ QFSb RGRa SFQb QFSF QbSb",
	"F^NMLNJPISIWJYKZM[O[QZRYSWSTRSQTQWRYSZU[W[YZZY[W[SZPXNVM KPJSJWKY RTRX YYZWZSYP NMLOKRKWLZM[ W[XZYWYRXOVM",
	"G]WMUTUXVZW[Y[[Y\\W XMVTVZ WMYMWTVX UTUQTNRMPMMNKQJTJVKYLZN[P[RZSYTWUT NNLQKTKWLY PMNOMQLTLWMZN[",
	"I\\PFNMMSMWNYOZQ[S[VZXWYTYRXOWNUMSMQNPOOQNT QFOMNQNWOZ VYWWXTXQWO MFRFPMNT S[UYVWWTWQVNUM NFQG OFPH",
	"I[WQWPVPVRXRXPWNUMRMONMQLTLVMYNZP[R[UZWW OONQMTMWNY RMPOOQNTNWOZP[",
	"G]YFVQUUUXVZW[Y[[Y\\W ZFWQVUVZ VF[FWTVX UTUQTNRMPMMNKQJTJVKYLZN[P[RZSYTWUT MOLQKTKWLY PMNOMQLTLWMZN[ WFZG XFYH",
	"I[MVQUTTWRXPWNUMRMONMQLTLVMYNZP[R[UZWX OONQMTMWNY RMPOOQNTNWOZP[",
	"JZZHZGYGYI[I[GZFXFVGTISKRNQRO[N^M`Kb TJSMRRP[O^ XFVHUJTMSRQZP]O_MaKbIbHaH_J_JaIaI` NMYM",
	"H]XMT[S^QaOb YMU[S_ XMZMV[T_RaObLbJaI`I^K^K`J`J_ VTVQUNSMQMNNLQKTKVLYMZO[Q[SZTYUWVT NOMQLTLWMY QMOONQMTMWNZO[",
	"G]OFI[K[ PFJ[ LFQFK[ MTOPQNSMUMWNXPXSVX WNWRVVVZ WPUUUXVZW[Y[[Y\\W MFPG NFOH",
	"KXTFTHVHVFTF UFUH TGVG LQMOOMQMRNSPSSQX RNRRQVQZ RPPUPXQZR[T[VYWW",
	"KXUFUHWHWFUF VFVH UGWG MQNOPMRMSNTPTSRZQ]P_NaLbJbIaI_K_KaJaJ` SNSSQZP]O_ SPRTP[O^N`Lb",
	"G]OFI[K[ PFJ[ LFQFK[ YOYNXNXPZPZNYMWMUNQROS MSOSQTRUTYUZWZ QUSYTZ OSPTRZS[U[WZYW MFPG NFOH",
	"LXTFQQPUPXQZR[T[VYWW UFRQQUQZ QFVFRTQX RFUG SFTH",
	"@cAQBODMFMGNHPHSF[ GNGSE[ GPFTD[F[ HSJPLNNMPMRNSPSSQ[ RNRSP[ RPQTO[Q[ SSUPWNYM[M]N^P^S\\X ]N]R\\V\\Z ]P[U[X\\Z][_[aYbW",
	"F^GQHOJMLMMNNPNSL[ MNMSK[ MPLTJ[L[ NSPPRNTMVMXNYPYSWX XNXRWVWZ XPVUVXWZX[Z[\\Y]W",
	"H\\QMNNLQKTKVLYMZP[S[VZXWYTYRXOWNTMQM NOMQLTLWMY VYWWXTXQWO QMOONQMTMWNZP[ S[UYVWWTWQVNTM",
	"G]HQIOKMMMNNOPOSNWKb NNNSMWJb NPMTIb OTPQQORNTMVMXNYOZRZTYWWZT[R[PZOWOT XOYQYTXWWY VMWNXQXTWWVYT[ FbNb JaGb J`Hb K`Lb JaMb",
	"G\\WMQb XMRb WMYMSb UTUQTNRMPMMNKQJTJVKYLZN[P[RZSYTWUT MOLQKTKWLY PMNOMQLTLWMZN[ NbVb RaOb R`Pb S`Tb RaUb",
	"I[JQKOMMOMPNQPQTO[ PNPTN[ PPOTM[O[ YOYNXNXPZPZNYMWMUNSPQT",
	"J[XPXOWOWQYQYOXNUMRMONNONQOSQTTUVVWX ONNQ ORQSTTVU WVVZ NOOQQRTSVTWVWXVZS[P[MZLYLWNWNYMYMX",
	"KYTFQQPUPXQZR[T[VYWW UFRQQUQZ TFVFRTQX NMXM",
	"F^GQHOJMLMMNNPNSLX MNMRLVLZ MPKUKXLZN[P[RZTXVU XMVUVXWZX[Z[\\Y]W YMWUWZ XMZMXTWX",
	"H\\IQJOLMNMONPPPSNX ONORNVNZ OPMUMXNZP[R[TZVXXUYQYMXMXNYP",
	"CaDQEOGMIMJNKPKSIX JNJRIVIZ JPHUHXIZK[M[OZQXRU TMRURXSZU[W[YZ[X]U^Q^M]M]N^P UMSUSZ TMVMTTSX",
	"G]JQLNNMPMRNSPSR PMQNQRPVOXMZK[I[HZHXJXJZIZIY RORRQVQY ZOZNYNYP[P[NZMXMVNTPSRRVRZS[ PVPXQZS[U[WZYW",
	"G]HQIOKMMMNNOPOSMX NNNRMVMZ NPLULXMZO[Q[SZUXWT YMU[T^RaPb ZMV[T_ YM[MW[U_SaPbMbKaJ`J^L^L`K`K_",
	"H\\YMXOVQNWLYK[ XOOOMPLR VORNONNO VORMOMMOLR LYUYWXXV NYRZUZVY NYR[U[WYXV",
	"",
	"",
	"",
	"",
	"",
	"",
	"H\\WQVOUNSMQMNNLPKSKULXNZQ[S[VZWYXWYSYNXJWHVGSFQFNGMHNHOGQF MPLRLVMX VYWWXSXNWJVH QMONNOMRMVNYOZQ[ S[UZVXWTWMVIUGSF",
	"I[UMWNXOYOXNUMRMONMPLSLUMXOZR[U[XZYYXYWZU[ NPMSMUNX RMPNOONRNVOYPZR[ NTTUUTTSNT NTTT",
	"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF NHMJLNLSMWNY VYWWXSXNWJVH QFOGNIMNMSNXOZQ[ S[UZVXWSWNVIUGSF LPXQ LQXP",
	"G]PMMNKPJSJUKXMZP[T[WZYXZUZSYPWNTMPM LPKSKULX XXYUYSXP PMNNMOLRLVMYNZP[T[VZWYXVXRWOVNTM QFSb RGRa SFQb QFSF QbSb",
	"H\\TMVNXPYPYOWNTMPMMNLOKQKSLUNWPXRYSZT\\T^S_Q_O^P^Q_ MOLQLSMUOW PMNNMPMSNURY YPXO",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"NV",
	"JZ",
	"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF NHMJLNLSMWNY VYWWXSXNWJVH QFOGNIMNMSNXOZQ[ S[UZVXWSWNVIUGSF",
	"H\\QHQ[ RHRZ SFS[ SFPINJ M[W[ QZO[ QYP[ SYT[ SZU[",
	"H\\LJLKMKMJLJ LIMINJNKMLLLKKKJLHMGPFTFWGXHYJYLXNUPPRNSLUKXK[ WHXJXLWN TFVGWJWLVNTPPR KYLXNXSYWYYX NXSZWZXY NXS[W[XZYXYV",
	"H\\LJLKMKMJLJ LIMINJNKMLLLKKKJLHMGPFTFWGXIXLWNTO VGWIWLVN SFUGVIVLUNSO QOTOVPXRYTYWXYWZT[P[MZLYKWKVLUMUNVNWMXLX WRXTXWWY SOUPVQWTWWVZT[ LVLWMWMVLV",
	"H\\SIS[ THTZ UFU[ UFJUZU P[X[ SZQ[ SYR[ UYV[ UZW[",
	"H\\MFKPMNPMSMVNXPYSYUXXVZS[P[MZLYKWKVLUMUNVNWMXLX WPXRXVWX SMUNVOWRWVVYUZS[ LVLWMWMVLV MFWF MGUG MHQHUGWF",
	"H\\VIVJWJWIVI WHVHUIUJVKWKXJXIWGUFRFOGMILKKOKULXNZQ[S[VZXXYUYTXQVOSNQNOONPMR NIMKLOLUMXNY WXXVXSWQ RFPGOHNJMNMUNXOZQ[ S[UZVYWVWSVPUOSN",
	"H\\KFKL YFYIXLTQSSRWR[ SRRTQWQ[ XLSQQTPWP[R[ KJLHNFPFUIWIXHYF MHNGPGRH KJLINHPHUI",
	"H\\PFMGLILLMNPOTOWNXLXIWGTFPF NGMIMLNN VNWLWIVG PFOGNINLONPO TOUNVLVIUGTF POMPLQKSKWLYMZP[T[WZXYYWYSXQWPTO MQLSLWMY WYXWXSWQ PONPMSMWNZP[ T[VZWWWSVPTO",
	"H\\MWMXNXNWMW WOVQURSSQSNRLPKMKLLINGQFSFVGXIYLYRXVWXUZR[O[MZLXLWMVNVOWOXNYMY MPLNLKMI VHWIXLXRWVVX QSORNQMNMKNHOGQF SFUGVIWLWSVWUYTZR[",
	"MXRXQYQZR[S[TZTYSXRX RYRZSZSYRY",
	"MXTZS[R[QZQYRXSXTYT\\S^Q_ RYRZSZSYRY S[T\\ TZS^",
	"MXRMQNQORPSPTOTNSMRM RNROSOSNRN RXQYQZR[S[TZTYSXRX RYRZSZSYRY",
	"MXRMQNQORPSPTOTNSMRM RNROSOSNRN TZS[R[QZQYRXSXTYT\\S^Q_ RYRZSZSYRY S[T\\ TZS^",
	"MXRFQGQIRQ RFRTST RFSFST SFTGTISQ RXQYQZR[S[TZTYSXRX RYRZSZSYRY",
	"I\\MKMJNJNLLLLJMHNGPFTFWGXHYJYLXNWOSQ WHXIXMWN TFVGWIWMVOUP RQRTSTSQRQ RXQYQZR[S[TZTYSXRX RYRZSZSYRY",
	"MXTFRGQIQLRMSMTLTKSJRJQK RKRLSLSKRK RGQK QIRJ",
	"MXTHSIRIQHQGRFSFTGTJSLQM RGRHSHSGRG SITJ THSL",
	"E_[O[NZNZP\\P\\N[MZMYNXPVUTXRZP[L[JZIXIUJSPORMSKSIRGPFNGMIMLNOPRTWWZY[[[\\Y\\X KZJXJUKSLR RMSI SKRG NGMK NNPQTVWYYZ N[LZKXKULSPO MINMQQUVXYZZ[Z\\Y",
	"H\\PBP_ TBT_ XKXJWJWLYLYJXHWGTFPFMGKIKLLNOPURWSXUXXWZ LLMNOOUQWRXT MGLILKMMONUPXRYTYWXYWZT[P[MZLYKWKUMUMWLWLV",
	"G^[BIbJb [B\\BJb",
	"KYUBSDQGOKNPNTOYQ]S`Ub QHPKOOOUPYQ\\ SDRFQIPOPUQ[R^S`",
	"KYOBQDSGUKVPVTUYS]Q`Ob SHTKUOUUTYS\\ QDRFSITOTUS[R^Q`",
	"JZRFQGSQRR RFRR RFSGQQRR MINIVOWO MIWO MIMJWNWO WIVINOMO WIMO WIWJMNMO",
	"F_JQ[Q[R JQJR[R",
	"F_RIRZSZ RISISZ JQ[Q[R JQJR[R",
	"F_JM[M[N JMJN[N JU[U[V JUJV[V",
	"NWSFRGRM SGRM SFTGRM",
	"I[NFMGMM NGMM NFOGMM WFVGVM WGVM WFXGVM",
	"KYQFOGNINKOMQNSNUMVKVIUGSFQF QFNIOMSNVKUGQF SFOGNKQNUMVISF",
	"F^ZIJRZ[ ZIZJLRZZZ[",
	"F^JIZRJ[ JIJJXRJZJ[",
	"G^OFObPb OFPFPb UFUbVb UFVFVb JP[P[Q JPJQ[Q JW[W[X JWJX[X",
	"F^[FYGVHSHPGNFLFJGIIIKKMMMOLPJPHNF [FH[ [FI[ [FJ[ YTWTUUTWTYV[X[ZZ[X[VYT OGLFIIJLMMPJOG NFJGIK KMOLPH ZUWTTWUZX[[XZU YTUUTY V[ZZ[V H[J[",
	"E`VNULSKQKOLNMMOMRNTOUQVSVUUVS OMNONROT QKPLOOORPUQV VKVSWUYVZV\\U]R]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYXYWZ WLWSXU VKXKXSYUZV",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"NV",
	"JZ",
	"H]TFQGOIMLLOKSKVLYMZO[Q[TZVXXUYRZNZKYHXGVFTF QHOJNLMOLSLWMY TYVWWUXRYNYJXH TFRGPJOLNOMSMXNZO[ Q[SZUWVUWRXNXIWGVF",
	"H]TJO[Q[ WFUJP[ WFQ[ WFTIQKOL TJRKOL",
	"H]OKOJPJPLNLNJOHPGSFVFYGZIZKYMWOMUKWI[ XGYIYKXMVOSQ VFWGXIXKWMUOMU JYKXMXRYWYXX MXRZWZ MXR[U[WZXXXW",
	"H]OKOJPJPLNLNJOHPGSFVFYGZIZKYMXNVOSP XGYIYKXMWN VFWGXIXKWMUOSP QPSPVQWRXTXWWYUZR[O[LZKYJWJULULWKWKV VRWTWWVY SPUQVSVWUYTZR[",
	"H]WJR[T[ ZFXJS[ ZFT[ ZFJUZU",
	"H]QFLP QF[F QGYG PHUHYG[F LPMOPNSNVOWPXRXUWXUZQ[N[LZKYJWJULULWKWKV VPWRWUVXTZ SNUOVQVUUXSZQ[",
	"H]YJYIXIXKZKZIYGWFTFQGOIMLLOKSKVLYMZO[R[UZWXXVXSWQVPTOQOOPNQMS PINLMOLSLWMY VXWVWSVQ TFRGPJOLNOMSMXNZO[ R[TZUYVVVRUPTO",
	"H]NFLL [FZIXLTQRTQWP[ RSPWO[ XLRRPUOWN[P[ MIPFRFWI OGRGWI MIOHRHWIYIZH[F",
	"H]SFPGOHNJNMOOQPTPWOYNZLZIYGWFSF UFPG PHOJONPO OORP SPWO XNYLYIXG YGUF SFQHPJPNQP TPVOWNXLXHWF QPMQKSJUJXKZN[R[VZWYXWXTWRVQTP RPMQ NQLSKUKXLZ KZP[VZ VYWWWTVR VQSP QPOQMSLULXMZN[ R[TZUYVWVSUQTP",
	"H]XNWPVQTRQROQNPMNMKNIPGSFVFXGYHZKZNYRXUVXTZQ[N[LZKXKVMVMXLXLW OPNNNKOI XHYJYNXRWUUX QRPQOOOKPHQGSF VFWGXIXNWRVUUWSZQ[",
	"MXPXOYOZP[Q[RZRYQXPX PYPZQZQYPY",
	"MXQ[P[OZOYPXQXRYR[Q]P^N_ PYPZQZQYPY Q[Q\\P^",
	"MXSMRNROSPTPUOUNTMSM SNSOTOTNSN PXOYOZP[Q[RZRYQXPX PYPZQZQYPY",
	"MXSMRNROSPTPUOUNTMSM SNSOTOTNSN Q[P[OZOYPXQXRYR[Q]P^N_ PYPZQZQYPY Q[Q\\P^",
	"MXVFUFTGRT VGUGRT VGVHRT VFWGWHRT PXOYOZP[Q[RZRYQXPX PYPZQZQYPY",
	"H]OKOJPJPLNLNJOHPGSFWFZG[I[KZMYNWOSPQQQSSTTT UFZG YGZIZKYMXNVO WFXGYIYKXMWNSPRQRSST PXOYOZP[Q[RZRYQXPX PYPZQZQYPY",
	"MXWFUGTHSJSLTMUMVLVKUJTJ UGTITJ TKTLULUKTK",
	"MXVIUITHTGUFVFWGWIVKULSM UGUHVHVGUG VIVJUL",
	"E_\\O\\N[N[P]P]N\\M[MYNWPRXPZN[K[HZGXGVHTISKRPPROTMUKUITGRFPGOIOLPRQURWTZV[X[YYYX L[HZ IZHXHVITJSLR PPQSTYVZ K[JZIXIVJTKSMRRO OLPOQRSVUYWZXZYY",
	"H]TBL_ YBQ_ ZKZJYJYL[L[JZHYGVFRFOGMIMLNNPPVSWUWXVZ NLONVRWT OGNINKOMUPWRXTXWWYVZS[O[LZKYJWJULULWKWKV",
	"G^_BEbFb _B`BFb",
	"JZZBXCUERHPKNOMSMXN\\O_Qb SHQKOONTN\\ ZBWDTGRJQLPOOSN\\ NTO]P`Qb",
	"JZSBUEVHWLWQVUTYR\\O_LaJb VHVPUUSYQ\\ SBTDUGVP VHUQTUSXRZP]M`Jb",
	"J[TFSGUQTR TFTR TFUGSQTR OIPIXOYO OIYO OIOJYNYO YIXIPOOO YIOO YIYJONOO",
	"F_JQ[Q[R JQJR[R",
	"F_RIRZSZ RISISZ JQ[Q[R JQJR[R",
	"F_JM[M[N JMJN[N JU[U[V JUJV[V",
	"MWUFTGRM UGRM UFVGRM",
	"H\\PFOGMM PGMM PFQGMM ZFYGWM ZGWM ZF[GWM",
	"KZSFQGPIPKQMSNUNWMXKXIWGUFSF SFPIQMUNXKWGSF UFQGPKSNWMXIUF",
	"F^ZIJRZ[ ZIZJLRZZZ[",
	"F^JIZRJ[ JIJJXRJZJ[",
	"G^SFKbLb SFTFLb YFQbRb YFZFRb KP\\P\\Q KPKQ\\Q IWZWZX IWIXZX",
	"E^^F\\GXHUHQGOFMFKGJIJKLMNMPLQJQHOF ^FE[ ^FF[ ^FG[ XTVTTUSWSYU[W[YZZXZVXT PGMFJIKLNMQJPG OFKGJK LMPLQH YUVTSWTZW[ZXYU XTTUSY U[YZZV E[G[",
	"E`UQUNTLRKPKNLMMLPLSMUOVQVSUTTUQ OLNMMPMSNU RKPLOMNPNSOUPV VKUQUSVUXVZV\\U]R]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYXYWZ WKVQVSWU VKXKWQWSXUZV",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	0 };

    
static const int* getFontData(int fontFace)
{
    bool isItalic = (fontFace & FONT_ITALIC) != 0;
    const int* ascii = 0;
    
    switch( fontFace & 15 )
    {
    case FONT_HERSHEY_SIMPLEX:
        ascii = HersheySimplex;
        break;
    case FONT_HERSHEY_PLAIN:
        ascii = !isItalic ? HersheyPlain : HersheyPlainItalic;
        break;
    case FONT_HERSHEY_DUPLEX:
        ascii = HersheyDuplex;
        break;
    case FONT_HERSHEY_COMPLEX:
        ascii = !isItalic ? HersheyComplex : HersheyComplexItalic;
        break;
    case FONT_HERSHEY_TRIPLEX:
        ascii = !isItalic ? HersheyTriplex : HersheyTriplexItalic;
        break;
    case FONT_HERSHEY_COMPLEX_SMALL:
        ascii = !isItalic ? HersheyComplexSmall : HersheyComplexSmallItalic;
        break;
    case FONT_HERSHEY_SCRIPT_SIMPLEX:
        ascii = HersheyScriptSimplex;
        break;
    case FONT_HERSHEY_SCRIPT_COMPLEX:
        ascii = HersheyScriptComplex;
        break;
    default:
        CV_Error( CV_StsOutOfRange, "Unknown font type" );
    }
    return ascii;
}
    
    
void putText( Mat& img, const string& text, Point org,
              int fontFace, double fontScale, Scalar color,
              int thickness, int line_type, bool bottomLeftOrigin )

{
    const int* ascii = getFontData(fontFace);
           
    double buf[4];
    scalarToRawData(color, buf, img.type(), 0);

    int base_line = -(ascii[0] & 15);
    int hscale = cvRound(fontScale*XY_ONE), vscale = hscale;

    if( line_type == CV_AA && img.depth() != CV_8U )
        line_type = 8;

    if( bottomLeftOrigin )
        vscale = -vscale;

    int view_x = org.x << XY_SHIFT;
    int view_y = (org.y << XY_SHIFT) + base_line*vscale;
    vector<Point> pts;
    pts.reserve(1 << 10);
    const char **faces = cv::g_HersheyGlyphs;

    for( int i = 0; text[i] != '\0'; i++ )
    {
        int c = (uchar)text[i];
        Point p;

        if( c >= 127 || c < ' ' )
            c = '?';

        const char* ptr = faces[ascii[(c-' ')+1]];
        p.x = (uchar)ptr[0] - 'R';
        p.y = (uchar)ptr[1] - 'R';
        int dx = p.y*hscale;
        view_x -= p.x*hscale;
        pts.resize(0);

        for( ptr += 2;; )
        {
            if( *ptr == ' ' || !*ptr )
            {
                if( pts.size() > 1 )
                    PolyLine( img, &pts[0], (int)pts.size(), false, buf, thickness, line_type, XY_SHIFT ); 
                if( !*ptr++ )
                    break;
                pts.resize(0);
            }
            else
            {
                p.x = (uchar)ptr[0] - 'R';
                p.y = (uchar)ptr[1] - 'R';
                ptr += 2;
                pts.push_back(Point(p.x*hscale + view_x, p.y*vscale + view_y));
            }
        }
        view_x += dx;
    }
}

Size getTextSize( const string& text, int fontFace, double fontScale, int thickness, int* _base_line)
{
    Size size;
    double view_x = 0;
    const char **faces = cv::g_HersheyGlyphs;
    const int* ascii = getFontData(fontFace);

    int base_line = (ascii[0] & 15);
    int cap_line = (ascii[0] >> 4) & 15;
    size.height = cvRound((cap_line + base_line)*fontScale + (thickness+1)/2);

    for( int i = 0; text[i] != '\0'; i++ )
    {
        int c = (uchar)text[i];
        Point p;

        if( c >= 127 || c < ' ' )
            c = '?';

        const char* ptr = faces[ascii[(c-' ')+1]];
        p.x = (uchar)ptr[0] - 'R';
        p.y = (uchar)ptr[1] - 'R';
        view_x += (p.y - p.x)*fontScale;
    }

    size.width = cvRound(view_x + thickness);
    if( _base_line )
        *_base_line = cvRound(base_line*fontScale + thickness*0.5);
    return size;
}

}


void cv::fillConvexPoly(InputOutputArray _img, InputArray _points,
                        const Scalar& color, int lineType, int shift)
{
    Mat img = _img.getMat(), points = _points.getMat();
    CV_Assert(points.checkVector(2, CV_32S) >= 0);
    fillConvexPoly(img, (const Point*)points.data, points.rows*points.cols*points.channels()/2, color, lineType, shift);
}


void cv::fillPoly(InputOutputArray _img, InputArrayOfArrays pts,
                  const Scalar& color, int lineType, int shift, Point offset)
{
    Mat img = _img.getMat();
    int i, ncontours = (int)pts.total();
    if( ncontours == 0 )
        return;
    AutoBuffer<Point*> _ptsptr(ncontours);
    AutoBuffer<int> _npts(ncontours);
    Point** ptsptr = _ptsptr;
    int* npts = _npts;
    
    for( i = 0; i < ncontours; i++ )
    {
        Mat p = pts.getMat(i);
        CV_Assert(p.checkVector(2, CV_32S) >= 0);
        ptsptr[i] = (Point*)p.data;
        npts[i] = p.rows*p.cols*p.channels()/2;
    }
    fillPoly(img, (const Point**)ptsptr, npts, (int)ncontours, color, lineType, shift, offset);
}


void cv::polylines(InputOutputArray _img, InputArrayOfArrays pts,
                   bool isClosed, const Scalar& color,
                   int thickness, int lineType, int shift )
{
    Mat img = _img.getMat();
    int i, ncontours = (int)pts.total();
    if( ncontours == 0 )
        return;
    AutoBuffer<Point*> _ptsptr(ncontours);
    AutoBuffer<int> _npts(ncontours);
    Point** ptsptr = _ptsptr;
    int* npts = _npts;
    
    for( i = 0; i < ncontours; i++ )
    {
        Mat p = pts.getMat(i);
        CV_Assert(p.checkVector(2, CV_32S) >= 0);
        ptsptr[i] = (Point*)p.data;
        npts[i] = p.rows*p.cols*p.channels()/2;
    }
    polylines(img, (const Point**)ptsptr, npts, (int)ncontours, isClosed, color, thickness, lineType, shift);
}


static const int CodeDeltas[8][2] =
{ {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1} };

#define CV_ADJUST_EDGE_COUNT( count, seq )  \
    ((count) -= ((count) == (seq)->total && !CV_IS_SEQ_CLOSED(seq)))

CV_IMPL void
cvDrawContours( void* _img, CvSeq* contour,
                CvScalar _externalColor, CvScalar _holeColor, 
                int  maxLevel, int thickness,
                int line_type, CvPoint _offset )
{
    CvSeq *contour0 = contour, *h_next = 0;
    CvTreeNodeIterator iterator;
    cv::vector<cv::PolyEdge> edges;
    cv::vector<cv::Point> pts;
    cv::Scalar externalColor = _externalColor, holeColor = _holeColor;
    cv::Mat img = cv::cvarrToMat(_img);
    cv::Point offset = _offset;
    double ext_buf[4], hole_buf[4];

    if( line_type == CV_AA && img.depth() != CV_8U )
        line_type = 8;

    if( !contour )
        return;

    CV_Assert( thickness <= 255 );

    scalarToRawData( externalColor, ext_buf, img.type(), 0 );
    scalarToRawData( holeColor, hole_buf, img.type(), 0 );

    maxLevel = MAX(maxLevel, INT_MIN+2);
    maxLevel = MIN(maxLevel, INT_MAX-1);
    
    if( maxLevel < 0 )
    {
        h_next = contour->h_next;
        contour->h_next = 0;
        maxLevel = -maxLevel+1;
    }

    cvInitTreeNodeIterator( &iterator, contour, maxLevel );
    while( (contour = (CvSeq*)cvNextTreeNode( &iterator )) != 0 )
    {
        CvSeqReader reader;
        int i, count = contour->total;
        int elem_type = CV_MAT_TYPE(contour->flags);
        void* clr = (contour->flags & CV_SEQ_FLAG_HOLE) == 0 ? ext_buf : hole_buf;

        cvStartReadSeq( contour, &reader, 0 );
        if( thickness < 0 )
            pts.resize(0);

        if( CV_IS_SEQ_CHAIN_CONTOUR( contour ))
        {
            cv::Point pt = ((CvChain*)contour)->origin;
            cv::Point prev_pt = pt;
            char prev_code = reader.ptr ? reader.ptr[0] : '\0';

            prev_pt += offset;

            for( i = 0; i < count; i++ )
            {
                char code;
                CV_READ_SEQ_ELEM( code, reader );

                assert( (code & ~7) == 0 );

                if( code != prev_code )
                {
                    prev_code = code;
                    if( thickness >= 0 )
                        cv::ThickLine( img, prev_pt, pt, clr, thickness, line_type, 2, 0 );
                    else
                        pts.push_back(pt);
                    prev_pt = pt;
                }
            
                pt.x += CodeDeltas[(int)code][0];
                pt.y += CodeDeltas[(int)code][1];
            }

            if( thickness >= 0 )
                cv::ThickLine( img, prev_pt,
                    cv::Point(((CvChain*)contour)->origin) + offset,
                    clr, thickness, line_type, 2, 0 );
            else
                cv::CollectPolyEdges(img, &pts[0], (int)pts.size(),
                                     edges, ext_buf, line_type, 0, offset);
        }
        else if( CV_IS_SEQ_POLYLINE( contour ))
        {
            CV_Assert( elem_type == CV_32SC2 );
            cv::Point pt1, pt2;
            int shift = 0;
            
            count -= !CV_IS_SEQ_CLOSED(contour);
            CV_READ_SEQ_ELEM( pt1, reader );
            pt1 += offset;
            if( thickness < 0 )
                pts.push_back(pt1);

            for( i = 0; i < count; i++ )
            {
                CV_READ_SEQ_ELEM( pt2, reader );
                pt2 += offset;
                if( thickness >= 0 )
                    cv::ThickLine( img, pt1, pt2, clr, thickness, line_type, 2, shift );
                else
                    pts.push_back(pt2);
                pt1 = pt2;
            }
            if( thickness < 0 )
                cv::CollectPolyEdges( img, &pts[0], (int)pts.size(),
                                      edges, ext_buf, line_type, 0, cv::Point() );
        }
    }

    if( thickness < 0 )
        cv::FillEdgeCollection( img, edges, ext_buf );

    if( h_next && contour0 )
        contour0->h_next = h_next;
}

CV_IMPL int
cvClipLine( CvSize size, CvPoint* pt1, CvPoint* pt2 )
{
    CV_Assert( pt1 && pt2 );
    return cv::clipLine( size, *(cv::Point*)pt1, *(cv::Point*)pt2 );
}


CV_IMPL int
cvEllipse2Poly( CvPoint center, CvSize axes, int angle,
                int arc_start, int arc_end, CvPoint* _pts, int delta )
{
    cv::vector<cv::Point> pts;
    cv::ellipse2Poly( center, axes, angle, arc_start, arc_end, delta, pts );
    memcpy( _pts, &pts[0], pts.size()*sizeof(_pts[0]) );
    return (int)pts.size();
}

CV_IMPL CvScalar
cvColorToScalar( double packed_color, int type )
{
    CvScalar scalar;
    
    if( CV_MAT_DEPTH( type ) == CV_8U )
    {
        int icolor = cvRound( packed_color );
        if( CV_MAT_CN( type ) > 1 )
        {
            scalar.val[0] = icolor & 255;
            scalar.val[1] = (icolor >> 8) & 255;
            scalar.val[2] = (icolor >> 16) & 255;
            scalar.val[3] = (icolor >> 24) & 255;
        }
        else
        {
            scalar.val[0] = CV_CAST_8U( icolor );
            scalar.val[1] = scalar.val[2] = scalar.val[3] = 0;
        }
    }
    else if( CV_MAT_DEPTH( type ) == CV_8S )
    {
        int icolor = cvRound( packed_color );
        if( CV_MAT_CN( type ) > 1 )
        {
            scalar.val[0] = (char)icolor;
            scalar.val[1] = (char)(icolor >> 8);
            scalar.val[2] = (char)(icolor >> 16);
            scalar.val[3] = (char)(icolor >> 24);
        }
        else
        {
            scalar.val[0] = CV_CAST_8S( icolor );
            scalar.val[1] = scalar.val[2] = scalar.val[3] = 0;
        }
    }
    else
    {
        int cn = CV_MAT_CN( type );
        switch( cn )
        {
        case 1:
            scalar.val[0] = packed_color;
            scalar.val[1] = scalar.val[2] = scalar.val[3] = 0;
            break;
        case 2:
            scalar.val[0] = scalar.val[1] = packed_color;
            scalar.val[2] = scalar.val[3] = 0;
            break;
        case 3:
            scalar.val[0] = scalar.val[1] = scalar.val[2] = packed_color;
            scalar.val[3] = 0;
            break;
        default:
            scalar.val[0] = scalar.val[1] =
                scalar.val[2] = scalar.val[3] = packed_color;
            break;
        }
    }

    return scalar;
}

CV_IMPL int
cvInitLineIterator( const CvArr* img, CvPoint pt1, CvPoint pt2,
                    CvLineIterator* iterator, int connectivity,
                    int left_to_right )
{
    CV_Assert( iterator != 0 );
    cv::LineIterator li(cv::cvarrToMat(img), pt1, pt2, connectivity, left_to_right!=0);

    iterator->err = li.err;
    iterator->minus_delta = li.minusDelta;
    iterator->plus_delta = li.plusDelta;
    iterator->minus_step = li.minusStep;
    iterator->plus_step = li.plusStep;
    iterator->ptr = li.ptr;

    return li.count;
}

CV_IMPL void
cvLine( CvArr* _img, CvPoint pt1, CvPoint pt2, CvScalar color,
        int thickness, int line_type, int shift )
{
    cv::Mat img = cv::cvarrToMat(_img);
    cv::line( img, pt1, pt2, color, thickness, line_type, shift );
}

CV_IMPL void
cvRectangle( CvArr* _img, CvPoint pt1, CvPoint pt2,
             CvScalar color, int thickness,
             int line_type, int shift )
{
    cv::Mat img = cv::cvarrToMat(_img);
    cv::rectangle( img, pt1, pt2, color, thickness, line_type, shift );
}

CV_IMPL void
cvRectangleR( CvArr* _img, CvRect rec,
              CvScalar color, int thickness,
              int line_type, int shift )
{
    cv::Mat img = cv::cvarrToMat(_img);
    cv::rectangle( img, rec, color, thickness, line_type, shift );
}

CV_IMPL void
cvCircle( CvArr* _img, CvPoint center, int radius,
          CvScalar color, int thickness, int line_type, int shift )
{
    cv::Mat img = cv::cvarrToMat(_img);
    cv::circle( img, center, radius, color, thickness, line_type, shift );
}

CV_IMPL void
cvEllipse( CvArr* _img, CvPoint center, CvSize axes,
           double angle, double start_angle, double end_angle,
           CvScalar color, int thickness, int line_type, int shift )
{
    cv::Mat img = cv::cvarrToMat(_img);
    cv::ellipse( img, center, axes, angle, start_angle, end_angle,
        color, thickness, line_type, shift );
}

CV_IMPL void
cvFillConvexPoly( CvArr* _img, const CvPoint *pts, int npts,
                  CvScalar color, int line_type, int shift )
{
    cv::Mat img = cv::cvarrToMat(_img);
    cv::fillConvexPoly( img, (const cv::Point*)pts, npts,
                        color, line_type, shift );
}

CV_IMPL void
cvFillPoly( CvArr* _img, CvPoint **pts, const int *npts, int ncontours,
            CvScalar color, int line_type, int shift )
{
    cv::Mat img = cv::cvarrToMat(_img);

    cv::fillPoly( img, (const cv::Point**)pts, npts, ncontours, color, line_type, shift );
}

CV_IMPL void
cvPolyLine( CvArr* _img, CvPoint **pts, const int *npts,
            int ncontours, int closed, CvScalar color,
            int thickness, int line_type, int shift )
{
    cv::Mat img = cv::cvarrToMat(_img);

    cv::polylines( img, (const cv::Point**)pts, npts, ncontours,
                   closed != 0, color, thickness, line_type, shift );
}

CV_IMPL void
cvPutText( CvArr* _img, const char *text, CvPoint org, const CvFont *_font, CvScalar color )
{
    cv::Mat img = cv::cvarrToMat(_img);
    CV_Assert( text != 0 && _font != 0);
    cv::putText( img, text, org, _font->font_face, (_font->hscale+_font->vscale)*0.5,
                color, _font->thickness, _font->line_type,
                CV_IS_IMAGE(_img) && ((IplImage*)_img)->origin != 0 );
}


CV_IMPL void
cvInitFont( CvFont *font, int font_face, double hscale, double vscale,
            double shear, int thickness, int line_type )
{
    CV_Assert( font != 0 && hscale > 0 && vscale > 0 && thickness >= 0 );

    font->ascii = cv::getFontData(font_face);
    font->font_face = font_face;
    font->hscale = (float)hscale;
    font->vscale = (float)vscale;
    font->thickness = thickness;
    font->shear = (float)shear;
    font->greek = font->cyrillic = 0;
    font->line_type = line_type;
}

CV_IMPL void
cvGetTextSize( const char *text, const CvFont *_font, CvSize *_size, int *_base_line )
{
    CV_Assert(text != 0 && _font != 0);
    cv::Size size = cv::getTextSize( text, _font->font_face, (_font->hscale + _font->vscale)*0.5,
                                     _font->thickness, _base_line );
    if( _size )
        *_size = size;
}

/* End of file. */
