// SPDX-License-Identifier: GPL-3.0-or-later
// Napszak a megjelenítőnek (M5, D35): a napszak tört részéből (0 = a hajnal kezdete, 1 = a következő hajnal) a
// környezeti fény színe, a nap iránya (az árnyék vetülete a képernyőn) és az árnyék erőssége. A szimuláció napszaka
// (M6) adja majd az értéket; a megjelenítés csak ezt a függvényt használja. Grafikus kártya nélkül tesztelhető.
#pragma once

namespace totu::render {

struct DayLight {
  float ambient[3] = {1, 1, 1};   // a jelenetet ezzel szorozzuk (a fényforrások ehhez adódnak)
  float shadow_x = 0.62f, shadow_y = 0.44f;   // az árnyék vetülete magasság-képpontonként (a képernyőn, y lefelé; a régi játék iránya)
  float shadow_alpha = 0.59f;     // az árnyék legsötétebb erőssége (0..1)
  float lights = 0;               // a fényforrások ereje (0 nappal, 1 éjjel)
};

// Fázisok (D35): hajnal 0–0,05, nappal 0,05–0,50 (dél 0,275), alkony 0,50–0,55, éjszaka 0,55–1.
DayLight daylight(double t);

}  // namespace totu::render
