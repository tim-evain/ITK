/*=========================================================================
 *
 *  Copyright NumFOCUS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/


#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkSimpleFilterWatcher.h"
#include "itkFFTPadImageFilter.h"
#include "itkZeroFluxNeumannBoundaryCondition.h"
#include "itkPeriodicBoundaryCondition.h"
#include "itkConstantBoundaryCondition.h"
#include "itkTestingMacros.h"

#include "itkObjectFactoryBase.h"
#include "itkVnlForwardFFTImageFilter.h"
#if defined(ITK_USE_FFTWD) || defined(ITK_USE_FFTWF)
#  include "itkFFTWForwardFFTImageFilter.h"
#endif


int
itkFFTPadImageFilterTest(int argc, char * argv[])
{
  if (argc < 5)
  {
    std::cerr << "Usage: " << std::endl;
    std::cerr << itkNameOfTestExecutableMacro(argv) << "  inputImageFile outputImageFile greatestPrimeFactor padType"
              << std::endl;
    return EXIT_FAILURE;
  }

  //
  //  The following code defines the input and output pixel types and their
  //  associated image types.
  //
  constexpr unsigned int Dimension = 2;
  using PixelType = unsigned char;
  using ImageType = itk::Image<PixelType, Dimension>;


  // readers/writers
  using ReaderType = itk::ImageFileReader<ImageType>;
  using WriterType = itk::ImageFileWriter<ImageType>;
  using FFTPadType = itk::FFTPadImageFilter<ImageType>;


  auto reader = ReaderType::New();
  reader->SetFileName(argv[1]);

  itk::ZeroFluxNeumannBoundaryCondition<ImageType> zfnCond;
  itk::ConstantBoundaryCondition<ImageType>        zeroCond;
  itk::PeriodicBoundaryCondition<ImageType>        wrapCond;

  // FFTPadImageFilter requires backend for ForwardFFTImageFilter to get prime factor
#ifndef ITK_FFT_FACTORY_REGISTER_MANAGER // Manual factory registration is required for ITK tests
#  if defined(ITK_USE_FFTWD) || defined(ITK_USE_FFTWF)
  itk::ObjectFactoryBase::RegisterInternalFactoryOnce<itk::FFTImageFilterFactory<itk::FFTWForwardFFTImageFilter>>();
#  endif
  itk::ObjectFactoryBase::RegisterInternalFactoryOnce<itk::FFTImageFilterFactory<itk::VnlForwardFFTImageFilter>>();
#endif

  // Create the filters
  auto fftpad = FFTPadType::New();
  fftpad->SetInput(reader->GetOutput());
  fftpad->SetSizeGreatestPrimeFactor(std::stoi(argv[3]));
  std::string padMethod = argv[4];
  if (padMethod == "Mirror")
  {
    //    fftpad->SetBoundaryCondition( &mirrorCond );
  }
  else if (padMethod == "Zero")
  {
    fftpad->SetBoundaryCondition(&zeroCond);
  }
  else if (padMethod == "ZeroFluxNeumann")
  {
    fftpad->SetBoundaryCondition(&zfnCond);
  }
  else if (padMethod == "Wrap")
  {
    fftpad->SetBoundaryCondition(&wrapCond);
  }
  itk::SimpleFilterWatcher watchFFTPad(fftpad, "fftpad");

  auto writer = WriterType::New();
  writer->SetInput(fftpad->GetOutput());
  writer->SetFileName(argv[2]);

  ITK_TRY_EXPECT_NO_EXCEPTION(writer->Update());


  // Ensure we can build with a different output image type.
  using OutputImageType = itk::Image<double, Dimension>;
  using FFTPadWithOutputType = itk::FFTPadImageFilter<ImageType, OutputImageType>;
  auto fftPadWithOutput = FFTPadWithOutputType::New();
  (void)fftPadWithOutput;

  return EXIT_SUCCESS;
}
