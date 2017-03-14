#include "precomp.hpp"
#include <iterator>

namespace cv
{

static inline char getCloseBrace(char c)
{
    return c == '[' ? ']' : c == '(' ? ')' : c == '{' ? '}' : '\0';
}


template<typename _Tp> static void writeElems(std::ostream& out, const _Tp* data,
                                              int nelems, int cn, char obrace, char cbrace)
{
    typedef typename DataType<_Tp>::work_type _WTp;
    nelems *= cn;
    for(int i = 0; i < nelems; i += cn)
    {
        if(cn == 1)
        {
            out << (_WTp)data[i] << (i+1 < nelems ? ", " : "");
            continue;
        }
        out << obrace;
        for(int j = 0; j < cn; j++)
            out << (_WTp)data[i + j] << (j+1 < cn ? ", " : "");
        out << cbrace << (i+cn < nelems ? ", " : "");
    }
}


static void writeElems(std::ostream& out, const void* data, int nelems, int type, char brace)
{
    int depth = CV_MAT_DEPTH(type), cn = CV_MAT_CN(type);
    char cbrace = ' ';
    if(!brace || isspace(brace))
    {
        nelems *= cn;
        cn = 1;
    }
    else
        cbrace = getCloseBrace(brace);
    if(depth == CV_8U)
        writeElems(out, (const uchar*)data, nelems, cn, brace, cbrace);
    else if(depth == CV_8S)
        writeElems(out, (const schar*)data, nelems, cn, brace, cbrace);
    else if(depth == CV_16U)
        writeElems(out, (const ushort*)data, nelems, cn, brace, cbrace);
    else if(depth == CV_16S)
        writeElems(out, (const short*)data, nelems, cn, brace, cbrace);
    else if(depth == CV_32S)
        writeElems(out, (const int*)data, nelems, cn, brace, cbrace);
    else if(depth == CV_32F)
    {
        std::streamsize pp = out.precision();
        out.precision(8);
        writeElems(out, (const float*)data, nelems, cn, brace, cbrace);
        out.precision(pp);
    }
    else if(depth == CV_64F)
    {
        std::streamsize pp = out.precision();
        out.precision(16);
        writeElems(out, (const double*)data, nelems, cn, brace, cbrace);
        out.precision(pp);
    }
    else
        CV_Error(CV_StsUnsupportedFormat, "");
}


static void writeMat(std::ostream& out, const Mat& m, char rowsep, char elembrace, bool singleLine)
{
    CV_Assert(m.dims <= 2);
    int type = m.type();
    
    char crowbrace = getCloseBrace(rowsep);
    char orowbrace = crowbrace ? rowsep : '\0';
    
    if( orowbrace || isspace(rowsep) )
        rowsep = '\0';
    
    for( int i = 0; i < m.rows; i++ )
    {
        if(orowbrace)
            out << orowbrace;
        if( m.data )
            writeElems(out, m.ptr(i), m.cols, type, elembrace);
        if(orowbrace)
            out << crowbrace << (i+1 < m.rows ? ", " : "");
        if(i+1 < m.rows)
        {
            if(rowsep)
                out << rowsep << (singleLine ? " " : "");
            if(!singleLine)
                out << "\n  ";
        }
    }
}

class MatlabFormatter : public Formatter
{
public:
    virtual ~MatlabFormatter() {}
    void write(std::ostream& out, const Mat& m, const int*, int) const
    {
        out << "[";
        writeMat(out, m, ';', ' ', m.cols == 1);
        out << "]";
    }
    
    void write(std::ostream& out, const void* data, int nelems, int type, const int*, int) const
    {
        writeElems(out, data, nelems, type, ' ');
    }
};

class PythonFormatter : public Formatter
{
public:
    virtual ~PythonFormatter() {}
    void write(std::ostream& out, const Mat& m, const int*, int) const
    {
        out << "[";
        writeMat(out, m, m.cols > 1 ? '[' : ' ', '[', m.cols*m.channels() == 1);
        out << "]";
    }
    
    void write(std::ostream& out, const void* data, int nelems, int type, const int*, int) const
    {
        writeElems(out, data, nelems, type, '[');
    }
};


class NumpyFormatter : public Formatter
{
public:
    virtual ~NumpyFormatter() {}
    void write(std::ostream& out, const Mat& m, const int*, int) const
    {
        static const char* numpyTypes[] =
        {
            "uint8", "int8", "uint16", "int16", "int32", "float32", "float64", "uint64"
        };
        out << "array([";
        writeMat(out, m, m.cols > 1 ? '[' : ' ', '[', m.cols*m.channels() == 1);
        out << "], type='" << numpyTypes[m.depth()] << "')";
    }
    
    void write(std::ostream& out, const void* data, int nelems, int type, const int*, int) const
    {
        writeElems(out, data, nelems, type, '[');
    }
};


class CSVFormatter : public Formatter
{
public:
    virtual ~CSVFormatter() {}
    void write(std::ostream& out, const Mat& m, const int*, int) const
    {
        writeMat(out, m, ' ', ' ', m.cols*m.channels() == 1);
        if(m.rows > 1)
            out << "\n";
    }
    
    void write(std::ostream& out, const void* data, int nelems, int type, const int*, int) const
    {
        writeElems(out, data, nelems, type, ' ');
    }
};


class CFormatter : public Formatter
{
public:
    virtual ~CFormatter() {}
    void write(std::ostream& out, const Mat& m, const int*, int) const
    {
        out << "{";
        writeMat(out, m, ',', ' ', m.cols==1);
        out << "}";
    }
    
    void write(std::ostream& out, const void* data, int nelems, int type, const int*, int) const
    {
        writeElems(out, data, nelems, type, ' ');
    }
};


static MatlabFormatter matlabFormatter;
static PythonFormatter pythonFormatter;
static NumpyFormatter numpyFormatter;
static CSVFormatter csvFormatter;
static CFormatter cFormatter;

static const Formatter* g_defaultFormatter0 = &matlabFormatter;
static const Formatter* g_defaultFormatter = &matlabFormatter;

bool my_streq(const char* a, const char* b)
{
    size_t i, alen = strlen(a), blen = strlen(b);
    if( alen != blen )
        return false;
    for( i = 0; i < alen; i++ )
        if( a[i] != b[i] && a[i] - 32 != b[i] )
            return false;
    return true;
}

const Formatter* Formatter::get(const char* fmt)
{
    if(!fmt || my_streq(fmt, ""))
        return g_defaultFormatter;
    if( my_streq(fmt, "MATLAB"))
        return &matlabFormatter;
    if( my_streq(fmt, "CSV"))
        return &csvFormatter;
    if( my_streq(fmt, "PYTHON"))
        return &pythonFormatter;
    if( my_streq(fmt, "NUMPY"))
        return &numpyFormatter;
    if( my_streq(fmt, "C"))
        return &cFormatter;
    CV_Error(CV_StsBadArg, "Unknown formatter");
    return g_defaultFormatter;
}

const Formatter* Formatter::setDefault(const Formatter* fmt)
{
    const Formatter* prevFmt = g_defaultFormatter;
    if(!fmt)
        fmt = g_defaultFormatter0;
    g_defaultFormatter = fmt;
    return prevFmt;
}
    
Formatted::Formatted(const Mat& _m, const Formatter* _fmt,
                     const vector<int>& _params)
{
    mtx = _m;
    fmt = _fmt ? _fmt : Formatter::get();
    std::copy(_params.begin(), _params.end(), back_inserter(params));
}
    
Formatted::Formatted(const Mat& _m, const Formatter* _fmt, const int* _params)
{
    mtx = _m;
    fmt = _fmt ? _fmt : Formatter::get();
    
    if( _params )
    {
        int i, maxParams = 100;
        for(i = 0; i < maxParams && _params[i] != 0; i+=2)
            ;
        std::copy(_params, _params + i, back_inserter(params));
    }
}

}

