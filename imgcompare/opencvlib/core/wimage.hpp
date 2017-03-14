#ifndef _CORE_WIMAGE_HPP_
#define _CORE_WIMAGE_HPP_

#include "../../opencvlib/core/core_c.h"

#ifdef __cplusplus

namespace cv {

template <typename T> class WImage;
template <typename T> class WImageBuffer;
template <typename T> class WImageView;

template<typename T, int C> class WImageC;
template<typename T, int C> class WImageBufferC;
template<typename T, int C> class WImageViewC;

// Commonly used typedefs.
typedef WImage<uchar>            WImage_b;
typedef WImageView<uchar>        WImageView_b;
typedef WImageBuffer<uchar>      WImageBuffer_b;

typedef WImageC<uchar, 1>        WImage1_b;
typedef WImageViewC<uchar, 1>    WImageView1_b;
typedef WImageBufferC<uchar, 1>  WImageBuffer1_b;

typedef WImageC<uchar, 3>        WImage3_b;
typedef WImageViewC<uchar, 3>    WImageView3_b;
typedef WImageBufferC<uchar, 3>  WImageBuffer3_b;

typedef WImage<float>            WImage_f;
typedef WImageView<float>        WImageView_f;
typedef WImageBuffer<float>      WImageBuffer_f;

typedef WImageC<float, 1>        WImage1_f;
typedef WImageViewC<float, 1>    WImageView1_f;
typedef WImageBufferC<float, 1>  WImageBuffer1_f;

typedef WImageC<float, 3>        WImage3_f;
typedef WImageViewC<float, 3>    WImageView3_f;
typedef WImageBufferC<float, 3>  WImageBuffer3_f;

// There isn't a standard for signed and unsigned short so be more
// explicit in the typename for these cases.
typedef WImage<short>            WImage_16s;
typedef WImageView<short>        WImageView_16s;
typedef WImageBuffer<short>      WImageBuffer_16s;

typedef WImageC<short, 1>        WImage1_16s;
typedef WImageViewC<short, 1>    WImageView1_16s;
typedef WImageBufferC<short, 1>  WImageBuffer1_16s;

typedef WImageC<short, 3>        WImage3_16s;
typedef WImageViewC<short, 3>    WImageView3_16s;
typedef WImageBufferC<short, 3>  WImageBuffer3_16s;

typedef WImage<ushort>            WImage_16u;
typedef WImageView<ushort>        WImageView_16u;
typedef WImageBuffer<ushort>      WImageBuffer_16u;

typedef WImageC<ushort, 1>        WImage1_16u;
typedef WImageViewC<ushort, 1>    WImageView1_16u;
typedef WImageBufferC<ushort, 1>  WImageBuffer1_16u;

typedef WImageC<ushort, 3>        WImage3_16u;
typedef WImageViewC<ushort, 3>    WImageView3_16u;
typedef WImageBufferC<ushort, 3>  WImageBuffer3_16u;

//
// WImage definitions
//
// This WImage class gives access to the data it refers to.  It can be
// constructed either by allocating the data with a WImageBuffer class or
// using the WImageView class to refer to a subimage or outside data.
template<typename T>
class WImage
{
public:
    typedef T BaseType;

    // WImage is an abstract class with no other virtual methods so make the
    // destructor virtual.
    virtual ~WImage() = 0;

    // Accessors
    IplImage* Ipl() {return image_; }
    const IplImage* Ipl() const {return image_; }
    T* ImageData() { return reinterpret_cast<T*>(image_->imageData); }
    const T* ImageData() const {
        return reinterpret_cast<const T*>(image_->imageData);
    }

    int Width() const {return image_->width; }
    int Height() const {return image_->height; }

    // WidthStep is the number of bytes to go to the pixel with the next y coord
    int WidthStep() const {return image_->widthStep; }

    int Channels() const {return image_->nChannels; }
    int ChannelSize() const {return sizeof(T); }  // number of bytes per channel

    // Number of bytes per pixel
    int PixelSize() const {return Channels() * ChannelSize(); }

    // Return depth type (e.g. IPL_DEPTH_8U, IPL_DEPTH_32F) which is the number
    // of bits per channel and with the signed bit set.
    // This is known at compile time using specializations.
    int Depth() const;

    inline const T* Row(int r) const {
        return reinterpret_cast<T*>(image_->imageData + r*image_->widthStep);
    }

    inline T* Row(int r) {
        return reinterpret_cast<T*>(image_->imageData + r*image_->widthStep);
    }

    // Pixel accessors which returns a pointer to the start of the channel
    inline T* operator() (int c, int r)  {
        return reinterpret_cast<T*>(image_->imageData + r*image_->widthStep) +
            c*Channels();
    }

    inline const T* operator() (int c, int r) const  {
        return reinterpret_cast<T*>(image_->imageData + r*image_->widthStep) +
            c*Channels();
    }

    // Copy the contents from another image which is just a convenience to cvCopy
    void CopyFrom(const WImage<T>& src) { cvCopy(src.Ipl(), image_); }

    // Set contents to zero which is just a convenient to cvSetZero
    void SetZero() { cvSetZero(image_); }

    // Construct a view into a region of this image
    WImageView<T> View(int c, int r, int width, int height);

protected:
    // Disallow copy and assignment
    WImage(const WImage&);
    void operator=(const WImage&);

    explicit WImage(IplImage* img) : image_(img) {
        assert(!img || img->depth == Depth());
    }

    void SetIpl(IplImage* image) {
        assert(!image || image->depth == Depth());
        image_ = image;
    }

    IplImage* image_;
};



// Image class when both the pixel type and number of channels
// are known at compile time.  This wrapper will speed up some of the operations
// like accessing individual pixels using the () operator.
template<typename T, int C>
class WImageC : public WImage<T>
{
public:
    typedef typename WImage<T>::BaseType BaseType;
    enum { kChannels = C };

    explicit WImageC(IplImage* img) : WImage<T>(img) {
        assert(!img || img->nChannels == Channels());
    }

    // Construct a view into a region of this image
    WImageViewC<T, C> View(int c, int r, int width, int height);

    // Copy the contents from another image which is just a convenience to cvCopy
    void CopyFrom(const WImageC<T, C>& src) {
        cvCopy(src.Ipl(), WImage<T>::image_);
    }

    // WImageC is an abstract class with no other virtual methods so make the
    // destructor virtual.
    virtual ~WImageC() = 0;

    int Channels() const {return C; }

protected:
    // Disallow copy and assignment
    WImageC(const WImageC&);
    void operator=(const WImageC&);

    void SetIpl(IplImage* image) {
        assert(!image || image->depth == WImage<T>::Depth());
        WImage<T>::SetIpl(image);
    }
};

//
// WImageBuffer definitions
//
// Image class which owns the data, so it can be allocated and is always
// freed.  It cannot be copied but can be explicity cloned.
//
template<typename T>
class WImageBuffer : public WImage<T>
{
public:
    typedef typename WImage<T>::BaseType BaseType;

    // Default constructor which creates an object that can be
    WImageBuffer() : WImage<T>(0) {}

    WImageBuffer(int width, int height, int nchannels) : WImage<T>(0) {
        Allocate(width, height, nchannels);
    }

    // Constructor which takes ownership of a given IplImage so releases
    // the image on destruction.
    explicit WImageBuffer(IplImage* img) : WImage<T>(img) {}

    // Allocate an image.  Does nothing if current size is the same as
    // the new size.
    void Allocate(int width, int height, int nchannels);

    // Set the data to point to an image, releasing the old data
    void SetIpl(IplImage* img) {
        ReleaseImage();
        WImage<T>::SetIpl(img);
    }

    // Clone an image which reallocates the image if of a different dimension.
    void CloneFrom(const WImage<T>& src) {
        Allocate(src.Width(), src.Height(), src.Channels());
        CopyFrom(src);
    }

    ~WImageBuffer() {
        ReleaseImage();
    }

    // Release the image if it isn't null.
    void ReleaseImage() {
        if (WImage<T>::image_) {
            IplImage* image = WImage<T>::image_;
            cvReleaseImage(&image);
            WImage<T>::SetIpl(0);
        }
    }

    bool IsNull() const {return WImage<T>::image_ == NULL; }

private:
    // Disallow copy and assignment
    WImageBuffer(const WImageBuffer&);
    void operator=(const WImageBuffer&);
};

// Like a WImageBuffer class but when the number of channels is known
// at compile time.
template<typename T, int C>
class WImageBufferC : public WImageC<T, C>
{
public:
    typedef typename WImage<T>::BaseType BaseType;
    enum { kChannels = C };

    // Default constructor which creates an object that can be
    WImageBufferC() : WImageC<T, C>(0) {}

    WImageBufferC(int width, int height) : WImageC<T, C>(0) {
        Allocate(width, height);
    }

    // Constructor which takes ownership of a given IplImage so releases
    // the image on destruction.
    explicit WImageBufferC(IplImage* img) : WImageC<T, C>(img) {}

    // Allocate an image.  Does nothing if current size is the same as
    // the new size.
    void Allocate(int width, int height);

    // Set the data to point to an image, releasing the old data
    void SetIpl(IplImage* img) {
        ReleaseImage();
        WImageC<T, C>::SetIpl(img);
    }

    // Clone an image which reallocates the image if of a different dimension.
    void CloneFrom(const WImageC<T, C>& src) {
        Allocate(src.Width(), src.Height());
        CopyFrom(src);
    }

    ~WImageBufferC() {
        ReleaseImage();
    }

    // Release the image if it isn't null.
    void ReleaseImage() {
        if (WImage<T>::image_) {
            IplImage* image = WImage<T>::image_;
            cvReleaseImage(&image);
            WImageC<T, C>::SetIpl(0);
        }
    }

    bool IsNull() const {return WImage<T>::image_ == NULL; }

private:
    // Disallow copy and assignment
    WImageBufferC(const WImageBufferC&);
    void operator=(const WImageBufferC&);
};

//
// WImageView definitions
//
// View into an image class which allows treating a subimage as an image
// or treating external data as an image
//
template<typename T>
class WImageView : public WImage<T>
{
public:
    typedef typename WImage<T>::BaseType BaseType;

    // Construct a subimage.  No checks are done that the subimage lies
    // completely inside the original image.
    WImageView(WImage<T>* img, int c, int r, int width, int height);

    // Refer to external data.
    // If not given width_step assumed to be same as width.
    WImageView(T* data, int width, int height, int channels, int width_step = -1);

    // Refer to external data.  This does NOT take ownership
    // of the supplied IplImage.
    WImageView(IplImage* img) : WImage<T>(img) {}

    // Copy constructor
    WImageView(const WImage<T>& img) : WImage<T>(0) {
        header_ = *(img.Ipl());
        WImage<T>::SetIpl(&header_);
    }

    WImageView& operator=(const WImage<T>& img) {
        header_ = *(img.Ipl());
        WImage<T>::SetIpl(&header_);
        return *this;
    }

protected:
    IplImage header_;
};


template<typename T, int C>
class WImageViewC : public WImageC<T, C>
{
public:
    typedef typename WImage<T>::BaseType BaseType;
    enum { kChannels = C };

    // Default constructor needed for vectors of views.
    WImageViewC();

    virtual ~WImageViewC() {}

    // Construct a subimage.  No checks are done that the subimage lies
    // completely inside the original image.
    WImageViewC(WImageC<T, C>* img,
        int c, int r, int width, int height);

    // Refer to external data
    WImageViewC(T* data, int width, int height, int width_step = -1);

    // Refer to external data.  This does NOT take ownership
    // of the supplied IplImage.
    WImageViewC(IplImage* img) : WImageC<T, C>(img) {}

    // Copy constructor which does a shallow copy to allow multiple views
    // of same data.  gcc-4.1.1 gets confused if both versions of
    // the constructor and assignment operator are not provided.
    WImageViewC(const WImageC<T, C>& img) : WImageC<T, C>(0) {
        header_ = *(img.Ipl());
        WImageC<T, C>::SetIpl(&header_);
    }
    WImageViewC(const WImageViewC<T, C>& img) : WImageC<T, C>(0) {
        header_ = *(img.Ipl());
        WImageC<T, C>::SetIpl(&header_);
    }

    WImageViewC& operator=(const WImageC<T, C>& img) {
        header_ = *(img.Ipl());
        WImageC<T, C>::SetIpl(&header_);
        return *this;
    }
    WImageViewC& operator=(const WImageViewC<T, C>& img) {
        header_ = *(img.Ipl());
        WImageC<T, C>::SetIpl(&header_);
        return *this;
    }

protected:
    IplImage header_;
};


// Specializations for depth
template<>
inline int WImage<uchar>::Depth() const {return IPL_DEPTH_8U; }
template<>
inline int WImage<signed char>::Depth() const {return IPL_DEPTH_8S; }
template<>
inline int WImage<short>::Depth() const {return IPL_DEPTH_16S; }
template<>
inline int WImage<ushort>::Depth() const {return IPL_DEPTH_16U; }
template<>
inline int WImage<int>::Depth() const {return IPL_DEPTH_32S; }
template<>
inline int WImage<float>::Depth() const {return IPL_DEPTH_32F; }
template<>
inline int WImage<double>::Depth() const {return IPL_DEPTH_64F; }

//
// Pure virtual destructors still need to be defined.
//
template<typename T> inline WImage<T>::~WImage() {}
template<typename T, int C> inline WImageC<T, C>::~WImageC() {}

//
// Allocate ImageData
//
template<typename T>
inline void WImageBuffer<T>::Allocate(int width, int height, int nchannels)
{
    if (IsNull() || WImage<T>::Width() != width ||
        WImage<T>::Height() != height || WImage<T>::Channels() != nchannels) {
        ReleaseImage();
        WImage<T>::image_ = cvCreateImage(cvSize(width, height),
            WImage<T>::Depth(), nchannels);
    }
}

template<typename T, int C>
inline void WImageBufferC<T, C>::Allocate(int width, int height)
{
    if (IsNull() || WImage<T>::Width() != width || WImage<T>::Height() != height) {
        ReleaseImage();
        WImageC<T, C>::SetIpl(cvCreateImage(cvSize(width, height),WImage<T>::Depth(), C));
    }
}

//
// ImageView methods
//
template<typename T>
WImageView<T>::WImageView(WImage<T>* img, int c, int r, int width, int height)
        : WImage<T>(0)
{
    header_ = *(img->Ipl());
    header_.imageData = reinterpret_cast<char*>((*img)(c, r));
    header_.width = width;
    header_.height = height;
    WImage<T>::SetIpl(&header_);
}

template<typename T>
WImageView<T>::WImageView(T* data, int width, int height, int nchannels, int width_step)
          : WImage<T>(0)
{
    cvInitImageHeader(&header_, cvSize(width, height), WImage<T>::Depth(), nchannels);
    header_.imageData = reinterpret_cast<char*>(data);
    if (width_step > 0) {
        header_.widthStep = width_step;
    }
    WImage<T>::SetIpl(&header_);
}

template<typename T, int C>
WImageViewC<T, C>::WImageViewC(WImageC<T, C>* img, int c, int r, int width, int height)
        : WImageC<T, C>(0)
{
    header_ = *(img->Ipl());
    header_.imageData = reinterpret_cast<char*>((*img)(c, r));
    header_.width = width;
    header_.height = height;
    WImageC<T, C>::SetIpl(&header_);
}

template<typename T, int C>
WImageViewC<T, C>::WImageViewC() : WImageC<T, C>(0) {
    cvInitImageHeader(&header_, cvSize(0, 0), WImage<T>::Depth(), C);
    header_.imageData = reinterpret_cast<char*>(0);
    WImageC<T, C>::SetIpl(&header_);
}

template<typename T, int C>
WImageViewC<T, C>::WImageViewC(T* data, int width, int height, int width_step)
    : WImageC<T, C>(0)
{
    cvInitImageHeader(&header_, cvSize(width, height), WImage<T>::Depth(), C);
    header_.imageData = reinterpret_cast<char*>(data);
    if (width_step > 0) {
        header_.widthStep = width_step;
    }
    WImageC<T, C>::SetIpl(&header_);
}

// Construct a view into a region of an image
template<typename T>
WImageView<T> WImage<T>::View(int c, int r, int width, int height) {
    return WImageView<T>(this, c, r, width, height);
}

template<typename T, int C>
WImageViewC<T, C> WImageC<T, C>::View(int c, int r, int width, int height) {
    return WImageViewC<T, C>(this, c, r, width, height);
}

}  // end of namespace

#endif // __cplusplus

#endif
