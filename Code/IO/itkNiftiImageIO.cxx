/*=========================================================================
Program:   Insight Segmentation & Registration Toolkit
Module:    itkNiftiImageIO.cxx
Language:  C++
Date:      $Date$
Version:   $Revision$

Copyright (c) Insight Software Consortium. All rights reserved.
See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "itkNiftiImageIO.h"
#include "itkIOCommon.h"
#include "itkExceptionObject.h"
#include "itkByteSwapper.h"
#include "itkMetaDataObject.h"
#include "itkSpatialOrientationAdapter.h"
#include <itksys/SystemTools.hxx>
#include <vnl/vnl_math.h>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

namespace itk
{
#if defined(__USE_VERY_VERBOSE_NIFTI_DEBUGGING__)
namespace
{
inline
int print_hex_vals( char const * const data, const int nbytes, FILE * const fp )
{
   int c;

   if ( !data || nbytes < 1 || !fp ) return -1;

   fputs("0x", fp);
   for ( c = 0; c < nbytes; c++ )
      fprintf(fp, " %x", data[c]);

   return 0;
}

/*----------------------------------------------------------------------*/
/*! display the contents of the nifti_1_header (send to stdout)
*//*--------------------------------------------------------------------*/
inline
int DumpNiftiHeader( const std::string &fname )
{
   int c;
   nifti_1_header *hp;
   int swap;
   hp = nifti_read_header(fname.c_str(),&swap,true);
   fputs( "-------------------------------------------------------\n", stderr );
      if ( !hp  ){ fputs(" ** no nifti_1_header to display!\n",stderr); return 1; }

   fprintf(stderr," nifti_1_header :\n"
           "    sizeof_hdr     = %d\n"
           "    data_type[10]  = ", hp->sizeof_hdr);
   print_hex_vals(hp->data_type, 10, stderr);
   fprintf(stderr, "\n"
           "    db_name[18]    = ");
   print_hex_vals(hp->db_name, 18, stderr);
   fprintf(stderr, "\n"
           "    extents        = %d\n"
           "    session_error  = %d\n"
           "    regular        = 0x%x\n"
           "    dim_info       = 0x%x\n",
      hp->extents, hp->session_error, hp->regular, hp->dim_info );
   fprintf(stderr, "    dim[8]         =");
   for ( c = 0; c < 8; c++ ) fprintf(stderr," %d", hp->dim[c]);
   fprintf(stderr, "\n"
           "    intent_p1      = %f\n"
           "    intent_p2      = %f\n"
           "    intent_p3      = %f\n"
           "    intent_code    = %d\n"
           "    datatype       = %d\n"
           "    bitpix         = %d\n"
           "    slice_start    = %d\n"
           "    pixdim[8]      =",
           hp->intent_p1, hp->intent_p2, hp->intent_p3, hp->intent_code,
           hp->datatype, hp->bitpix, hp->slice_start);
   /* break pixdim over 2 lines */
   for ( c = 0; c < 4; c++ ) fprintf(stderr," %f", hp->pixdim[c]);
   fprintf(stderr, "\n                    ");
   for ( c = 4; c < 8; c++ ) fprintf(stderr," %f", hp->pixdim[c]);
   fprintf(stderr, "\n"
           "    vox_offset     = %f\n"
           "    scl_slope      = %f\n"
           "    scl_inter      = %f\n"
           "    slice_end      = %d\n"
           "    slice_code     = %d\n"
           "    xyzt_units     = 0x%x\n"
           "    cal_max        = %f\n"
           "    cal_min        = %f\n"
           "    slice_duration = %f\n"
           "    toffset        = %f\n"
           "    glmax          = %d\n"
           "    glmin          = %d\n",
           hp->vox_offset, hp->scl_slope, hp->scl_inter, hp->slice_end,
           hp->slice_code, hp->xyzt_units, hp->cal_max, hp->cal_min,
           hp->slice_duration, hp->toffset, hp->glmax, hp->glmin);
   fprintf(stderr,
           "    descrip        = '%.80s'\n"
           "    aux_file       = '%.24s'\n"
           "    qform_code     = %d\n"
           "    sform_code     = %d\n"
           "    quatern_b      = %f\n"
           "    quatern_c      = %f\n"
           "    quatern_d      = %f\n"
           "    qoffset_x      = %f\n"
           "    qoffset_y      = %f\n"
           "    qoffset_z      = %f\n"
           "    srow_x[4]      = %f, %f, %f, %f\n"
           "    srow_y[4]      = %f, %f, %f, %f\n"
           "    srow_z[4]      = %f, %f, %f, %f\n"
           "    intent_name    = '%-.16s'\n"
           "    magic          = '%-.4s'\n",
           hp->descrip, hp->aux_file, hp->qform_code, hp->sform_code,
           hp->quatern_b, hp->quatern_c, hp->quatern_d,
           hp->qoffset_x, hp->qoffset_y, hp->qoffset_z,
           hp->srow_x[0], hp->srow_x[1], hp->srow_x[2], hp->srow_x[3],
           hp->srow_y[0], hp->srow_y[1], hp->srow_y[2], hp->srow_y[3],
           hp->srow_z[0], hp->srow_z[1], hp->srow_z[2], hp->srow_z[3],
           hp->intent_name, hp->magic);
   fputs( "-------------------------------------------------------\n", stderr );
   fflush(stderr);

   return 0;
}
}
#endif // #if defined(__USE_VERY_VERBOSE_NIFTI_DEBUGGING__)

NiftiImageIO::NiftiImageIO():
  m_NiftiImage(0)
{
  this->SetNumberOfDimensions(3);
  m_RescaleSlope = 1.0;
  m_RescaleIntercept = 0.0;
  nifti_set_debug_level(0); // suppress error messages
}

NiftiImageIO::~NiftiImageIO()
{
  nifti_image_free(this->m_NiftiImage);
}

void NiftiImageIO::PrintSelf(std::ostream& os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
}

bool NiftiImageIO::CanWriteFile(const char * FileNameToWrite)
{
  std::string fname(FileNameToWrite);
  std::string::size_type ext = fname.rfind('.');
  //
  // for now, defer to analyze to write .hdr/.img pairs
  if(ext != std::string::npos)
    {
    std::string exts = fname.substr(ext);
    if(exts == ".gz")
      {
      std::string::size_type dotpos = fname.rfind('.',ext-1);
      if(dotpos != std::string::npos)
        {
        exts = fname.substr(dotpos);
        }
      }
    if(exts == ".hdr" || exts == ".img" || exts == ".img.gz")
      {
      return false;
      }
    }

  return (nifti_is_complete_filename(FileNameToWrite) == 1 ) ? true: false;
}


// Internal function to rescale pixel according to Rescale Slope/Intercept
template<class TBuffer>
void RescaleFunction(TBuffer* buffer, double slope, double intercept, size_t size)
{
  for(unsigned int i=0; i<size; i++)
   {
   double tmp = static_cast<double>(buffer[i]) * slope;
   tmp += intercept;
   buffer[i] = static_cast<TBuffer>(tmp);
   }
}

void NiftiImageIO::Read(void* buffer)
{
  this->m_NiftiImage=nifti_image_read(m_FileName.c_str(),true);
  if (this->m_NiftiImage == NULL)
    {
    ExceptionObject exception(__FILE__, __LINE__);
    exception.SetDescription("Read failed");
    throw exception;

    }
  const int dims=this->GetNumberOfDimensions();
  size_t numElts = 1;

  switch (dims)
    {
    case 7:
      numElts *= this->m_NiftiImage->nw;
    case 6:
      numElts *= this->m_NiftiImage->nv;
    case 5:
      numElts *= this->m_NiftiImage->nu;
    case 4:
      numElts *= this->m_NiftiImage->nt;
    case 3:
      numElts *= this->m_NiftiImage->nz;
    case 2:
      numElts *= this->m_NiftiImage->ny;
    case 1:
      numElts *= this->m_NiftiImage->nx;
      break;
    default:
      numElts = 0;
    }
  const size_t NumBytes=numElts * this->m_NiftiImage->nbyper;
  memcpy(buffer, this->m_NiftiImage->data, NumBytes);

  if(m_RescaleSlope > 1 ||
     m_RescaleIntercept != 0)
    {
    switch(m_ComponentType)
      {
      case CHAR:
        RescaleFunction(static_cast<char *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case UCHAR:
        RescaleFunction(static_cast<unsigned char *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case SHORT:
        RescaleFunction(static_cast<short *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case USHORT:
        RescaleFunction(static_cast<unsigned short *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case INT:
        RescaleFunction(static_cast<int *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case UINT:
        RescaleFunction(static_cast<unsigned int *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case LONG:
        RescaleFunction(static_cast<long *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case ULONG:
        RescaleFunction(static_cast<unsigned long *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case FLOAT:
        RescaleFunction(static_cast<float *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      case DOUBLE:
        RescaleFunction(static_cast<double *>(buffer),
                        m_RescaleSlope,
                        m_RescaleIntercept,numElts);
        break;
      default:
        if(this->GetPixelType() == SCALAR)
          {
          ExceptionObject exception(__FILE__, __LINE__);
          exception.SetDescription("Datatype not supported");
          throw exception;
          }
      }
    }
}


// This method will only test if the header looks like an
// Nifti Header.  Some code is redundant with ReadImageInformation
// a StateMachine could provide a better implementation
bool NiftiImageIO::CanReadFile( const char* FileNameToRead )
{
  return is_nifti_file(FileNameToRead) > 0;
}


namespace
{
inline double determinant(const std::vector<double> &dirx,
                          const std::vector<double> &diry,
                          const std::vector<double> &dirz)
{
  return
    dirx[0]*diry[1]*dirz[2]-
    dirx[0]*dirz[1]*diry[2]-
    diry[0]*dirx[1]*dirz[2]+
    diry[0]*dirz[1]*dirx[2]+
    dirz[0]*dirx[1]*diry[2]-
    dirz[0]*diry[1]*dirx[2];
}
}

//
// shorthand for SpatialOrientation types
typedef itk::SpatialOrientation::CoordinateTerms 
SO_CoordTermsType;
typedef itk::SpatialOrientation::ValidCoordinateOrientationFlags 
SO_OrientationType;

/** Convert from NIFTI orientation codes to ITK orientation codes.
 *  As it happens, this implicitly negates the x and y directiosn, as is
 *  required to go from NIFTI to DICOM style orientations, in that the labeling
 *  is consistent, but the NIFTI rotation matrix and the ITK Direction cosines
 *  for the x and y dimensions have the opposite sign/direction.
 */
inline SO_OrientationType
Nifti2SO_Coord(int i, int j, int k)
{
  SO_CoordTermsType
    NiftiOrient2SO_CoordinateTerms[] =
    {
      itk::SpatialOrientation::ITK_COORDINATE_UNKNOWN,
      itk::SpatialOrientation::ITK_COORDINATE_Left,
      itk::SpatialOrientation::ITK_COORDINATE_Right,
      itk::SpatialOrientation::ITK_COORDINATE_Posterior,
      itk::SpatialOrientation::ITK_COORDINATE_Anterior,
      itk::SpatialOrientation::ITK_COORDINATE_Inferior,
      itk::SpatialOrientation::ITK_COORDINATE_Superior,
    };
  return static_cast<SO_OrientationType>
    ((NiftiOrient2SO_CoordinateTerms[i] << itk::SpatialOrientation::ITK_COORDINATE_PrimaryMinor) |
    (NiftiOrient2SO_CoordinateTerms[j] << itk::SpatialOrientation::ITK_COORDINATE_SecondaryMinor) |
     (NiftiOrient2SO_CoordinateTerms[k] << itk::SpatialOrientation::ITK_COORDINATE_TertiaryMinor));

}
                              
    
void NiftiImageIO::ReadImageInformation()
{
  this->m_NiftiImage=nifti_image_read(m_FileName.c_str(),false);
  static std::string prev;
  if(prev != m_FileName)
    {
#if defined(__USE_VERY_VERBOSE_NIFTI_DEBUGGING__)
    DumpNiftiHeader(m_FileName);
#endif
    prev = m_FileName;
    }
  if(this->m_NiftiImage == 0)
    {
    ExceptionObject exception(__FILE__, __LINE__);
    std::string ErrorMessage(m_FileName);
    ErrorMessage += " is not recognized as a NIFTI file";
    exception.SetDescription(ErrorMessage.c_str());
    throw exception;

    }
  this->SetNumberOfDimensions(this->m_NiftiImage->ndim);
  switch( this->m_NiftiImage->datatype )
    {
    case NIFTI_TYPE_INT8:
      m_ComponentType = CHAR;
      m_PixelType = SCALAR;
      break;
    case NIFTI_TYPE_UINT8:
      m_ComponentType = UCHAR;
      m_PixelType = SCALAR;
      break;
    case NIFTI_TYPE_INT16:
      m_ComponentType = SHORT;
      m_PixelType = SCALAR;
      break;
    case NIFTI_TYPE_UINT16:
      m_ComponentType = USHORT;
      m_PixelType = SCALAR;
      break;
    case NIFTI_TYPE_INT32:
      m_ComponentType = INT;
      m_PixelType = SCALAR;
      break;
    case NIFTI_TYPE_UINT32:
      m_ComponentType = UINT;
      m_PixelType = SCALAR;
      break;
    case NIFTI_TYPE_FLOAT32:
      m_ComponentType = FLOAT;
      m_PixelType = SCALAR;
      break;
    case NIFTI_TYPE_FLOAT64:
      m_ComponentType = DOUBLE;
      m_PixelType = SCALAR;
      break;
    case NIFTI_TYPE_RGB24:
      m_ComponentType = UCHAR;
      m_PixelType = RGB;
      this->SetNumberOfComponents(3);
      //    case DT_RGB:
      // DEBUG -- Assuming this is a triple, not quad
      //image.setDataType( uiig::DATA_RGBQUAD );
      //      break;
    default:
      break;
    }
  //
  // set up the dimension stuff
  double spacingscale=1.0;//default to mm
  switch(XYZT_TO_SPACE(this->m_NiftiImage->xyz_units))
      {
  case NIFTI_UNITS_METER:
      spacingscale=1e3;
      break;
  case NIFTI_UNITS_MM:
      spacingscale=1e0;
      break;
  case NIFTI_UNITS_MICRON:
      spacingscale=1e-3;;
      break;
      }
  double timingscale=1.0;//Default to seconds
  switch(XYZT_TO_TIME(this->m_NiftiImage->xyz_units))
      {
  case NIFTI_UNITS_SEC:
      timingscale=1.0;
      break;
  case NIFTI_UNITS_MSEC:
      timingscale=1e-3;
      break;
  case NIFTI_UNITS_USEC:
      timingscale=1e-6;;
      break;
      }
  const int dims=this->GetNumberOfDimensions();
  switch(dims)
      {
  case 7:
      this->SetDimensions(6,this->m_NiftiImage->nw);
      this->SetSpacing(6,this->m_NiftiImage->dw);//NOTE: Scaling is not defined in this dimension
  case 6:
      this->SetDimensions(5,this->m_NiftiImage->nv);
      this->SetSpacing(5,this->m_NiftiImage->dv);//NOTE: Scaling is not defined in this dimension
  case 5:
      this->SetDimensions(4,this->m_NiftiImage->nu);
      this->SetSpacing(4,this->m_NiftiImage->du);//NOTE: Scaling is not defined in this dimension
  case 4:
      this->SetDimensions(3,this->m_NiftiImage->nt);
      this->SetSpacing(3,this->m_NiftiImage->dt*timingscale);
  case 3:
      this->SetDimensions(2,this->m_NiftiImage->nz);
      this->SetSpacing(2,this->m_NiftiImage->dz*spacingscale);
  case 2:
      this->SetDimensions(1,this->m_NiftiImage->ny);
      this->SetSpacing(1,this->m_NiftiImage->dy*spacingscale);
  case 1:
      this->SetDimensions(0,this->m_NiftiImage->nx);
      this->SetSpacing(0,this->m_NiftiImage->dx*spacingscale);
      }
  this->ComputeStrides();
  //Get Dictionary Information
  //Insert Orientation.
  //Need to encapsulate as much Nifti information as possible here.
  MetaDataDictionary &thisDic=this->GetMetaDataDictionary();
  std::string classname(this->GetNameOfClass());
  EncapsulateMetaData<std::string>(thisDic,ITK_InputFilterName, classname);

  switch( this->m_NiftiImage->datatype)
    {
    case NIFTI_TYPE_INT8:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string(typeid(char).name()));
      break;
    case NIFTI_TYPE_UINT8:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string(typeid(unsigned char).name()));
      break;
    case NIFTI_TYPE_INT16:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string(typeid(short).name()));
      break;
    case NIFTI_TYPE_UINT16:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string(typeid(unsigned short).name()));
      break;
    case NIFTI_TYPE_INT32:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string(typeid(long).name()));
      break;
    case NIFTI_TYPE_UINT32:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string(typeid(unsigned long).name()));
      break;
    case NIFTI_TYPE_FLOAT32:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string(typeid(float).name()));
      break;
    case NIFTI_TYPE_FLOAT64:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string(typeid(double).name()));
      break;
    case NIFTI_TYPE_RGB24:
      EncapsulateMetaData<std::string>(thisDic,ITK_OnDiskStorageTypeName,std::string("RGB"));
      break;
      //    case DT_RGB:
      // DEBUG -- Assuming this is a triple, not quad
      //image.setDataType( uiig::DATA_RGBQUAD );
      //      break;
    default:
      break;
    }
  typedef SpatialOrientationAdapter<3> OrientAdapterType;

  SpatialOrientationAdapter<3>::DirectionType dir;

  //
  // in the case of an Analyze75 file, use old analyze orient method.
  if(this->m_NiftiImage->qform_code == 0 && this->m_NiftiImage->sform_code == 0)
    {
    SpatialOrientationAdapter<3>::OrientationType orient;
    switch(this->m_NiftiImage->analyze75_orient)
      {
      case a75_transverse_unflipped:
        orient = SpatialOrientation::ITK_COORDINATE_ORIENTATION_RPI;
        break;
      case a75_sagittal_unflipped:
        orient = SpatialOrientation::ITK_COORDINATE_ORIENTATION_PIR;
        break;
      // according to analyze documents, you don't see flipped orientation in the wild
      case a75_transverse_flipped:
      case a75_coronal_flipped:
      case a75_sagittal_flipped:
      case a75_orient_unknown:
      case a75_coronal_unflipped:
        orient = SpatialOrientation::ITK_COORDINATE_ORIENTATION_RIP;
        break;
      }
    dir =  OrientAdapterType().ToDirectionCosines(orient);
    m_RescaleSlope = 1;
    m_RescaleIntercept = 0;
    m_Origin[0] = m_Origin[1] = 0;
    if(dims > 2)
      {
      m_Origin[2] = 0;
      }
    }
  else // qform or sform
    {
    //
    mat44 theMat;
    if(this->m_NiftiImage->qform_code > 0)
      {
      theMat = this->m_NiftiImage->qto_xyz;
      }
    else if(this->m_NiftiImage->sform_code > 0)
      {
      theMat = this->m_NiftiImage->sto_xyz;
      }
    int ii, jj, kk;
    nifti_mat44_to_orientation(theMat,&ii,&jj,&kk);
    SO_OrientationType orient = Nifti2SO_Coord(ii,jj,kk);
    dir =  OrientAdapterType().ToDirectionCosines(orient);
    // scale image data based on slope/intercept
    //
    if((m_RescaleSlope = this->m_NiftiImage->scl_slope) == 0)
      {
      m_RescaleSlope = 1;
      }
    m_RescaleIntercept = this->m_NiftiImage->scl_inter;
    //
    // set origin
    m_Origin[0] = -theMat.m[0][3];
    m_Origin[1] = -theMat.m[1][3];
    if(dims > 2)
      {
      m_Origin[2] = theMat.m[2][3];
      }
    }
  
  std::vector<double> dirx(3,0),
    diry(3,0),
    dirz(3,0);
  dirx[0] = dir[0][0]; dirx[1] = dir[1][0]; dirx[2] = dir[2][0];
  diry[0] = dir[0][1]; diry[1] = dir[1][1]; diry[2] = dir[2][1];
  dirz[0] = dir[0][2]; dirz[1] = dir[1][2]; dirz[2] = dir[2][2];
//   std::cerr << "read: dirx " << dirx[0] << " " << dirx[1] << " " << dirx[2] << std::endl;
//   std::cerr << "read: diry " << diry[0] << " " << diry[1] << " " << diry[2] << std::endl;
//   std::cerr << "read: dirz " << dirz[0] << " " << dirz[1] << " " << dirz[2] << std::endl;
  this->SetDirection(0,dirx);
  this->SetDirection(1,diry);
  if(dims > 2)
    {
    this->SetDirection(2,dirz);
    }
                                             
  

  //Important hist fields
  std::string description(this->m_NiftiImage->descrip);
  EncapsulateMetaData<std::string>(this->GetMetaDataDictionary(),
                                    ITK_FileNotes,description);

  // We don't need the image anymore
  nifti_image_free(this->m_NiftiImage);
  this->m_NiftiImage = 0;
}

namespace {
inline mat44 mat44_transpose(mat44 in)
{
  mat44 out;
  for(unsigned i = 0; i < 4; i++)
    {
    for(unsigned j = 0; j < 4; j++)
      {
      out.m[i][j] = in.m[j][i];
      }
    }
  return out;
}
}
/**
   *
   */
void
NiftiImageIO
::WriteImageInformation(void) //For Nifti this does not write a file, it only fills in the appropriate header information.
{
#if 0
  if(this->GetNumberOfComponents() > 1)
    {
    ExceptionObject exception(__FILE__, __LINE__);
    std::string ErrorMessage=
      "More than one component per pixel not supported";
    exception.SetDescription(ErrorMessage.c_str());
    throw exception;
    }
#endif
  //  MetaDataDictionary &thisDic=this->GetMetaDataDictionary();
  //
  // fill out the image header.
  if(this->m_NiftiImage == 0)
    {
    this->m_NiftiImage = nifti_simple_init_nim();
    }
  //
  // set the filename
  std::string FName(this->GetFileName());
  this->m_NiftiImage->fname = (char *)malloc(FName.size()+1);
  strcpy(this->m_NiftiImage->fname,FName.c_str());
  //
  // set the file type
  std::string::size_type ext = FName.rfind('.');
  if(ext == std::string::npos)
    {
      ExceptionObject exception(__FILE__, __LINE__);
      std::string ErrorMessage("Bad Nifti file name ");
      ErrorMessage += FName;
      exception.SetDescription(ErrorMessage.c_str());
      throw exception;

    }
  std::string Ext = FName.substr(ext);
  //
  // look for compressed Nifti
  if(Ext == ".gz")
    {
    ext = FName.rfind(".nii.gz");
    if(ext != std::string::npos)
      {
      Ext = ".nii.gz";
      }
    }
  if(Ext == ".nii" || Ext == ".nii.gz")
    {
    this->m_NiftiImage->nifti_type = 1;
    this->m_NiftiImage->iname = (char *)malloc(FName.size()+1);
    strcpy(this->m_NiftiImage->fname,FName.c_str());
    strcpy(this->m_NiftiImage->iname,FName.c_str());
    }
  else if(Ext == ".hdr" || Ext == ".img")
    {
    this->m_NiftiImage->nifti_type = 2;
    if(Ext == ".hdr")
      {
      strcpy(this->m_NiftiImage->fname,FName.c_str());
      }
    else
      {
      FName.erase(ext);
      FName += ".hdr";
      }
    strcpy(this->m_NiftiImage->fname,FName.c_str());
    FName.erase(FName.rfind('.'));
    FName += ".img";
    this->m_NiftiImage->iname = (char *)malloc(FName.size()+1);
    strcpy(this->m_NiftiImage->iname,FName.c_str());
    }
  else
    {
    ExceptionObject exception(__FILE__, __LINE__);
    std::string ErrorMessage("Bad Nifti file name ");
    ErrorMessage += FName;
    exception.SetDescription(ErrorMessage.c_str());
    throw exception;
    }
  unsigned short dims =
    this->m_NiftiImage->ndim =
    this->m_NiftiImage->dim[0] =
    this->GetNumberOfDimensions();
//     FIELD         NOTES
//     -----------------------------------------------------
//     sizeof_hdr    must be 348
//     -----------------------------------------------------
//     dim           dim[0] and dim[1] are always required;
//                   dim[2] is required for 2-D volumes,
//                   dim[3] for 3-D volumes, etc.
  this->m_NiftiImage->nvox = 1;
  //Spacial dims in ITK are given in mm.
  //If 4D assume 4thD is in SECONDS, for all of ITK.
  //NOTE: Due to an ambiguity in the nifti specification, some developers external tools
  //      believe that the time units must be set, even if there is only one dataset.
  //      Having the time specified for a purly spatial image has no consequence, so go ahead and set it
  //      to seconds.
  this->m_NiftiImage->xyz_units=NIFTI_UNITS_MM | NIFTI_UNITS_SEC;
  switch(dims)
      {
  case 7:
      this->m_NiftiImage->nvox *=
          this->m_NiftiImage->dim[7] =
          this->m_NiftiImage->nw = this->GetDimensions(6);
      this->m_NiftiImage->pixdim[7] = 
        this->m_NiftiImage->dw = this->GetSpacing(6);
  case 6:
      this->m_NiftiImage->nvox *=
          this->m_NiftiImage->dim[6] =
          this->m_NiftiImage->nv = this->GetDimensions(5);
      this->m_NiftiImage->pixdim[6] =
          this->m_NiftiImage->dv = this->GetSpacing(5);
  case 5:
      this->m_NiftiImage->nvox *=
          this->m_NiftiImage->dim[5] =
          this->m_NiftiImage->nu = this->GetDimensions(4);
      this->m_NiftiImage->pixdim[5] =
          this->m_NiftiImage->du = this->GetSpacing(4);
  case 4:
      this->m_NiftiImage->nvox *=
          this->m_NiftiImage->dim[4] =
          this->m_NiftiImage->nt = this->GetDimensions(3);
      this->m_NiftiImage->pixdim[4] =
          this->m_NiftiImage->dt = this->GetSpacing(3);
  case 3:
      this->m_NiftiImage->nvox *=
          this->m_NiftiImage->dim[3] =
          this->m_NiftiImage->nz = this->GetDimensions(2);
      this->m_NiftiImage->pixdim[3] =
          this->m_NiftiImage->dz = this->GetSpacing(2);
  case 2:
      this->m_NiftiImage->nvox *=
          this->m_NiftiImage->dim[2] =
          this->m_NiftiImage->ny = this->GetDimensions(1);
      this->m_NiftiImage->pixdim[2] =
          this->m_NiftiImage->dy = this->GetSpacing(1);
  case 1:
      this->m_NiftiImage->nvox *=
          this->m_NiftiImage->dim[1] =
          this->m_NiftiImage->nx = this->GetDimensions(0);
      this->m_NiftiImage->pixdim[1] =
          this->m_NiftiImage->dx = this->GetSpacing(0);
      }

//     -----------------------------------------------------
//     datatype      needed to specify type of image data
//     -----------------------------------------------------
//     bitpix        should correspond correctly to datatype
//     -----------------------------------------------------
  switch(this->GetComponentType())
    {
    case UCHAR:
      this->m_NiftiImage->datatype = NIFTI_TYPE_UINT8;
      this->m_NiftiImage->nbyper = 1;
      break;
    case CHAR:
      this->m_NiftiImage->datatype = NIFTI_TYPE_INT8;
      this->m_NiftiImage->nbyper = 1;
      break;
    case USHORT:
      this->m_NiftiImage->datatype = NIFTI_TYPE_UINT16;
      this->m_NiftiImage->nbyper = 2;
      break;
    case SHORT:
      this->m_NiftiImage->datatype = NIFTI_TYPE_INT16;
      this->m_NiftiImage->nbyper = 2;
      break;
    case ULONG:
    case UINT:
      this->m_NiftiImage->datatype = NIFTI_TYPE_UINT32;
      this->m_NiftiImage->nbyper = 4;
      break;
    case LONG:
    case INT:
      this->m_NiftiImage->datatype = NIFTI_TYPE_INT32;
      this->m_NiftiImage->nbyper = 4;
      break;
    case FLOAT:
      this->m_NiftiImage->datatype = NIFTI_TYPE_FLOAT32;
      this->m_NiftiImage->nbyper = 4;
      break;
    case DOUBLE:
      this->m_NiftiImage->datatype = NIFTI_TYPE_FLOAT64;
      this->m_NiftiImage->nbyper = 8;
      break;
    case UNKNOWNCOMPONENTTYPE:
    default:
      {
      ExceptionObject exception(__FILE__, __LINE__);
      std::string ErrorMessage=
        "More than one component per pixel not supported";
      exception.SetDescription(ErrorMessage.c_str());
      throw exception;
      }
    }
  switch(this->GetPixelType())
    {
    case SCALAR:
      break;
    case RGB:
      this->m_NiftiImage->nbyper *= 3;
      this->m_NiftiImage->datatype = NIFTI_TYPE_RGB24;
      break;
    default:
      {
      ExceptionObject exception(__FILE__, __LINE__);
      std::string ErrorMessage =
        "Unsupported Pixel Type";
      exception.SetDescription(ErrorMessage.c_str());
      throw exception;
      }
    }
//     -----------------------------------------------------
//     vox_offset    required for an "n+1" header
//     -----------------------------------------------------
//     magic         must be "ni1\0" or "n+1\0"
//     -----------------------------------------------------
  this->m_NiftiImage->scl_slope = 1.0;
  this->m_NiftiImage->scl_inter = 0.0;

  //
  // use NIFTI method 2
  this->m_NiftiImage->sform_code = NIFTI_XFORM_SCANNER_ANAT;
  this->m_NiftiImage->qform_code = NIFTI_XFORM_ALIGNED_ANAT;

  //
  // set the quarternions, from the direction vectors
  std::vector<double> dirx = this->GetDirection(0);
  //  negateifXorY(dirx);
  std::vector<double> diry  = this->GetDirection(1);
//   std::cerr << "write: dirx " << dirx[0] << " " << dirx[1] << " " << dirx[2] << std::endl;
//   std::cerr << "write: diry " << diry[0] << " " << diry[1] << " " << diry[2] << std::endl;
//   dirx[0] = - dirx[0];
//   dirx[1] = - dirx[1];
//   dirx[2] = - dirx[2];
//   diry[0] = - diry[0];
//   diry[1] = - diry[1];
//   diry[2] = - diry[2];
    
  std::vector<double> dirz;
  if(dims > 2)
    {
    dirz = this->GetDirection(2);
    //    negateifXorY(dirz);
    }
  else
    {
    //    dirz[0] = dirz[1] = dirz[2] = 0;
    dirz.push_back(0);  dirz.push_back(0); dirz.push_back(0);
    }
  //  std::cerr << "write: dirz " << dirz[0] << " " << dirz[1] << " " << dirz[2] << std::endl;
  for(unsigned i=0; i < 2; i++)
    {
    dirx[i] = -dirx[i];
    diry[i] = -diry[i];
    dirz[i] = -dirz[i];
    }
  mat44 matrix = 
    nifti_make_orthog_mat44(dirx[0],dirx[1],dirx[2],
                            diry[0],diry[1],diry[2],
                            dirz[0],dirz[1],dirz[2]);
  matrix = mat44_transpose(matrix);
  // Fill in origin.
  for(unsigned i = 0; i < 2; i++)
    {
    matrix.m[i][3] = -this->GetOrigin(i);
    }
  matrix.m[2][3] = (dims > 2) ? this->GetOrigin(2) : 0.0;

  nifti_mat44_to_quatern(matrix,
                         &(this->m_NiftiImage->quatern_b),
                         &(this->m_NiftiImage->quatern_c),
                         &(this->m_NiftiImage->quatern_d),
                         &(this->m_NiftiImage->qoffset_x),
                         &(this->m_NiftiImage->qoffset_y),
                         &(this->m_NiftiImage->qoffset_z),
                         0,
                         0,
                         0,
                         &(this->m_NiftiImage->qfac));
  // copy q matrix to s matrix
  this->m_NiftiImage->qto_xyz =  matrix;
  this->m_NiftiImage->sto_xyz =  matrix;
  for(unsigned i = 0; i < 3; i++)
    {
    for(unsigned j = 0; j < 3; j++)
      {
      this->m_NiftiImage->sto_xyz.m[i][j] = this->GetSpacing(j) *
        this->m_NiftiImage->sto_xyz.m[i][j];
      this->m_NiftiImage->sto_ijk.m[i][j] = 
        this->m_NiftiImage->sto_xyz.m[i][j] / this->GetSpacing(j);
      }
    }
  this->m_NiftiImage->sto_ijk =  
    nifti_mat44_inverse(this->m_NiftiImage->sto_xyz);
  this->m_NiftiImage->qto_ijk =  
    nifti_mat44_inverse(this->m_NiftiImage->qto_xyz);
  
  this->m_NiftiImage->pixdim[0] = this->m_NiftiImage->qfac;
  this->m_NiftiImage->qform_code = NIFTI_XFORM_SCANNER_ANAT;
  //  this->m_NiftiImage->sform_code = 0;
  return;
}


/**
   *
   */
void
NiftiImageIO

::Write( const void* buffer)
{
  this->WriteImageInformation(); //Write the image Information before writing data
  this->m_NiftiImage->data=const_cast<void *>(buffer);//Need a const cast here so that we don't have to copy the memory for writing.
  nifti_image_write(this->m_NiftiImage);
  this->m_NiftiImage->data = 0; // if left pointing to data buffer
                                // nifti_image_free will try and free this memory
}

} // end namespace itk
