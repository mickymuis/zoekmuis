#ifndef __HIGHGUI_H_
#define __HIGHGUI_H_

#if _MSC_VER >= 1200
#pragma warning( disable: 4251 )
#endif

/*#include "cvconfig.h"*/

#include "../../opencvlib/highgui/highgui.hpp"
#include "../../opencvlib/highgui/highgui_c.h"
#include "../../opencvlib/imgproc/imgproc_c.h"
#include "../../opencvlib/core/internal.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>

#ifdef HAVE_TEGRA_OPTIMIZATION
#include "../../opencvlib/highgui/highgui_tegra.hpp"
#endif

#if defined WIN32 || defined _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max

void  FillBitmapInfo( BITMAPINFO* bmi, int width, int height, int bpp, int origin );
#endif

// BT: enable loading and saving images in jpg, png and tiff formats
#define HAVE_JPEG
#define HAVE_PNG
#define HAVE_PNG_GET_VALID
#define HAVE_PNG_H
#define HAVE_PNG_SET_TRNS_TO_ALPHA
//#define HAVE_TIFF

/* Errors */
#define HG_OK          0 /* Don't bet on it! */
#define HG_BADNAME    -1 /* Bad window or file name */
#define HG_INITFAILED -2 /* Can't initialize HigHGUI */
#define HG_WCFAILED   -3 /* Can't create a window */
#define HG_NULLPTR    -4 /* The null pointer where it should not appear */
#define HG_BADPARAM   -5

#define __BEGIN__ __CV_BEGIN__
#define __END__  __CV_END__
#define EXIT __CV_EXIT__

#define CV_WINDOW_MAGIC_VAL     0x00420042
#define CV_TRACKBAR_MAGIC_VAL   0x00420043

/***************************** CvCapture structure ******************************/

struct CvCapture
{
    virtual ~CvCapture() {}
    virtual double getProperty(int) { return 0; }
    virtual bool setProperty(int, double) { return 0; }
    virtual bool grabFrame() { return true; }
    virtual IplImage* retrieveFrame(int) { return 0; }
	virtual int getCaptureDomain() { return CV_CAP_ANY; } // Return the type of the capture object: CV_CAP_VFW, etc...
};

/*************************** CvVideoWriter structure ****************************/

struct CvVideoWriter
{
    virtual ~CvVideoWriter() {}
    virtual bool writeFrame(const IplImage*) { return false; }
};

#if defined WIN32 || defined _WIN32
#define HAVE_VFW 1

/* uncomment to enable CMUCamera1394 fireware camera module */
//#define HAVE_CMU1394 1
#endif


CvCapture * cvCreateCameraCapture_V4L( int index );
CvCapture * cvCreateCameraCapture_DC1394( int index );
CvCapture * cvCreateCameraCapture_DC1394_2( int index );
CvCapture* cvCreateCameraCapture_MIL( int index );
CvCapture * cvCreateCameraCapture_CMU( int index );
CV_IMPL CvCapture * cvCreateCameraCapture_TYZX( int index );
CvCapture* cvCreateFileCapture_Win32( const char* filename );
CvCapture* cvCreateCameraCapture_VFW( int index );
CvCapture* cvCreateFileCapture_VFW( const char* filename );
CvVideoWriter* cvCreateVideoWriter_Win32( const char* filename, int fourcc,
                                          double fps, CvSize frameSize, int is_color );
CvVideoWriter* cvCreateVideoWriter_VFW( const char* filename, int fourcc,
                                        double fps, CvSize frameSize, int is_color );
CvCapture* cvCreateCameraCapture_DShow( int index );
CvCapture* cvCreateCameraCapture_OpenNI( int index );
CvCapture* cvCreateCameraCapture_Android( int index );
CvCapture* cvCreateCameraCapture_XIMEA( int index );

CVAPI(int) cvHaveImageReader(const char* filename);
CVAPI(int) cvHaveImageWriter(const char* filename);

CvCapture* cvCreateFileCapture_Images(const char* filename);
CvVideoWriter* cvCreateVideoWriter_Images(const char* filename);

CvCapture* cvCreateFileCapture_XINE (const char* filename);

#define CV_CAP_GSTREAMER_1394		0
#define CV_CAP_GSTREAMER_V4L		1
#define CV_CAP_GSTREAMER_V4L2		2
#define CV_CAP_GSTREAMER_FILE		3

CvCapture* cvCreateCapture_GStreamer(int type, const char *filename);
CvCapture* cvCreateFileCapture_FFMPEG_proxy(const char* filename);


CvVideoWriter* cvCreateVideoWriter_FFMPEG_proxy( const char* filename, int fourcc,
                                            double fps, CvSize frameSize, int is_color );

CvCapture * cvCreateFileCapture_QT (const char  * filename);
CvCapture * cvCreateCameraCapture_QT  (const int     index);

CvVideoWriter* cvCreateVideoWriter_QT ( const char* filename, int fourcc,
                                        double fps, CvSize frameSize, int is_color );

CvCapture * cvCreateCameraCapture_Unicap  (const int     index);
CvCapture * cvCreateCameraCapture_PvAPI  (const int     index);
CvVideoWriter* cvCreateVideoWriter_GStreamer( const char* filename, int fourcc,
                                            double fps, CvSize frameSize, int is_color );

//Yannick Verdie 2010                                 
double cvGetModeWindow_W32(const char* name);
double cvGetModeWindow_GTK(const char* name);
double cvGetModeWindow_CARBON(const char* name);

void cvSetModeWindow_W32(const char* name, double prop_value);
void cvSetModeWindow_GTK(const char* name, double prop_value);
void cvSetModeWindow_CARBON(const char* name, double prop_value);

//for QT
#if defined (HAVE_QT)
double cvGetModeWindow_QT(const char* name);
void cvSetModeWindow_QT(const char* name, double prop_value);
double cvGetPropWindow_QT(const char* name);
void cvSetPropWindow_QT(const char* name,double prop_value);
double cvGetRatioWindow_QT(const char* name);
void cvSetRatioWindow_QT(const char* name,double prop_value);
#endif

/*namespace cv
{

class CV_EXPORTS BaseWindow
{
public:
    BaseWindow(const String& name, int flags=0);
    virtual ~BaseWindow();
    virtual void close();
    virtual void show(const Mat& mat);
    virtual void resize(Size size);
    virtual void move(Point topleft);
    virtual Size size() const;
    virtual Point topLeft() const;
    virtual void setGeometry(Point topLeft, Size size);
    virtual void getGeometry(Point& topLeft, Size& size) const;
    virtual String getTitle() const;
    virtual void setTitle(const String& str);
    virtual String getName() const;
    virtual void setScaleMode(int mode);
    virtual int getScaleMode();
    virtual void setScrollPos(double pos);
    virtual double getScrollPos() const;
    virtual void setScale(double scale);
    virtual double getScale() const;
    virtual Point getImageCoords(Point pos) const;
    virtual Scalar getPixelValue(Point pos, const String& colorspace=String()) const;

    virtual void addTrackbar( const String& trackbar, int low, int high, int step );
};

typedef Ptr<BaseWindow> Window;

}*/

#endif /* __HIGHGUI_H_ */
