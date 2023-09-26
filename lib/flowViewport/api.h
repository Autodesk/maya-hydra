// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef FVP_API_H
#define FVP_API_H

#ifdef __GNUC__
#define FVP_API_EXPORT __attribute__((visibility("default")))
#define FVP_API_IMPORT
#elif defined(_WIN32) || defined(_WIN64)
#define FVP_API_EXPORT __declspec(dllexport)
#define FVP_API_IMPORT __declspec(dllimport)
#else
#define FVP_API_EXPORT
#define FVP_API_IMPORT
#endif

#if defined(FVP_STATIC)
#define FVP_API
#else
#if defined(FVP_EXPORT)
#define FVP_API FVP_API_EXPORT
#else
#define FVP_API FVP_API_IMPORT
#endif
#endif

// Convenience symbol versioning include: because api.h is widely
// included, this reduces the need to explicitly include flowViewport.h.
#include <flowViewport/flowViewport.h>

#endif // FVP_API_H
