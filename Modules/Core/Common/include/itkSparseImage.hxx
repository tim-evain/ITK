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
#ifndef itkSparseImage_hxx
#define itkSparseImage_hxx

#include "itkDataObject.h"

namespace itk
{
template <typename TNode, unsigned int VImageDimension>
SparseImage<TNode, VImageDimension>::SparseImage()
{
  m_NodeList = NodeListType::New();
  m_NodeStore = NodeStoreType::New();
}

template <typename TNode, unsigned int VImageDimension>
void
SparseImage<TNode, VImageDimension>::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
}

template <typename TNode, unsigned int VImageDimension>
void
SparseImage<TNode, VImageDimension>::Initialize()
{
  Superclass::Initialize();
  m_NodeList = NodeListType::New();
  m_NodeStore = NodeStoreType::New();
}
} // end namespace itk

#endif
