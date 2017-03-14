#ifndef _FEATURES_2D_HPP_
#define _FEATURES_2D_HPP_

#include "../../opencvlib/core/core.hpp"
#include "../../opencvlib/flann/miniflann.hpp"

#ifdef __cplusplus
#include <limits>


namespace cv
{
    struct CV_EXPORTS DefaultRngAuto
    {
        const uint64 old_state;

        DefaultRngAuto() : old_state(theRNG().state) { theRNG().state = (uint64)-1; }
        ~DefaultRngAuto() { theRNG().state = old_state; }

        DefaultRngAuto& operator=(const DefaultRngAuto&);
    };


// CvAffinePose: defines a parameterized affine transformation of an image patch.
// An image patch is rotated on angle phi (in degrees), then scaled lambda1 times
// along horizontal and lambda2 times along vertical direction, and then rotated again
// on angle (theta - phi).
class CV_EXPORTS CvAffinePose
{
public:
    float phi;
    float theta;
    float lambda1;
    float lambda2;
};

/*!
 The Keypoint Class
 
 The class instance stores a keypoint, i.e. a point feature found by one of many available keypoint detectors, such as
 Harris corner detector, cv::FAST, cv::StarDetector, cv::SURF, cv::SIFT, cv::LDetector etc.
 
 The keypoint is characterized by the 2D position, scale
 (proportional to the diameter of the neighborhood that needs to be taken into account),
 orientation and some other parameters. The keypoint neighborhood is then analyzed by another algorithm that builds a descriptor
 (usually represented as a feature vector). The keypoints representing the same object in different images can then be matched using
 cv::KDTree or another method.
*/
class CV_EXPORTS_W_SIMPLE KeyPoint
{
public:
    //! the default constructor
    CV_WRAP KeyPoint() : pt(0,0), size(0), angle(-1), response(0), octave(0), class_id(-1) {}
    //! the full constructor
    KeyPoint(Point2f _pt, float _size, float _angle=-1,
            float _response=0, int _octave=0, int _class_id=-1)
            : pt(_pt), size(_size), angle(_angle),
            response(_response), octave(_octave), class_id(_class_id) {}
    //! another form of the full constructor
    CV_WRAP KeyPoint(float x, float y, float _size, float _angle=-1,
            float _response=0, int _octave=0, int _class_id=-1)
            : pt(x, y), size(_size), angle(_angle),
            response(_response), octave(_octave), class_id(_class_id) {}
    
    size_t hash() const;
    
    //! converts vector of keypoints to vector of points
    static void convert(const std::vector<KeyPoint>& keypoints,
                        CV_OUT std::vector<Point2f>& points2f,
                        const std::vector<int>& keypointIndexes=std::vector<int>());
    //! converts vector of points to the vector of keypoints, where each keypoint is assigned the same size and the same orientation
    static void convert(const std::vector<Point2f>& points2f,
                        CV_OUT std::vector<KeyPoint>& keypoints,
                        float size=1, float response=1, int octave=0, int class_id=-1);

    //! computes overlap for pair of keypoints;
    //! overlap is a ratio between area of keypoint regions intersection and
    //! area of keypoint regions union (now keypoint region is circle)
    static float overlap(const KeyPoint& kp1, const KeyPoint& kp2);

    CV_PROP_RW Point2f pt; //!< coordinates of the keypoints
    CV_PROP_RW float size; //!< diameter of the meaningful keypoint neighborhood
    CV_PROP_RW float angle; //!< computed orientation of the keypoint (-1 if not applicable)
    CV_PROP_RW float response; //!< the response by which the most strong keypoints have been selected. Can be used for the further sorting or subsampling
    CV_PROP_RW int octave; //!< octave (pyramid layer) from which the keypoint has been extracted
    CV_PROP_RW int class_id; //!< object class (if the keypoints need to be clustered by an object they belong to) 
};
    
//! writes vector of keypoints to the file storage
CV_EXPORTS void write(FileStorage& fs, const string& name, const vector<KeyPoint>& keypoints);
//! reads vector of keypoints from the specified file storage node
CV_EXPORTS void read(const FileNode& node, CV_OUT vector<KeyPoint>& keypoints);    

/*
 * A class filters a vector of keypoints.
 * Because now it is difficult to provide a convenient interface for all usage scenarios of the keypoints filter class,
 * it has only 4 needed by now static methods.
 */
class CV_EXPORTS KeyPointsFilter
{
public:
    KeyPointsFilter(){}

    /*
     * Remove keypoints within borderPixels of an image edge.
     */
    static void runByImageBorder( vector<KeyPoint>& keypoints, Size imageSize, int borderSize );
    /*
     * Remove keypoints of sizes out of range.
     */
    static void runByKeypointSize( vector<KeyPoint>& keypoints, float minSize, float maxSize=std::numeric_limits<float>::max() );
    /*
     * Remove keypoints from some image by mask for pixels of this image.
     */
    static void runByPixelsMask( vector<KeyPoint>& keypoints, const Mat& mask );
    /*
     * Remove duplicated keypoints.
     */
    static void removeDuplicated( vector<KeyPoint>& keypoints );
};

/****************************************************************************************\
*                                    FeatureDetector                                     *
\****************************************************************************************/

/*
 * Abstract base class for 2D image feature detectors.
 */
class CV_EXPORTS FeatureDetector
{
public:
    virtual ~FeatureDetector();
    
    /*
     * Detect keypoints in an image.
     * image        The image.
     * keypoints    The detected keypoints.
     * mask         Mask specifying where to look for keypoints (optional). Must be a char
     *              matrix with non-zero values in the region of interest.
     */
    void detect( const Mat& image, vector<KeyPoint>& keypoints, const Mat& mask=Mat() ) const;
    
    /*
     * Detect keypoints in an image set.
     * images       Image collection.
     * keypoints    Collection of keypoints detected in an input images. keypoints[i] is a set of keypoints detected in an images[i].
     * masks        Masks for image set. masks[i] is a mask for images[i].
     */
    void detect( const vector<Mat>& images, vector<vector<KeyPoint> >& keypoints, const vector<Mat>& masks=vector<Mat>() ) const;

    // Read detector object from a file node.
    virtual void read( const FileNode& );
    // Read detector object from a file node.
    virtual void write( FileStorage& ) const;

    // Return true if detector object is empty
    virtual bool empty() const;

    // Create feature detector by detector name.
    static Ptr<FeatureDetector> create( const string& detectorType );

protected:
    virtual void detectImpl( const Mat& image, vector<KeyPoint>& keypoints, const Mat& mask=Mat() ) const = 0;

    /*
     * Remove keypoints that are not in the mask.
     * Helper function, useful when wrapping a library call for keypoint detection that
     * does not support a mask argument.
     */
    static void removeInvalidPoints( const Mat& mask, vector<KeyPoint>& keypoints );
};


/****************************************************************************************\
*                                 DescriptorExtractor                                    *
\****************************************************************************************/

/*
 * Abstract base class for computing descriptors for image keypoints.
 *
 * In this interface we assume a keypoint descriptor can be represented as a
 * dense, fixed-dimensional vector of some basic type. Most descriptors used
 * in practice follow this pattern, as it makes it very easy to compute
 * distances between descriptors. Therefore we represent a collection of
 * descriptors as a cv::Mat, where each row is one keypoint descriptor.
 */
class CV_EXPORTS DescriptorExtractor
{
public:
    virtual ~DescriptorExtractor();

    /*
     * Compute the descriptors for a set of keypoints in an image.
     * image        The image.
     * keypoints    The input keypoints. Keypoints for which a descriptor cannot be computed are removed.
     * descriptors  Copmputed descriptors. Row i is the descriptor for keypoint i.
     */
    void compute( const Mat& image, vector<KeyPoint>& keypoints, Mat& descriptors ) const;

    /*
     * Compute the descriptors for a keypoints collection detected in image collection.
     * images       Image collection.
     * keypoints    Input keypoints collection. keypoints[i] is keypoints detected in images[i].
     *              Keypoints for which a descriptor cannot be computed are removed.
     * descriptors  Descriptor collection. descriptors[i] are descriptors computed for set keypoints[i].
     */
    void compute( const vector<Mat>& images, vector<vector<KeyPoint> >& keypoints, vector<Mat>& descriptors ) const;

    virtual void read( const FileNode& );
    virtual void write( FileStorage& ) const;

    virtual int descriptorSize() const = 0;
    virtual int descriptorType() const = 0;

    virtual bool empty() const;

    static Ptr<DescriptorExtractor> create( const string& descriptorExtractorType );

protected:
    virtual void computeImpl( const Mat& image, vector<KeyPoint>& keypoints, Mat& descriptors ) const = 0;

    /*
     * Remove keypoints within borderPixels of an image edge.
     */
    static void removeBorderKeypoints( vector<KeyPoint>& keypoints,
                                       Size imageSize, int borderSize );
};

/****************************************************************************************\
*                                          Distance                                      *
\****************************************************************************************/
template<typename T>
struct CV_EXPORTS Accumulator
{
    typedef T Type;
};

template<> struct Accumulator<unsigned char>  { typedef float Type; };
template<> struct Accumulator<unsigned short> { typedef float Type; };
template<> struct Accumulator<char>   { typedef float Type; };
template<> struct Accumulator<short>  { typedef float Type; };

/*
 * Squared Euclidean distance functor
 */
template<class T>
struct CV_EXPORTS SL2
{
    typedef T ValueType;
    typedef typename Accumulator<T>::Type ResultType;

    ResultType operator()( const T* a, const T* b, int size ) const
    {
        ResultType result = ResultType();
        for( int i = 0; i < size; i++ )
        {
            ResultType diff = (ResultType)(a[i] - b[i]);
            result += diff*diff;
        }
        return result;
    }
};

/*
 * Euclidean distance functor
 */
template<class T>
struct CV_EXPORTS L2
{
    typedef T ValueType;
    typedef typename Accumulator<T>::Type ResultType;

    ResultType operator()( const T* a, const T* b, int size ) const
    {
        ResultType result = ResultType();
        for( int i = 0; i < size; i++ )
        {
            ResultType diff = (ResultType)(a[i] - b[i]);
            result += diff*diff;
        }
        return (ResultType)sqrt((double)result);
    }
};

/*
 * Manhattan distance (city block distance) functor
 */
template<class T>
struct CV_EXPORTS L1
{
    typedef T ValueType;
    typedef typename Accumulator<T>::Type ResultType;

    ResultType operator()( const T* a, const T* b, int size ) const
    {
        ResultType result = ResultType();
        for( int i = 0; i < size; i++ )
        {
            ResultType diff = a[i] - b[i];
            result += (ResultType)fabs( diff );
        }
        return result;
    }
};
/****************************************************************************************\
*                                      DMatch                                            *
\****************************************************************************************/
/*
 * Struct for matching: query descriptor index, train descriptor index, train image index and distance between descriptors.
 */
struct CV_EXPORTS DMatch
{
    DMatch() : queryIdx(-1), trainIdx(-1), imgIdx(-1), distance(std::numeric_limits<float>::max()) {}
    DMatch( int _queryIdx, int _trainIdx, float _distance ) :
            queryIdx(_queryIdx), trainIdx(_trainIdx), imgIdx(-1), distance(_distance) {}
    DMatch( int _queryIdx, int _trainIdx, int _imgIdx, float _distance ) :
            queryIdx(_queryIdx), trainIdx(_trainIdx), imgIdx(_imgIdx), distance(_distance) {}

    int queryIdx; // query descriptor index
    int trainIdx; // train descriptor index
    int imgIdx;   // train image index

    float distance;

    // less is better
    bool operator<( const DMatch &m ) const
    {
        return distance < m.distance;
    }
};

/****************************************************************************************\
*                                  DescriptorMatcher                                     *
\****************************************************************************************/
/*
 * Abstract base class for matching two sets of descriptors.
 */
class CV_EXPORTS DescriptorMatcher
{
public:
    virtual ~DescriptorMatcher();

	/*
     * Add descriptors to train descriptor collection.
     * descriptors      Descriptors to add. Each descriptors[i] is a descriptors set from one image.
     */
    virtual void add( const vector<Mat>& descriptors );
    /*
     * Get train descriptors collection.
     */
    const vector<Mat>& getTrainDescriptors() const;
    /*
     * Clear train descriptors collection.
     */
    virtual void clear();

    /*
     * Return true if there are not train descriptors in collection.
     */
    virtual bool empty() const;
    /*
     * Return true if the matcher supports mask in match methods.
     */
    virtual bool isMaskSupported() const = 0;

    /*
     * Train matcher (e.g. train flann index).
     * In all methods to match the method train() is run every time before matching.
     * Some descriptor matchers (e.g. BruteForceMatcher) have empty implementation
     * of this method, other matchers really train their inner structures
     * (e.g. FlannBasedMatcher trains flann::Index). So nonempty implementation
     * of train() should check the class object state and do traing/retraining
     * only if the state requires that (e.g. FlannBasedMatcher trains flann::Index
     * if it has not trained yet or if new descriptors have been added to the train
     * collection).
     */
    virtual void train();
    /*
     * Group of methods to match descriptors from image pair.
     * Method train() is run in this methods.
     */
    // Find one best match for each query descriptor (if mask is empty).
    void match( const Mat& queryDescriptors, const Mat& trainDescriptors,
                vector<DMatch>& matches, const Mat& mask=Mat() ) const;
    // Find k best matches for each query descriptor (in increasing order of distances).
    // compactResult is used when mask is not empty. If compactResult is false matches
    // vector will have the same size as queryDescriptors rows. If compactResult is true
    // matches vector will not contain matches for fully masked out query descriptors.
    void knnMatch( const Mat& queryDescriptors, const Mat& trainDescriptors,
                   vector<vector<DMatch> >& matches, int k,
                   const Mat& mask=Mat(), bool compactResult=false ) const;
    // Find best matches for each query descriptor which have distance less than
    // maxDistance (in increasing order of distances).
    void radiusMatch( const Mat& queryDescriptors, const Mat& trainDescriptors,
                      vector<vector<DMatch> >& matches, float maxDistance,
                      const Mat& mask=Mat(), bool compactResult=false ) const;
    /*
     * Group of methods to match descriptors from one image to image set.
     * See description of similar methods for matching image pair above.
     */
    void match( const Mat& queryDescriptors, vector<DMatch>& matches,
                const vector<Mat>& masks=vector<Mat>() );
    void knnMatch( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, int k,
           const vector<Mat>& masks=vector<Mat>(), bool compactResult=false );
    void radiusMatch( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, float maxDistance,
                   const vector<Mat>& masks=vector<Mat>(), bool compactResult=false );

    // Reads matcher object from a file node
    virtual void read( const FileNode& );
    // Writes matcher object to a file storage
    virtual void write( FileStorage& ) const;

    // Clone the matcher. If emptyTrainData is false the method create deep copy of the object, i.e. copies
    // both parameters and train data. If emptyTrainData is true the method create object copy with current parameters
    // but with empty train data.
    virtual Ptr<DescriptorMatcher> clone( bool emptyTrainData=false ) const = 0;

    static Ptr<DescriptorMatcher> create( const string& descriptorMatcherType );
protected:
    /*
     * Class to work with descriptors from several images as with one merged matrix.
     * It is used e.g. in FlannBasedMatcher.
     */
    class CV_EXPORTS DescriptorCollection
    {
    public:
        DescriptorCollection();
        DescriptorCollection( const DescriptorCollection& collection );
        virtual ~DescriptorCollection();

        // Vector of matrices "descriptors" will be merged to one matrix "mergedDescriptors" here.
        void set( const vector<Mat>& descriptors );
        virtual void clear();

        const Mat& getDescriptors() const;
        const Mat getDescriptor( int imgIdx, int localDescIdx ) const;
        const Mat getDescriptor( int globalDescIdx ) const;
        void getLocalIdx( int globalDescIdx, int& imgIdx, int& localDescIdx ) const;

        int size() const;

    protected:
        Mat mergedDescriptors;
        vector<int> startIdxs;
    };

    // In fact the matching is implemented only by the following two methods. These methods suppose
    // that the class object has been trained already. Public match methods call these methods
    // after calling train().
    virtual void knnMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, int k,
           const vector<Mat>& masks=vector<Mat>(), bool compactResult=false ) = 0;
    virtual void radiusMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, float maxDistance,
           const vector<Mat>& masks=vector<Mat>(), bool compactResult=false ) = 0;

    static bool isPossibleMatch( const Mat& mask, int queryIdx, int trainIdx );
    static bool isMaskedOut( const vector<Mat>& masks, int queryIdx );

    static Mat clone_op( Mat m ) { return m.clone(); }
	void checkMasks( const vector<Mat>& masks, int queryDescriptorsCount ) const;

    // Collection of descriptors from train images.
    vector<Mat> trainDescCollection;
};

/*
 * Brute-force descriptor matcher.
 *
 * For each descriptor in the first set, this matcher finds the closest
 * descriptor in the second set by trying each one.
 *
 * For efficiency, BruteForceMatcher is templated on the distance metric.
 * For float descriptors, a common choice would be cv::L2<float>.
 */
template<class Distance>
class CV_EXPORTS BruteForceMatcher : public DescriptorMatcher
{
public:
    BruteForceMatcher( Distance d = Distance() ) : distance(d) {}
    virtual ~BruteForceMatcher() {}

    virtual bool isMaskSupported() const { return true; }

    virtual Ptr<DescriptorMatcher> clone( bool emptyTrainData=false ) const;

protected:
    virtual void knnMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, int k,
           const vector<Mat>& masks=vector<Mat>(), bool compactResult=false );
    virtual void radiusMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, float maxDistance,
           const vector<Mat>& masks=vector<Mat>(), bool compactResult=false );

    Distance distance;

private:
    /*
     * Next two methods are used to implement specialization.
     */
    static void commonKnnMatchImpl( BruteForceMatcher<Distance>& matcher,
                    const Mat& queryDescriptors, vector<vector<DMatch> >& matches, int k,
                    const vector<Mat>& masks, bool compactResult );
    static void commonRadiusMatchImpl( BruteForceMatcher<Distance>& matcher,
                    const Mat& queryDescriptors, vector<vector<DMatch> >& matches, float maxDistance,
                    const vector<Mat>& masks, bool compactResult );
};

template<class Distance>
Ptr<DescriptorMatcher> BruteForceMatcher<Distance>::clone( bool emptyTrainData ) const
{
    BruteForceMatcher* matcher = new BruteForceMatcher(distance);
    if( !emptyTrainData )
    {
        matcher->trainDescCollection.resize(trainDescCollection.size());
        std::transform( trainDescCollection.begin(), trainDescCollection.end(),
                        matcher->trainDescCollection.begin(), clone_op );
    }
    return matcher;
}

template<class Distance>
void BruteForceMatcher<Distance>::knnMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, int k,
                                                const vector<Mat>& masks, bool compactResult )
{
    commonKnnMatchImpl( *this, queryDescriptors, matches, k, masks, compactResult );
}

template<class Distance>
void BruteForceMatcher<Distance>::radiusMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches,
                                                   float maxDistance, const vector<Mat>& masks, bool compactResult )
{
    commonRadiusMatchImpl( *this, queryDescriptors, matches, maxDistance, masks, compactResult );
}

template<class Distance>
inline void BruteForceMatcher<Distance>::commonKnnMatchImpl( BruteForceMatcher<Distance>& matcher,
                          const Mat& queryDescriptors, vector<vector<DMatch> >& matches, int knn,
                          const vector<Mat>& masks, bool compactResult )
{
    typedef typename Distance::ValueType ValueType;
    typedef typename Distance::ResultType DistanceType;
    CV_DbgAssert( !queryDescriptors.empty() );
    CV_Assert( DataType<ValueType>::type == queryDescriptors.type() );
     
    int dimension = queryDescriptors.cols;
    matches.reserve(queryDescriptors.rows);

    size_t imgCount = matcher.trainDescCollection.size();
    vector<Mat> allDists( imgCount ); // distances between one query descriptor and all train descriptors
    for( size_t i = 0; i < imgCount; i++ )
        allDists[i] = Mat( 1, matcher.trainDescCollection[i].rows, DataType<DistanceType>::type );

    for( int qIdx = 0; qIdx < queryDescriptors.rows; qIdx++ )
    {
        if( matcher.isMaskedOut( masks, qIdx ) )
        {
            if( !compactResult ) // push empty vector
                matches.push_back( vector<DMatch>() );
        }
        else
        {
            // 1. compute distances between i-th query descriptor and all train descriptors
            for( size_t iIdx = 0; iIdx < imgCount; iIdx++ )
            {
                CV_Assert( DataType<ValueType>::type == matcher.trainDescCollection[iIdx].type() ||  matcher.trainDescCollection[iIdx].empty() );
                CV_Assert( queryDescriptors.cols == matcher.trainDescCollection[iIdx].cols ||
                           matcher.trainDescCollection[iIdx].empty() );

                const ValueType* d1 = (const ValueType*)(queryDescriptors.data + queryDescriptors.step*qIdx);
                allDists[iIdx].setTo( Scalar::all(std::numeric_limits<DistanceType>::max()) );
                for( int tIdx = 0; tIdx < matcher.trainDescCollection[iIdx].rows; tIdx++ )
                {
                    if( masks.empty() || matcher.isPossibleMatch(masks[iIdx], qIdx, tIdx) )
                    {
                        const ValueType* d2 = (const ValueType*)(matcher.trainDescCollection[iIdx].data +
                                                                 matcher.trainDescCollection[iIdx].step*tIdx);
                        allDists[iIdx].at<DistanceType>(0, tIdx) = matcher.distance(d1, d2, dimension);
                    }
                }
            }

            // 2. choose k nearest matches for query[i]
            matches.push_back( vector<DMatch>() );
            vector<vector<DMatch> >::reverse_iterator curMatches = matches.rbegin();
            for( int k = 0; k < knn; k++ )
            {
                DMatch bestMatch;
                bestMatch.distance = std::numeric_limits<float>::max();
                for( size_t iIdx = 0; iIdx < imgCount; iIdx++ )
                {
                    if( !allDists[iIdx].empty() )
                    {
                        double minVal;
                        Point minLoc;
                        minMaxLoc( allDists[iIdx], &minVal, 0, &minLoc, 0 );
                        if( minVal < bestMatch.distance )
                            bestMatch = DMatch( qIdx, minLoc.x, (int)iIdx, (float)minVal );
                    }
                }
                if( bestMatch.trainIdx == -1 )
                    break;

                allDists[bestMatch.imgIdx].at<DistanceType>(0, bestMatch.trainIdx) = std::numeric_limits<DistanceType>::max();
                curMatches->push_back( bestMatch );
            }
            //TODO should already be sorted at this point?
            std::sort( curMatches->begin(), curMatches->end() );
        }
    }
}

template<class Distance>
inline void BruteForceMatcher<Distance>::commonRadiusMatchImpl( BruteForceMatcher<Distance>& matcher,
                             const Mat& queryDescriptors, vector<vector<DMatch> >& matches, float maxDistance,
                             const vector<Mat>& masks, bool compactResult )
{
    typedef typename Distance::ValueType ValueType;
    typedef typename Distance::ResultType DistanceType;
	CV_DbgAssert( !queryDescriptors.empty() );
    CV_Assert( DataType<ValueType>::type == queryDescriptors.type() );
    
    int dimension = queryDescriptors.cols;
    matches.reserve(queryDescriptors.rows);

    size_t imgCount = matcher.trainDescCollection.size();
    for( int qIdx = 0; qIdx < queryDescriptors.rows; qIdx++ )
    {
        if( matcher.isMaskedOut( masks, qIdx ) )
        {
            if( !compactResult ) // push empty vector
                matches.push_back( vector<DMatch>() );
        }
        else
        {
            matches.push_back( vector<DMatch>() );
            vector<vector<DMatch> >::reverse_iterator curMatches = matches.rbegin();
            for( size_t iIdx = 0; iIdx < imgCount; iIdx++ )
            {
                CV_Assert( DataType<ValueType>::type == matcher.trainDescCollection[iIdx].type() ||
                           matcher.trainDescCollection[iIdx].empty() );
                CV_Assert( queryDescriptors.cols == matcher.trainDescCollection[iIdx].cols ||
						   matcher.trainDescCollection[iIdx].empty() );

                const ValueType* d1 = (const ValueType*)(queryDescriptors.data + queryDescriptors.step*qIdx);
                for( int tIdx = 0; tIdx < matcher.trainDescCollection[iIdx].rows; tIdx++ )
                {
                    if( masks.empty() || matcher.isPossibleMatch(masks[iIdx], qIdx, tIdx) )
                    {
                        const ValueType* d2 = (const ValueType*)(matcher.trainDescCollection[iIdx].data +
                                                                 matcher.trainDescCollection[iIdx].step*tIdx);
                        DistanceType d = matcher.distance(d1, d2, dimension);
                        if( d < maxDistance )
                            curMatches->push_back( DMatch( qIdx, tIdx, (int)iIdx, (float)d ) );
                    }
                }
            }
            std::sort( curMatches->begin(), curMatches->end() );
        }
    }
}

/*
 * BruteForceMatcher L2 specialization
 */
template<>
void BruteForceMatcher<L2<float> >::knnMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, int k,
                                                  const vector<Mat>& masks, bool compactResult );
template<>
void BruteForceMatcher<L2<float> >::radiusMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches,
                                                     float maxDistance, const vector<Mat>& masks, bool compactResult );

/*
 * Flann based matcher
 */
class CV_EXPORTS FlannBasedMatcher : public DescriptorMatcher
{
public:
    FlannBasedMatcher( const Ptr<flann::IndexParams>& indexParams=new flann::KDTreeIndexParams(),
                       const Ptr<flann::SearchParams>& searchParams=new flann::SearchParams() );

    virtual void add( const vector<Mat>& descriptors );
    virtual void clear();

    // Reads matcher object from a file node
    virtual void read( const FileNode& );
    // Writes matcher object to a file storage
    virtual void write( FileStorage& ) const;

    virtual void train();
    virtual bool isMaskSupported() const;
	
    virtual Ptr<DescriptorMatcher> clone( bool emptyTrainData=false ) const;

protected:
    static void convertToDMatches( const DescriptorCollection& descriptors,
                                   const Mat& indices, const Mat& distances,
                                   vector<vector<DMatch> >& matches );

    virtual void knnMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, int k,
                   const vector<Mat>& masks=vector<Mat>(), bool compactResult=false );
    virtual void radiusMatchImpl( const Mat& queryDescriptors, vector<vector<DMatch> >& matches, float maxDistance,
                   const vector<Mat>& masks=vector<Mat>(), bool compactResult=false );

    Ptr<flann::IndexParams> indexParams;
    Ptr<flann::SearchParams> searchParams;
    Ptr<flann::Index> flannIndex;

    DescriptorCollection mergedDescriptors;
    int addedDescCount;
};

/****************************************************************************************\
*                                GenericDescriptorMatcher                                *
\****************************************************************************************/
/*
 *   Abstract interface for a keypoint descriptor and matcher
 */
class GenericDescriptorMatcher;
typedef GenericDescriptorMatcher GenericDescriptorMatch;

class CV_EXPORTS GenericDescriptorMatcher
{
public:
    GenericDescriptorMatcher();
    virtual ~GenericDescriptorMatcher();

    /*
     * Add train collection: images and keypoints from them.
     * images       A set of train images.
     * ketpoints    Keypoint collection that have been detected on train images.
     *
     * Keypoints for which a descriptor cannot be computed are removed. Such keypoints
     * must be filtered in this method befor adding keypoints to train collection "trainPointCollection".
     * If inheritor class need perform such prefiltering the method add() must be overloaded.
     * In the other class methods programmer has access to the train keypoints by a constant link.
     */
    virtual void add( const vector<Mat>& images,
                      vector<vector<KeyPoint> >& keypoints );

    const vector<Mat>& getTrainImages() const;
    const vector<vector<KeyPoint> >& getTrainKeypoints() const;

    /*
     * Clear images and keypoints storing in train collection.
     */
    virtual void clear();
    /*
     * Returns true if matcher supports mask to match descriptors.
     */
    virtual bool isMaskSupported() = 0;
    /*
     * Train some inner structures (e.g. flann index or decision trees).
     * train() methods is run every time in matching methods. So the method implementation
     * should has a check whether these inner structures need be trained/retrained or not.
     */
    virtual void train();

    /*
     * Classifies query keypoints.
     * queryImage    The query image
     * queryKeypoints   Keypoints from the query image
     * trainImage    The train image
     * trainKeypoints   Keypoints from the train image
     */
    // Classify keypoints from query image under one train image.
    void classify( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                           const Mat& trainImage, vector<KeyPoint>& trainKeypoints ) const;
    // Classify keypoints from query image under train image collection.
    void classify( const Mat& queryImage, vector<KeyPoint>& queryKeypoints );

    /*
     * Group of methods to match keypoints from image pair.
     * Keypoints for which a descriptor cannot be computed are removed.
     * train() method is called here.
     */
    // Find one best match for each query descriptor (if mask is empty).
    void match( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                const Mat& trainImage, vector<KeyPoint>& trainKeypoints,
                vector<DMatch>& matches, const Mat& mask=Mat() ) const;
    // Find k best matches for each query keypoint (in increasing order of distances).
    // compactResult is used when mask is not empty. If compactResult is false matches
    // vector will have the same size as queryDescriptors rows.
    // If compactResult is true matches vector will not contain matches for fully masked out query descriptors.
    void knnMatch( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                   const Mat& trainImage, vector<KeyPoint>& trainKeypoints,
                   vector<vector<DMatch> >& matches, int k,
                   const Mat& mask=Mat(), bool compactResult=false ) const;
    // Find best matches for each query descriptor which have distance less than maxDistance (in increasing order of distances).
    void radiusMatch( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                      const Mat& trainImage, vector<KeyPoint>& trainKeypoints,
                      vector<vector<DMatch> >& matches, float maxDistance,
                      const Mat& mask=Mat(), bool compactResult=false ) const;
    /*
     * Group of methods to match keypoints from one image to image set.
     * See description of similar methods for matching image pair above.
     */
    void match( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                vector<DMatch>& matches, const vector<Mat>& masks=vector<Mat>() );
    void knnMatch( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                   vector<vector<DMatch> >& matches, int k,
                   const vector<Mat>& masks=vector<Mat>(), bool compactResult=false );
    void radiusMatch( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                      vector<vector<DMatch> >& matches, float maxDistance,
                      const vector<Mat>& masks=vector<Mat>(), bool compactResult=false );

    // Reads matcher object from a file node
    virtual void read( const FileNode& );
    // Writes matcher object to a file storage
    virtual void write( FileStorage& ) const;

    // Return true if matching object is empty (e.g. feature detector or descriptor matcher are empty)
    virtual bool empty() const;

    // Clone the matcher. If emptyTrainData is false the method create deep copy of the object, i.e. copies
    // both parameters and train data. If emptyTrainData is true the method create object copy with current parameters
    // but with empty train data.
    virtual Ptr<GenericDescriptorMatcher> clone( bool emptyTrainData=false ) const = 0;

    static Ptr<GenericDescriptorMatcher> create( const string& genericDescritptorMatcherType,
                                                 const string &paramsFilename=string() );

protected:
    // In fact the matching is implemented only by the following two methods. These methods suppose
    // that the class object has been trained already. Public match methods call these methods
    // after calling train().
    virtual void knnMatchImpl( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                               vector<vector<DMatch> >& matches, int k,
                               const vector<Mat>& masks, bool compactResult ) = 0;
    virtual void radiusMatchImpl( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                                  vector<vector<DMatch> >& matches, float maxDistance,
                                  const vector<Mat>& masks, bool compactResult ) = 0;
    /*
     * A storage for sets of keypoints together with corresponding images and class IDs
     */
    class CV_EXPORTS KeyPointCollection
    {
    public:
        KeyPointCollection();
        KeyPointCollection( const KeyPointCollection& collection );
        void add( const vector<Mat>& images, const vector<vector<KeyPoint> >& keypoints );
        void clear();

        // Returns the total number of keypoints in the collection
        size_t keypointCount() const;
        size_t imageCount() const;

        const vector<vector<KeyPoint> >& getKeypoints() const;
        const vector<KeyPoint>& getKeypoints( int imgIdx ) const;
        const KeyPoint& getKeyPoint( int imgIdx, int localPointIdx ) const;
        const KeyPoint& getKeyPoint( int globalPointIdx ) const;
        void getLocalIdx( int globalPointIdx, int& imgIdx, int& localPointIdx ) const;

        const vector<Mat>& getImages() const;
        const Mat& getImage( int imgIdx ) const;

    protected:
        int pointCount;

        vector<Mat> images;
        vector<vector<KeyPoint> > keypoints;
        // global indices of the first points in each image, startIndices.size() = keypoints.size()
        vector<int> startIndices;

    private:
        static Mat clone_op( Mat m ) { return m.clone(); }
    };

    KeyPointCollection trainPointCollection;
};


/****************************************************************************************\
*                                VectorDescriptorMatcher                                 *
\****************************************************************************************/

/*
 *  A class used for matching descriptors that can be described as vectors in a finite-dimensional space
 */
class VectorDescriptorMatcher;
typedef VectorDescriptorMatcher VectorDescriptorMatch;

class CV_EXPORTS VectorDescriptorMatcher : public GenericDescriptorMatcher
{
public:
    VectorDescriptorMatcher( const Ptr<DescriptorExtractor>& extractor, const Ptr<DescriptorMatcher>& matcher );
    virtual ~VectorDescriptorMatcher();

    virtual void add( const vector<Mat>& imgCollection,
                      vector<vector<KeyPoint> >& pointCollection );

    virtual void clear();

    virtual void train();

    virtual bool isMaskSupported();

    virtual void read( const FileNode& fn );
    virtual void write( FileStorage& fs ) const;
    virtual bool empty() const;

    virtual Ptr<GenericDescriptorMatcher> clone( bool emptyTrainData=false ) const;

protected:
    virtual void knnMatchImpl( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                               vector<vector<DMatch> >& matches, int k,
                               const vector<Mat>& masks, bool compactResult );
    virtual void radiusMatchImpl( const Mat& queryImage, vector<KeyPoint>& queryKeypoints,
                                  vector<vector<DMatch> >& matches, float maxDistance,
                                  const vector<Mat>& masks, bool compactResult );

    Ptr<DescriptorExtractor> extractor;
    Ptr<DescriptorMatcher> matcher;
};

/****************************************************************************************\
*                                   Drawing functions                                    *
\****************************************************************************************/
struct CV_EXPORTS DrawMatchesFlags
{
    enum{ DEFAULT = 0, // Output image matrix will be created (Mat::create),
                       // i.e. existing memory of output image may be reused.
                       // Two source image, matches and single keypoints will be drawn.
                       // For each keypoint only the center point will be drawn (without
                       // the circle around keypoint with keypoint size and orientation).
          DRAW_OVER_OUTIMG = 1, // Output image matrix will not be created (Mat::create).
                                // Matches will be drawn on existing content of output image.
          NOT_DRAW_SINGLE_POINTS = 2, // Single keypoints will not be drawn.
          DRAW_RICH_KEYPOINTS = 4 // For each keypoint the circle around keypoint with keypoint size and
                                  // orientation will be drawn.
        };
};

// Draw keypoints.
CV_EXPORTS void drawKeypoints( const Mat& image, const vector<KeyPoint>& keypoints, Mat& outImage,
                               const Scalar& color=Scalar::all(-1), int flags=DrawMatchesFlags::DEFAULT );

// Draws matches of keypints from two images on output image.
CV_EXPORTS void drawMatches( const Mat& img1, const vector<KeyPoint>& keypoints1,
                             const Mat& img2, const vector<KeyPoint>& keypoints2,
                             const vector<DMatch>& matches1to2, Mat& outImg,
                             const Scalar& matchColor=Scalar::all(-1), const Scalar& singlePointColor=Scalar::all(-1),
                             const vector<char>& matchesMask=vector<char>(), int flags=DrawMatchesFlags::DEFAULT );

CV_EXPORTS void drawMatches( const Mat& img1, const vector<KeyPoint>& keypoints1,
                             const Mat& img2, const vector<KeyPoint>& keypoints2,
                             const vector<vector<DMatch> >& matches1to2, Mat& outImg,
                             const Scalar& matchColor=Scalar::all(-1), const Scalar& singlePointColor=Scalar::all(-1),
                             const vector<vector<char> >& matchesMask=vector<vector<char> >(), int flags=DrawMatchesFlags::DEFAULT );

/****************************************************************************************\
*   Functions to evaluate the feature detectors and [generic] descriptor extractors      *
\****************************************************************************************/

CV_EXPORTS void evaluateFeatureDetector( const Mat& img1, const Mat& img2, const Mat& H1to2,
                                         vector<KeyPoint>* keypoints1, vector<KeyPoint>* keypoints2,
                                         float& repeatability, int& correspCount,
                                         const Ptr<FeatureDetector>& fdetector=Ptr<FeatureDetector>() );

CV_EXPORTS void computeRecallPrecisionCurve( const vector<vector<DMatch> >& matches1to2,
                                             const vector<vector<uchar> >& correctMatches1to2Mask,
                                             vector<Point2f>& recallPrecisionCurve );

CV_EXPORTS float getRecall( const vector<Point2f>& recallPrecisionCurve, float l_precision );
CV_EXPORTS int getNearestPoint( const vector<Point2f>& recallPrecisionCurve, float l_precision );

CV_EXPORTS void evaluateGenericDescriptorMatcher( const Mat& img1, const Mat& img2, const Mat& H1to2,
                                                  vector<KeyPoint>& keypoints1, vector<KeyPoint>& keypoints2,
                                                  vector<vector<DMatch> >* matches1to2, vector<vector<uchar> >* correctMatches1to2Mask,
                                                  vector<Point2f>& recallPrecisionCurve,
                                                  const Ptr<GenericDescriptorMatcher>& dmatch=Ptr<GenericDescriptorMatcher>() );


/****************************************************************************************\
*                                     Bag of visual words                                *
\****************************************************************************************/
/*
 * Abstract base class for training of a 'bag of visual words' vocabulary from a set of descriptors
 */
class CV_EXPORTS BOWTrainer
{
public:
    BOWTrainer();
    virtual ~BOWTrainer();

    void add( const Mat& descriptors );
    const vector<Mat>& getDescriptors() const;
    int descripotorsCount() const;

    virtual void clear();

    /*
     * Train visual words vocabulary, that is cluster training descriptors and
     * compute cluster centers.
     * Returns cluster centers.
     *
     * descriptors      Training descriptors computed on images keypoints.
     */
    virtual Mat cluster() const = 0;
    virtual Mat cluster( const Mat& descriptors ) const = 0;

protected:
    vector<Mat> descriptors;
    int size;
};

/*
 * This is BOWTrainer using cv::kmeans to get vocabulary.
 */
class CV_EXPORTS BOWKMeansTrainer : public BOWTrainer
{
public:
    BOWKMeansTrainer( int clusterCount, const TermCriteria& termcrit=TermCriteria(),
                      int attempts=3, int flags=KMEANS_PP_CENTERS );
    virtual ~BOWKMeansTrainer();

    // Returns trained vocabulary (i.e. cluster centers).
    virtual Mat cluster() const;
    virtual Mat cluster( const Mat& descriptors ) const;

protected:

    int clusterCount;
    TermCriteria termcrit;
    int attempts;
    int flags;
};

/*
 * Class to compute image descriptor using bag of visual words.
 */
class CV_EXPORTS BOWImgDescriptorExtractor
{
public:
    BOWImgDescriptorExtractor( const Ptr<DescriptorExtractor>& dextractor,
                               const Ptr<DescriptorMatcher>& dmatcher );
    virtual ~BOWImgDescriptorExtractor();

    void setVocabulary( const Mat& vocabulary );
    const Mat& getVocabulary() const;
    void compute( const Mat& image, vector<KeyPoint>& keypoints, Mat& imgDescriptor,
                  vector<vector<int> >* pointIdxsOfClusters=0, Mat* descriptors=0 );
    // compute() is not constant because DescriptorMatcher::match is not constant

    int descriptorSize() const;
    int descriptorType() const;

protected:
    Mat vocabulary;
    Ptr<DescriptorExtractor> dextractor;
    Ptr<DescriptorMatcher> dmatcher;
};

} /* namespace cv */

#endif /* __cplusplus */

#endif

/* End of file. */
