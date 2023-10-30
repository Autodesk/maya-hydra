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
#ifndef FVP_COLOR_PREFERENCES_H
#define FVP_COLOR_PREFERENCES_H

#include "flowViewport/api.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/token.h>

#include <ufe/notification.h>
#include <ufe/observer.h>
#include <ufe/subject.h>

#include <memory>
#include <unordered_map>

namespace FVP_NS_DEF {

class ColorPreferencesTranslator;

/// \class ColorPreferences
///
/// \brief Singleton used to retrieve color preferences and
/// subscribe to \p ColorChanged notifications.
///
/// The \p ColorPreferences class acts as the entry point for Flow Viewport
/// users to get informed about the color preferences of the host.
/// It is a singleton that provides two services :
/// - It rebroadcasts the notifications it receives from the host. It will only rebroadcast
/// notifications of type ColorChanged.
/// - It forwards the \p getColor() calls it receives to its \p ColorPreferencesTranslator.
/// The \p ColorPreferencesTranslator must be supplied by the host to
/// provide the translation between the host and the Flow Viewport.
///
class FVP_API ColorPreferences final
    : public Observer
    , public Subject
    , public std::enable_shared_from_this<ColorPreferences>
{
public:
    ~ColorPreferences() override = default;

    /// @brief Returns the singleton instance of this class. The referenced object is managed by a
    /// \p shared_ptr, enabling the use of \p shared_from_this(). Creates a new instance if none
    /// currently exists.
    /// @return The singleton instance of this class, backed by a \p shared_ptr.
    static ColorPreferences& getInstance();

    /// @brief Deletes the current singleton instance of this class, if one exists.
    static void deleteInstance();

    /// @brief Operator overload to receive notifications about color changes.
    /// This will in turn rebroadcast the notification to all its observers,
    /// <em> but only if the notification is of type ColorChanged. </em>
    /// If the notification is of any other type, it will not be rebroadcast.
    /// This will get called automatically by calling notify on this \p Subject,
    /// you do not need to call this manually.
    void operator()(const Notification& notification) override;

    /// @brief Retrieve the color value for a given color preference.
    ///
    /// @param[in] preference The color preference we want to know the color of.
    /// @param[out] outColor Output parameter to store the color resulting from
    /// the query.
    ///
    /// @return True if the color was found and \p outColor was populated,
    /// false otherwise.
    bool getColor(const PXR_NS::TfToken& preference, PXR_NS::GfVec4f& outColor) const;

    /// @brief Set the translator for which to forward \p getColor() calls to.
    ///
    /// @param[in] translator is a pointer to an implementation of the
    /// \p ColorPreferencesTranslator interface. It is the object to which
    /// the \p getColor() calls will be forwarded to.
    void setTranslator(const std::shared_ptr<ColorPreferencesTranslator>& newTranslator);

private:
    ColorPreferences() = default;
    ColorPreferences(const ColorPreferences&) = delete;
    ColorPreferences(ColorPreferences&&) = delete;
    ColorPreferences& operator=(const ColorPreferences&) = delete;
    ColorPreferences& operator=(ColorPreferences&&) = delete;

    std::shared_ptr<ColorPreferencesTranslator> _translator;
};

} // namespace FVP_NS_DEF

#endif // FVP_COLOR_PREFERENCES_H
