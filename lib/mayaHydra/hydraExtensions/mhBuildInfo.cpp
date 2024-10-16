//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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

#include <mayaHydraLib/mhBuildInfo.h>

namespace MAYAHYDRA_NS_DEF {

int         MhBuildInfo::buildNumber() { return MAYAHYDRA_BUILD_NUMBER; }
const char* MhBuildInfo::gitCommit()   { return MAYAHYDRA_GIT_COMMIT;   }
const char* MhBuildInfo::gitBranch()   { return MAYAHYDRA_GIT_BRANCH;   }
const char* MhBuildInfo::cutId()       { return MAYAHYDRA_CUT_ID;       }
const char* MhBuildInfo::buildDate()   { return MAYAHYDRA_BUILD_DATE;   }

} // namespace MAYAHYDRA_NS_DEF
