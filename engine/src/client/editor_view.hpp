// SPDX-License-Identifier: GPL-3.0-or-later
// A szerkesztő rajzolása (M11): a világ fölé a régiók, pontok, kamerapontok és az ecset, bal oldalt a paletta, alul az
// állapotsor – a HUD alapelemeivel (képernyő-koordinátában).
#pragma once

#include <functional>
#include <string>

#include "client/editor_doc.hpp"
#include "client/editor_ui.hpp"
#include "client/hud.hpp"
#include "client/world.hpp"

namespace totu::client {

using Translate = std::function<std::string(const std::string&)>;
void draw_editor(Hud& hud, const EditorUi& ui, const EditorDoc& doc, const Camera& cam, int w, int h,
                 const Translate& tr, const std::vector<std::string>& problems);

}  // namespace totu::client
