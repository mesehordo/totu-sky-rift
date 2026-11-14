# Egységtípusok és erősségek – átnézésre

*© Mesehordo – TOTU: Sky Rift. Ötletdoboz Ö24. A `python tools/egysegtipusok.py` állítja elő; a besorolás kézzel felülírható (a script `KEZI` szótára), vagy írd meg, melyik egység kerüljön máshova.*

## Típusok és szorzók
**196 egység:** ⚔ Közelharcos 44, ✨ Varázsló 35, 🗿 Óriás / szörny 29, 🏹 Lövész 27, 🛡 Lándzsás / pajzsos 19, 🪶 Repülő 15, ⛏ Munkás 11, 🐎 Lovas / hátas 8, 💥 Ostromgép 8


| Típus | Mi ez | Erős ez ellen | Gyenge ez ellen |
|---|---|---|---|
| ⚔ **Közelharcos** | kard, buzogány, balta | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | 🏹 Lövész, 🗿 Óriás / szörny |
| 🛡 **Lándzsás / pajzsos** | lándzsa, dárda, pajzsfal | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | ⚔ Közelharcos, ✨ Varázsló, 💥 Ostromgép |
| 🏹 **Lövész** | íj, parittya, dobófegyver | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | 🐎 Lovas / hátas |
| 🐎 **Lovas / hátas** | gyors hátas harcos | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | 🛡 Lándzsás / pajzsos, 🗿 Óriás / szörny |
| 🗿 **Óriás / szörny** | nagy testű lény vagy gép | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | 🛡 Lándzsás / pajzsos, ✨ Varázsló |
| ✨ **Varázsló** | mágiahasználó, támogató | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | ⚔ Közelharcos, 🐎 Lovas / hátas, 🪶 Repülő |
| 🪶 **Repülő** | légi egység | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | 🏹 Lövész |
| 💥 **Ostromgép** | távoli, épületromboló | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🐎 Lovas / hátas, 🪶 Repülő |
| ⛏ **Munkás** | gyűjt, épít | – | – |

A szorzó a támadó sebzésére vonatkozik (a páncél előtt). A karakterlapon a portré mellett a típus ikonja, alatta „Erős:” és „Gyenge:” ikonsor; egérrel fölé állva a súgó megmondja a szorzót.

## Szürke elfek

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Kőfaragó | ⛏ Munkás | – | – |  |
| Tollas harcos | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Jáde-íjász | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Raptorlovas | 🐎 Lovas / hátas | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | – |  |
| Napszóró pap | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Quetzal | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Tollaskoronás rex | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Szelai | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – | hős |
| Csillagdárdás | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Obszidián-vető | 💥 Ostromgép | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Jádeőr | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Itzcoatl | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | hős |
| Xochi | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – | hős |
| Nap főpapja | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Jaguár-orvlövész | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Csillagjós | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |

## Ursanok

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Telepes | ⛏ Munkás | – | – |  |
| Pajzshordozó | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Szigonyvető | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Medvebőrös berserker | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Rúnapap | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – | nem harcol |
| Kardfogú lovas | 🐎 Lovas / hátas | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | – |  |
| Viharholló | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Harci mammut | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Ragna Kőváll | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | hős |
| Dárdaőr | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Ostromszán | 💥 Ostromgép | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Kardfogú tigris | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Hjalmar Fagyjós | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – | hős |
| Bjarka, a Vadak Ura | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – | hős |
| Rúnakő | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – | nem harcol |
| Medvealakváltó | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Harci kürtös | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Rúnaóriás | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |

## Croftianok

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Csápszolga | ⛏ Munkás | – | – |  |
| Karmos | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Tüskeköpő | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Savköpő | 💥 Ostromgép | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Agyfaló | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Szárnyas rém | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Csápos horror | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Keth | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – | hős |
| Páncélbogár | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Gubó | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | nem harcol |
| Szemhordozó | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – | nem harcol |
| Vor-Uthul | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | hős |
| Ssarha, a Raj Hangja | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – | hős |
| Robbanó atka | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | 🏛 Épületek ×1,25 | öngyilkos tömegegység |
| Vadászkarmos | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Ostromtaréj | 💥 Ostromgép | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Ásóféreg | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |

## Wullof

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Gyűjtögető | ⛏ Munkás | – | – |  |
| Lándzsás harcos | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Bumerángvető | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Holdsámán | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Szellemfarkas | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Bölénylovas | 🐎 Lovas / hátas | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | – |  |
| Viharkakadu | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Termeszvár-óriás | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Vándorbatyu | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – | nem harcol |
| Tava | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – | hős |
| Dárdavető portyázó | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Varnu, a Holdsámán | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – | hős |
| Kurrin Harcfőnök | 🐎 Lovas / hátas | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | – | hős |
| Emu | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | nem harcol |
| Bölény | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Groomi | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Csordavezér bika | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Groomi-fióka | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Groomi-anya | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Totemfaragó | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Álomjáró | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |

## Aurethi Birodalom

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Fabrikátor | ⛏ Munkás | – | – |  |
| Légiós | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Ballistarius | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Szellemidéző | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Centurio-mecha | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Aquila-drón | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Kolosszus | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Szellemlégiós | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Szellemkolosszus | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Livia Varr | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | 🪶 Repülő ×1,5 | leírás: légvédelem; hős |
| Nera | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – | hős |
| Onager-mecha | 💥 Ostromgép | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Cassius Ferrum legátus | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | 🪶 Repülő ×1,5 | leírás: légvédelem; hős |
| Lélekhorgony | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | nem harcol |
| Praetoriánus | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Arató | ⛏ Munkás | – | – | nem harcol |

## Huk’ucha

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Tóásó | ⛏ Munkás | – | – |  |
| Csúzlis | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Kampókardos | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Tóidéző | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Tavi lény | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – | idézett |
| Gőtelovas | 🐎 Lovas / hátas | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | – |  |
| Szitakötő-felderítő | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Teknőshátas mozsár | 💥 Ostromgép | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Kvarra, a Tóanya | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – | hős |
| Bul’gok Kampókirály | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | hős |
| Iriss Szivárványgyöngy | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – | hős |
| Áradás papja | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Iszapbéka | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Gyöngyvető | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Kagylópajzsos | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Gyöngyóriás | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Mélységi hívó | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Ikrarakó | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | nem harcol |
| Ikra-csomó | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | nem harcol; idézett |
| Ebihal | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | idézett, rövid életű; idézett |

## Ooni

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Lámpásvivő | ⛏ Munkás | – | – |  |
| Oni harcos | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Kőhajító oni | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Nian őrző | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Tengu követ | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Kitsu-mata | 🏹 Lövész | (a lövész típusszorzói) | – | D29: korábban Kitsune-maszkos, varázsló |
| Illúzió-más | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | idézett |
| Kapudöngető | 💥 Ostromgép | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🏛 Épületek ×2 | leírás: épületek ellen; faltörő kos – közelről |
| Ō-oni | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Akahana, a Vörös Buzogány | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | hős |
| Shiro, a Kapuőr | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – | hős |
| Karasu, a tengu bölcs | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – | hős |
| Tomboló oni | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Taiko-dobos | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Kőkutya-őr | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Lámpáspap | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Hollótengu-harcos | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Viharhívó tengu | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Favágó oni | ⛏ Munkás | – | – |  |
| Tanuki-bányász | ⛏ Munkás | – | – |  |

## Micéliák (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Spóraszedő | ⛏ Munkás | – | – |  |
| Pöfeteg | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Fonalhálós | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Tüskegomba-lövész | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | 🪶 Repülő ×1,5 | leírás: légvédelem |
| Emlékőr | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |

## Pinguu (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Jégdárdás | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | 🐎 Lovas / hátas ×1,25 | a jéghegy lelassítja a lovasokat |
| Hópajzsos | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Rozmárlovas | 🐎 Lovas / hátas | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | – |  |
| Ár-apály sámán | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Kajakos portyázó | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |

## Ghoblin (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Kőharapó | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | 🏛 Épületek ×1,5 | épületek ellen bónusza van; a köveket is megrágja |
| Sziklagörgető | 💥 Ostromgép | 🏛 Épületek ×2 · 🛡 Lándzsás / pajzsos ×1,25 | 🏛 Épületek ×1,5 | épületek ellen bónusza van |
| Pajzsköves | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Mészköpő | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Kőhéjú öreg | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Mészbombás manó | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | 🏛 Épületek ×2 | leírás: épületek ellen; öngyilkos, épületromboló |

## Mhonyo (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Hajnalpenge | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Szerkezet-íjász | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Szikraőr | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Kardkovács | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Fogaskerék-gólem | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |

## Daresh (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Fullánclándzsás | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Gyantapajzsos | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Rajénekes | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Darázsraj | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Szárnyas íjász | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |

## Tokla (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Fagyóriás | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Harangjáró | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Hóvihar-busó | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Zúzmaratörpe | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Jégvető törpe | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |

## Galóca (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Fogaskalap | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Tüskebunkós | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Óriásgalóca | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Spóraköpő | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Gyökérgáncsoló | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |

## Niil-elfek (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Papiruszlándzsás | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Nádhajós íjász | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Lótuszpajzsos | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Árapálypap | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Krokodilőr | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |

## Tűz-elfek (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Homokszablyás | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Parázsíjász | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Homokvihar-táncos | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Hőszövő | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Délibáb-csaló | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Délibáb-más | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – | idézett |

## Tendüür (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Lámpásőr | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Szélhímző | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Tövisvívó | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Lombíjász | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Tölgyőr | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |

## Nila-uchana (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Szörnykötő | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Rakshasa | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – |  |
| Makara-idéző | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Nága szövetséges | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Makara | 🐎 Lovas / hátas | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | – |  |
| Megkötött rakshasa | 🗿 Óriás / szörny | ⚔ Közelharcos ×1,25 · 🐎 Lovas / hátas ×1,25 · 🏛 Épületek ×1,5 | – | idézett |

## Zhan-zhama (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Csirkepásztor | ⚔ Közelharcos | – | – | olcsó tömegegység – nincs külön erőssége |
| Tojásdobáló | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |
| Kakaslovas | 🐎 Lovas / hátas | 🏹 Lövész ×1,5 · ✨ Varázsló ×1,5 · 💥 Ostromgép ×1,5 | – |  |
| Tolljós | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Madárszellem | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |

## Shrissh-matah (szövetséges nép)

| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |
|---|---|---|---|---|
| Rúnaszövő | ✨ Varázsló | 🗿 Óriás / szörny ×1,5 · 🛡 Lándzsás / pajzsos ×1,25 | – |  |
| Sziklafutó | ⚔ Közelharcos | 🛡 Lándzsás / pajzsos ×1,25 · ✨ Varázsló ×1,25 | – |  |
| Pajzsbogár | 🛡 Lándzsás / pajzsos | 🐎 Lovas / hátas ×1,5 · 🗿 Óriás / szörny ×1,25 | – |  |
| Tűszúnyog | 🪶 Repülő | 💥 Ostromgép ×1,5 · ✨ Varázsló ×1,25 | – |  |
| Tüskelövő | 🏹 Lövész | ⚔ Közelharcos ×1,25 · 🪶 Repülő ×1,5 | – |  |

