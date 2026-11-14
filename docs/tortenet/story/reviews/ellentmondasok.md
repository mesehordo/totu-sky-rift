# Ellentmondások és javítási javaslatok

*© 2026 Mesehordo*

Az 1. lépésben talált eltérések a világdokumentum (v04) és a játék adatai között, illetve a dokumentumon belül. Az „Állapot” oszlop azt mutatja, mit tettem a szerkesztett szövegben.

| # | Eltérés | Javaslat | Állapot |
|---|---|---|---|
| 1 | A v04 12. fejezete négy nyersanyagot ír (fa, kő, étel, meteorit). A játékban fa, arany és meteorit van, plusz Áhítat és Tóének. | A fejezet átírása a jelenlegi gazdaságra. | **Javítva** a szerkesztett szövegben. |
| 2 | A Huk’ucháról: „pajzsot nem használnak”. A játékban van Kagylópajzsos („a nép egyetlen pajzsos harcosa”). | „A Kagylópajzsosok kivételével.” A kagylóteknő a szövegben kivételes, mélyvízi hagyomány. | **Javítva.** |
| 3 | Sairen (Ooni) és Relluk (Huk’ucha) a történet főszereplői, de a játék hősei Akahana, Shiro, Karasu, illetve Kvarra, Bul’gok, Iriss. | Kampányhős-szabály. | **Eldöntve:** a küldetésekben szerepelhetnek; sima csatában az alap 3 hős marad. |
| 4 | A főtörténet legátusa Maurus, a játék aurethi hőse Cassius Ferrum legátus. | D3 javaslat: Cassius a Hetedik Légió legátusa, a fináléban megtagadja Maurus parancsát. | **Eldöntve:** elfogadva (D3 → kánon). |
| 5 | A v04 szerint az aurethiek „ugyanahhoz a régi, addig néma istenhez” imádkoztak; a játékban Ferrox, Arcanor és Umbrax szobrai is állnak („öreg istenek”). | Többistenhit, amelyben Ordan a templomi hálózat istene (D1, D2). | **Eldöntve:** elfogadva (D1, D2 → kánon). |
| 6 | A v04 nem határozza meg a holdszilánkot, a Tokla-mechanika és az ursan hadjárat mégis épít rá. | D9 javaslat, két egymásnak ellentmondó népi magyarázattal. | **Beépítve, nyitva hagyva.** |
| 7 | A Wullofról a v04 fennsíki szélfogókat és folyóvárosokat ír; a játék vörös pusztát, eukaliptuszerdőt, emukat, bölényeket és Groomikat. | Nem ellentmondás, csak hiány: a szöveg mindkettőt használja (folyóvidék, vörös fennsík, puszta). | **Kiegészítve.** |
| 8 | A v04 szerint „nem minden ursan harcos”; a játékban riadóra minden telepes fegyvert ragad. | Összeegyeztethető: nincs hivatásos hadseregük, de mindenki fegyverforgató. A szöveg kimondja. | **Kiegészítve.** |
| 9 | „Micéliákpel” elírás a v04 5. fejezetében. | „Micéliákkal”. | **Javítva.** |
| 10 | A `FACTIONS_LORE.md` régi micélia-mechanikája (Spórafonal, Pöfeteg-összeolvadás, Álomkert, Lebegőbölcső) eltér a mostani szövetséges adatoktól (Termőtest, Szívgomba, Emlékfa; 5 egység). | A történet csak az Emlékfát és a Szívgombát használja, amelyek mindkét változatban megvannak. | **Megkerülve.** A lore-fájl frissítése külön feladat. |
| 11 | A Wullof falkabónusza a `FACTIONS_LORE.md` szerint +8%, a játékban +10%. | Csak mechanikai; a történetet nem érinti. | Jelezve. |
| 12 | Az Égi kegy „Ordan villáma” a többjátékos módban a finálé után is létezik, holott Ordan központi személyisége megszűnik. | A többjátékos mód nem kronologikus. Ha kell magyarázat: a régi szentélyek automatikus védelmi rendszerei (a v04 9. fejezete szerint megmaradnak). | Jelezve. |
| 13 | Croftian játékhősök (Vor-Uthul, Ssarha) és több főfaji hős (Itzcoatl, Xochi, Hjalmar, Bjarka, Varnu, Kurrin, Akahana, Shiro, Karasu, Kvarra, Bul’gok, Iriss) nem szerepel a főtörténetben. | Mellékszereplőként rövid helyet kapnak a népek fejezeteiben és a karakterlapokon, a cselekményt nem változtatják. | **Beépítve, minimálisan.** |
| 14 | Itzcoatl végső képessége „Kukulkán alakja” volt: valódi kultúrából átvett istennév. | Saját név. | **Javítva:** „Quetzlaru alakja” a játékadatban (`data/abilities.json`, `data/lang/en.json`, `tools/factions_v2.py`, `tools/lang_en_data.py`) és a dokumentációkban; az azonosító (`h_itz_kukulkan`) és az ikonfájl neve változatlan. |
| 15 | A Tokla-maszkokat a játék „busó”-nak nevezi (valós magyar népszokás). | Megtartva: a szöveg a Tokla saját szokásaként írja le, a mohácsi hagyomány részleteit nem másolja. | Jelezve. |
| 16 | Az Ooni kapuvárosok „Kőoszlopos” csarnokai (v04) és a játék „márványoszlopai”. | Összeegyeztethető. | — |
