#include <exception>
#include <algorithm>
#include <vector>
#include "RcppANTsR.h"
#include <ants.h>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sstream>
#include <unistd.h>
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkRegionOfInterestImageFilter.h"
#include "vnl/vnl_matrix.h"
#include "itkNeighborhoodIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkCSVNumericObjectFileWriter.h"
#include <vnl/algo/vnl_svd.h>
#include <itkStatisticsImageFilter.h>
#include "itkImageRegionIterator.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkGradientImageFilter.h"
#include "itkCovariantVector.h"
#include "itkGradientRecursiveGaussianImageFilter.h"
#include "itkBSplineInterpolateImageFunction.h"
#include "itkRIPMMARCImageFilter.h"

template< class ImageType >
SEXP patchAnalysisHelper(
    SEXP r_inimg,
    SEXP r_inmaskimg,
    SEXP r_outimg,
    SEXP r_patchRadius,
    SEXP r_patchSamples,
    SEXP r_patchVar,
    SEXP r_meanCenter,
    SEXP r_verbose )
{
  typedef typename ImageType::Pointer ImagePointerType;
  typename ImageType::Pointer inimg =
    Rcpp::as< ImagePointerType >( r_inimg );
  typename ImageType::Pointer inmaskimg =
    Rcpp::as< ImagePointerType >( r_inmaskimg );
  typename ImageType::Pointer outimg =
    Rcpp::as< ImagePointerType >( r_outimg );
  float patchRadius = Rcpp::as< float >( r_patchRadius );
  float patchVar = Rcpp::as< float >( r_patchVar );
  unsigned int patchSamples = Rcpp::as< unsigned int >( r_patchSamples );
  bool meanCenter = Rcpp::as< bool >( r_meanCenter );
  unsigned int verbose = Rcpp::as< unsigned int >( r_verbose );

  typedef itk::RIPMMARCImageFilter< ImageType > filterType;
  typename filterType::Pointer filter = filterType::New();
  filter->SetInput( inimg );
  filter->SetMaskImage( inmaskimg );
  filter->SetLearnPatchBasis(   true );
  filter->SetRotationInvariant( true );
  filter->SetMeanCenterPatches( meanCenter );
  filter->SetPatchRadius( patchRadius );
  filter->SetNumberOfSamplePatches( patchSamples );
  filter->SetTargetVarianceExplained( patchVar );
  filter->SetVerbose( verbose );
  if ( verbose > 0 ) std::cout << filter << std::endl;
  filter->Update( );
//  outimg = filter->GetOutput(); // what should the output be?
//  r_outimg = Rcpp::wrap( outimg );
  outimg = filter->GetCanonicalFrame(); // what should the output be?
  r_outimg = Rcpp::wrap( outimg );

  // solutions should be much smaller so may not be a big deal to copy
  // FIXME - should not copy, should map memory
  typename filterType::vnlMatrixType solV =
    filter->GetSignificantPatchEigenvectors();
  Rcpp::NumericMatrix ripMat( solV.cols(), solV.rows() );
  unsigned long rows = solV.rows();
  for( unsigned long c = 0; c < solV.cols(); c++ )
    {
    for( unsigned int r = 0; r < rows; r++ )
      {
      ripMat( c, r ) = solV( r, c );
      }
    }
  // get the full image patch matrix
  solV = filter->GetPatchesForAllPointsWithinMask();
  Rcpp::NumericMatrix iripMat( solV.cols(), solV.rows() );
  rows = solV.rows();
  for( unsigned long c = 0; c < solV.cols(); c++ )
    {
    for( unsigned int r = 0; r < rows; r++ )
      {
      iripMat( c, r ) = solV( r, c );
      }
    }

  // get the full image eigenvectorCoefficients matrix
  solV = filter->GetEigenvectorCoefficients();
  Rcpp::NumericMatrix eripMat( solV.cols(), solV.rows() );
  rows = solV.rows();
  for( unsigned long c = 0; c < solV.cols(); c++ )
    {
    for( unsigned int r = 0; r < rows; r++ )
      {
      eripMat( c, r ) = solV( r, c );
      }
    }
  float varx = filter->GetAchievedVarianceExplained();
  return(
      Rcpp::List::create(
        Rcpp::Named("canonicalFrame") = r_outimg,
        Rcpp::Named("basisMat") = ripMat,
        Rcpp::Named("imagePatchMat") = iripMat,
        Rcpp::Named("evecCoeffs") = eripMat,
        Rcpp::Named("varex") = varx )
      );

  return( r_outimg );
}

RcppExport SEXP patchAnalysis(
  SEXP r_inimg,
  SEXP r_maskimg,
  SEXP r_outimg,
  SEXP r_patchRadius,
  SEXP r_patchSamples,
  SEXP r_patchVar,
  SEXP r_meanCenter,
  SEXP r_verbose )
{
try
  {
  Rcpp::S4 antsImage( r_inimg );
  std::string pixeltype = Rcpp::as< std::string >( antsImage.slot( "pixeltype" ));
  unsigned int dimension = Rcpp::as< int >( antsImage.slot( "dimension" ) );

  if ( (pixeltype == "float") & ( dimension == 2 ) )
    {
    typedef float PixelType;
    const unsigned int dim = 2;
    typedef itk::Image< PixelType, dim > ImageType;
    return Rcpp::wrap(
      patchAnalysisHelper< ImageType >(
        r_inimg, r_maskimg, r_outimg, r_patchRadius,
        r_patchSamples, r_patchVar, r_meanCenter, r_verbose )
      );
    }
  else if ( (pixeltype == "float") & ( dimension == 3 ) )
    {
    typedef float PixelType;
    const unsigned int dim = 3;
    typedef itk::Image< PixelType, dim > ImageType3D;
    return Rcpp::wrap(
      patchAnalysisHelper< ImageType3D >(
          r_inimg, r_maskimg, r_outimg, r_patchRadius,
          r_patchSamples, r_patchVar, r_meanCenter, r_verbose )
      );
    }
  else
    {
    Rcpp::stop("Unsupported image dimension or pixel type.");
    }
}

catch( itk::ExceptionObject & err )
{
  Rcpp::Rcout << "ITK ExceptionObject caught!" << std::endl;
  forward_exception_to_r( err );
}
catch( const std::exception& exc )
{
  Rcpp::Rcout << "STD ExceptionObject caught!" << std::endl;
  forward_exception_to_r( exc );
}
catch( ... )
{
  Rcpp::stop( "C++ exception (unknown reason)");
}
 return Rcpp::wrap(NA_REAL); // should not be reached
}
