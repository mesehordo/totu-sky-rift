// SPDX-License-Identifier: GPL-3.0-or-later
// Az időjárás látványa (M5, 5. lépés; D35): az időjárás fajtájából és erősségéből a megjelenítő paraméterei – a
// környezeti fény szorzója, az árnyék gyengülése, a színkorrekció (telítettség, kontraszt), a pára (köd, homok, hamu),
// a vándorló felhőárnyék, a hőremegés, a csapadék (eső, hó, homok, hamu: sűrűség, sebesség, irány, hossz, szín), a
// villámlás és a talajra hulló cseppek. A szimuláció időjárása (M6, saját véletlenszám-folyammal) adja a fajtát és az
// erősséget; a megjelenítés csak ezt használja. Grafikus kártya nélkül tesztelhető.
#pragma once

#include <cstdint>
#include <string_view>

namespace totu::render {

enum class Weather : uint8_t { Clear, Cloudy, Rain, Storm, Fog, Snow, Blizzard, Sandstorm, Heat, Ashfall, Count };

enum class Precip : uint8_t { None = 0, Rain = 1, Snow = 2, Sand = 3, Ash = 4 };

struct WeatherLook {
  float ambient[3] = {1, 1, 1};   // a napszak környezeti fényét szorozza
  float shadow = 1;               // az árnyék erősségének szorzója (felhős égen gyengébb)
  float lights = 0;               // a fényforrások legkisebb ereje (sötét viharban nappal is égnek)
  float saturation = 1, contrast = 1;
  float haze[3] = {0.7f, 0.72f, 0.75f};
  float haze_amount = 0;          // 0..1: a pára fedése (a pára a világban vándorol, foltos)
  float clouds = 0;               // a vándorló felhőárnyék erőssége
  float heat = 0;                 // hőremegés (a kép hullámzik)
  Precip precip = Precip::None;
  float density = 0;              // a csapadék sűrűsége (0..1)
  float speed = 0;                // esési sebesség (képernyő-képpont / s)
  float length = 0;               // csíkhossz képpontban (0: pehely)
  float size = 1;                 // pehely sugara / csík vastagsága (képpont)
  float color[4] = {1, 1, 1, 0};  // csapadék színe és fedése
  float fall_x = 0, fall_y = 1;   // esési irány (egységvektor, képernyő: y lefelé)
  float lightning = 0;            // villámok percenként
  float splashes = 0;             // talajra hulló cseppek másodpercenként (képernyőnyi területen)
  float wind = 0;                 // szél a részecskéknek (képpont / s², + = jobbra)
};

// A nevek a pályaadat és a parancssor szerint (ékezet nélkül): derult, felhos, eso, zivatar, kod, ho, hovihar,
// homokvihar, hoseg, hamueso.
const char* weather_name(Weather w);
bool weather_from_name(std::string_view name, Weather* out);

// `intensity` 0..1 (a spec szerint enyhe ≈ 0,5, erős = 1); 0 esetén a derült ég látványa.
WeatherLook weather_look(Weather w, float intensity);
// Fokozatos átmenet két időjárás között (t = 0: a, t = 1: b). A csapadék fajtája a felénél vált, a sűrűsége közben
// a nullán megy át – így nem ugrik.
WeatherLook mix(const WeatherLook& a, const WeatherLook& b, float t);
// A villám felvillanása (0..1) az idő függvényében, adott gyakoriság mellett; ugyanarra az időre mindig ugyanaz.
float lightning_flash(double time, float per_minute);

}  // namespace totu::render
