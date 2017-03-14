/* If you happen not to want the image transform support, disable it here */
#ifndef TRANSFORMS_SUPPORTED
#define TRANSFORMS_SUPPORTED 1		/* 0 disables transform code */
#endif



/* Short forms of external names for systems with brain-damaged linkers. */

#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jtransform_parse_crop_spec	jTrParCrop
#define jtransform_request_workspace	jTrRequest
#define jtransform_adjust_parameters	jTrAdjust
#define jtransform_execute_transform	jTrExec
#define jtransform_perfect_transform	jTrPerfect
#define jcopy_markers_setup		jCMrkSetup
#define jcopy_markers_execute		jCMrkExec
#endif /* NEED_SHORT_EXTERNAL_NAMES */


/*
 * Codes for supported types of image transformations.
 */

typedef enum {
	JXFORM_NONE,		/* no transformation */
	JXFORM_FLIP_H,		/* horizontal flip */
	JXFORM_FLIP_V,		/* vertical flip */
	JXFORM_TRANSPOSE,	/* transpose across UL-to-LR axis */
	JXFORM_TRANSVERSE,	/* transpose across UR-to-LL axis */
	JXFORM_ROT_90,		/* 90-degree clockwise rotation */
	JXFORM_ROT_180,		/* 180-degree rotation */
	JXFORM_ROT_270		/* 270-degree clockwise (or 90 ccw) */
} JXFORM_CODE;

/*
 * Codes for crop parameters, which can individually be unspecified,
 * positive, or negative.  (Negative width or height makes no sense, though.)
 */

typedef enum {
	JCROP_UNSET,
	JCROP_POS,
	JCROP_NEG
} JCROP_CODE;

/*
 * Transform parameters struct.
 * NB: application must not change any elements of this struct after
 * calling jtransform_request_workspace.
 */

typedef struct {
  /* Options: set by caller */
  JXFORM_CODE transform;	/* image transform operator */
  boolean perfect;		/* if TRUE, fail if partial MCUs are requested */
  boolean trim;			/* if TRUE, trim partial MCUs as needed */
  boolean force_grayscale;	/* if TRUE, convert color image to grayscale */
  boolean crop;			/* if TRUE, crop source image */

  /* Crop parameters: application need not set these unless crop is TRUE.
   * These can be filled in by jtransform_parse_crop_spec().
   */
  JDIMENSION crop_width;	/* Width of selected region */
  JCROP_CODE crop_width_set;
  JDIMENSION crop_height;	/* Height of selected region */
  JCROP_CODE crop_height_set;
  JDIMENSION crop_xoffset;	/* X offset of selected region */
  JCROP_CODE crop_xoffset_set;	/* (negative measures from right edge) */
  JDIMENSION crop_yoffset;	/* Y offset of selected region */
  JCROP_CODE crop_yoffset_set;	/* (negative measures from bottom edge) */

  /* Internal workspace: caller should not touch these */
  int num_components;		/* # of components in workspace */
  jvirt_barray_ptr * workspace_coef_arrays; /* workspace for transformations */
  JDIMENSION output_width;	/* cropped destination dimensions */
  JDIMENSION output_height;
  JDIMENSION x_crop_offset;	/* destination crop offsets measured in iMCUs */
  JDIMENSION y_crop_offset;
  int max_h_samp_factor;	/* destination iMCU size */
  int max_v_samp_factor;
} jpeg_transform_info;


#if TRANSFORMS_SUPPORTED

/* Parse a crop specification (written in X11 geometry style) */
EXTERN(boolean) jtransform_parse_crop_spec
	JPP((jpeg_transform_info *info, const char *spec));
/* Request any required workspace */
EXTERN(void) jtransform_request_workspace
	JPP((j_decompress_ptr srcinfo, jpeg_transform_info *info));
/* Adjust output image parameters */
EXTERN(jvirt_barray_ptr *) jtransform_adjust_parameters
	JPP((j_decompress_ptr srcinfo, j_compress_ptr dstinfo,
	     jvirt_barray_ptr *src_coef_arrays,
	     jpeg_transform_info *info));
/* Execute the actual transformation, if any */
EXTERN(void) jtransform_execute_transform
	JPP((j_decompress_ptr srcinfo, j_compress_ptr dstinfo,
	     jvirt_barray_ptr *src_coef_arrays,
	     jpeg_transform_info *info));
/* Determine whether lossless transformation is perfectly
 * possible for a specified image and transformation.
 */
EXTERN(boolean) jtransform_perfect_transform
	JPP((JDIMENSION image_width, JDIMENSION image_height,
	     int MCU_width, int MCU_height,
	     JXFORM_CODE transform));

/* jtransform_execute_transform used to be called
 * jtransform_execute_transformation, but some compilers complain about
 * routine names that long.  This macro is here to avoid breaking any
 * old source code that uses the original name...
 */
#define jtransform_execute_transformation	jtransform_execute_transform

#endif /* TRANSFORMS_SUPPORTED */


/*
 * Support for copying optional markers from source to destination file.
 */

typedef enum {
	JCOPYOPT_NONE,		/* copy no optional markers */
	JCOPYOPT_COMMENTS,	/* copy only comment (COM) markers */
	JCOPYOPT_ALL		/* copy all optional markers */
} JCOPY_OPTION;

#define JCOPYOPT_DEFAULT  JCOPYOPT_COMMENTS	/* recommended default */

/* Setup decompression object to save desired markers in memory */
EXTERN(void) jcopy_markers_setup
	JPP((j_decompress_ptr srcinfo, JCOPY_OPTION option));
/* Copy markers saved in the given source object to the destination object */
EXTERN(void) jcopy_markers_execute
	JPP((j_decompress_ptr srcinfo, j_compress_ptr dstinfo,
	     JCOPY_OPTION option));
