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
#ifndef MAYA_COLOR_PREFERENCES_TRANSLATOR_H
#define MAYA_COLOR_PREFERENCES_TRANSLATOR_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/token.h>

#include <maya/MCallbackIdArray.h>

#include <flowViewport/colorPreferences/fvpColorPreferencesTranslator.h>

#include <map>
#include <memory>
#include <string>

namespace MAYAHYDRA_NS_DEF {

/// \class MayaColorPreferencesTranslator
///
/// \brief Singleton used to retrieve color preferences from Maya and track their changes
/// to feed the Flow Viewport layer.
///
/// The \p MayaColorPreferencesTranslator class acts as the translation layer
/// for color preferences between Maya and the Flow Viewport layer.
/// It is a singleton that serves two purposes :
/// - It listens to Maya color preferences changes to create and send \p Fvp::ColorChanged
/// notifications.
/// - It implements the \p Fvp::ColorPreferencesTranslator interface.
///
/// This class is designed and expected to have only one observer : the \p Fvp::ColorPreferences
/// singleton, though this is not strictly enforced.
///
class MayaColorPreferencesTranslator
    : public Fvp::ColorPreferencesTranslator
    , public Fvp::Subject
    , public std::enable_shared_from_this<MayaColorPreferencesTranslator>
{
public:
    /// @brief Returns the singleton instance of this class. The referenced object is managed by a
    /// \p shared_ptr, enabling the use of \p shared_from_this(). Creates a new instance if none
    /// currently exists.
    /// @return The singleton instance of this class, backed by a \p shared_ptr.
    static MayaColorPreferencesTranslator& getInstance();

    /// @brief Deletes the current singleton instance of this class, if one exists.
    static void deleteInstance();

    ~MayaColorPreferencesTranslator() override;

    /// @brief Retrieve the color value for a given color preference.
    ///
    /// @param[in] preference The color preference we want to know the color of.
    /// @param[out] outColor Output parameter to store the color resulting from
    /// the query.
    ///
    /// @return True if the color was found and \p outColor was populated,
    /// false otherwise.
    bool getColor(const PXR_NS::TfToken& preference, PXR_NS::GfVec4f& outColor) const override;

private:
    struct MayaRGBAColor
    {
        std::string     colorName;
        PXR_NS::GfVec4f color;
    };

    struct MayaIndexedColor
    {
        std::string     colorName;
        std::string     tableName;
        PXR_NS::GfVec4f color;
    };

    MayaColorPreferencesTranslator();
    MayaColorPreferencesTranslator(const MayaColorPreferencesTranslator&) = delete;
    MayaColorPreferencesTranslator(MayaColorPreferencesTranslator&&) = delete;
    MayaColorPreferencesTranslator& operator=(const MayaColorPreferencesTranslator&) = delete;
    MayaColorPreferencesTranslator& operator=(MayaColorPreferencesTranslator&&) = delete;

    void trackRGBAColor(const std::string& mayaColorName, const PXR_NS::TfToken& fvpColorToken);
    void trackIndexedColor(
        const std::string&     mayaColorName,
        const std::string&     colorTableName,
        const PXR_NS::TfToken& fvpColorToken);

    static void onPreferencesChanged(void* clientData);
    void        syncPreferences();

    MCallbackIdArray                            _callbackIds;
    std::map<PXR_NS::TfToken, MayaRGBAColor>    _rgbaColorsCache;
    std::map<PXR_NS::TfToken, MayaIndexedColor> _indexedColorsCache;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYA_COLOR_PREFERENCES_TRANSLATOR_H
