# A régi (Python) játék kinézete és az új motor – összevetés

Kelt: 2026-11-19. Forrás: a régi `rts/ui.py` rajzolói és két kép ugyanarról a helyzetről (Ursanok, a bázis az első
percben). A tételekről a tulajdonos **egyesével** dönt: átvesszük, módosítva vesszük át, vagy marad az új.

| # | Tétel | Régi játék | Új motor most | Döntés |
|---|---|---|---|---|
| 1 | Kijelölés jele | „pecsét” a figura alatt: lágy belső fény, sötét kontúrú kettős gyűrű, négy nyílhegy a tengelyeken, a csapat színében, a figura méretéhez igazítva (`sel_sigil`) | lapos zöld folt (24 × 10 képpont), minden egységnél ugyanakkora | **átvéve** (2026-11-19): a régi pecsét, saját zöld, idegen piros, a figura méretével, pulzálva (`client/sigil`; kép: `kepek/utan_kijeloles_pecset.jpg`). A szövetséges kék még nincs (a pillanatkép nem tudja a szövetséget) |
| 2 | Gyülekezőpont | lengő, fecskefarkú zászló a csapat színében, saját árnyékkal; az épülettől menetelő nyilak vezetnek oda, a végén kis pecsét (`draw_rally_flag`, `draw_rally_path`) | a pont megvan (parancs), de a pályán nem látszik | **átvéve**: a kijelölt saját épületnél zászló (csapatszín, lengés, árnyék), menetelő nyilak, kis pecsét; jobb kattintással állítható (eddig a kliens nem adta ki a parancsot). Kép: `kepek/utan_gyulekezopont.jpg` |
| 3 | Mozgáscél jelzése | a kijelölt csoporttól a célig menetelő zöld nyilak, a célnál befelé mutató pecsét (`draw_move_indicator`) | nincs | **átvéve**: a legutóbbi saját mozgásparancs céljáig, amíg a kijelöltek közül valaki oda tart. Kép: `kepek/utan_mozgascel.jpg` |
| 4 | Életerő-csík | 4 px; zöld > 60 % > sárga > 30 % > piros; fölötte kék pajzscsík, alatta lila energiacsík (saját egységnél), idézett lénynél „N mp” visszaszámlálás; az egér alatti egységnél is | 3 px, folytonos zöld → sárga → piros; csak kijelölve vagy sérülten; pajzs, energia, visszaszámlálás nincs | **átvéve**: csak az egér alatti egységnél / épületnél, 4 px, a régi színlépcsőkkel, pajzs fölötte, energia alatta (saját egység). Az idézett lény visszaszámlálása még nincs (szöveg kellene a világba). Kép: `kepek/utan_eletero_csik.jpg` |
| 5 | Csapatszín az épületeken | a lobogók, vásznak a játékos színében | a főépület vászna a kép eredeti (piros) színében | **másképp** (D51, D52): az épület mellett lengő lobogó a csapatszínben, népenként saját alakkal, címer nélkül, a nép kiemelőszínű szegélyével; árnyéka a nappal forog. Egységeken: kulcsszín (tervben). Képek: `kepek/utan_csapatlobogo.jpg`, `kepek/lobogok_nepenkent.jpg` |
| 6 | Terep | földutak és fű lágy átmenetekkel, talajdíszek (virág, gomba, bokor, kő, ág) | egyszínű fűminta, díszek nélkül | – |
| 7 | Légkör | lebegő szentjánosbogarak, pollen, falevél (a pálya témája szerint) | nincs (csak az időjárás részecskéi) | – |
| 8 | Cipelt nyersanyag | kis zsák/köteg a munkáson a nyersanyag színében | nincs | **átvéve**: kis zsák sötét kontúrral a nyersanyag színében, a figura vállánál. Kép: `kepek/utan_cipelt_nyersanyag.jpg` |
| 9 | Talajjelek | ének / kapcsolt képesség, rejtőzés jele a figura alatt; hősnél talajjel és korona | nincs | **átvéve**: forgó rúnagyűrű (ének, kapcsoló), pöttyözött gyűrű (saját rejtőzött), a hős talajjele (fényudvar, kettős gyűrű, arany rovátkák) és jelvénye a feje fölött (a nép régi jelképe a csapatszínben, arany szegéllyel). Kép: `kepek/utan_hos_jelek.jpg` (az ének és a rejtőzés képen nincs ellenőrizve) |
| 10 | Küldetésjelek | célgyűrű és jelzőfény felirattal a pályán | csak a célok listája | – |
| 11 | Holttestek, tárgyak | holttestek (lélekidézőknél), a földön heverő tárgyak | a szimulációban megvannak, a pályán nem látszanak | – |
| 12 | Építés | lerakási rács, tervezett (sorba állított) épületek, munkahelyek, alapozás, tavacska | kísértetkép zöld/piros színnel | – |
| 13 | Minimap | terepkép, köd, pöttyök, kamerakeret | megvan; a két kép között nincs lényeges eltérés | – |

**Döntés (a tulajdonos, 2026-11-19):** a 12. tétel kivételével mindenhol a régi játék megoldása jön át, egyenkénti kérdés nélkül; a 12.-nél (építés) a különbséget képen kell megmutatni.

A régi játék szabálya csak akkor nyer, ha nem ütközik a tanulmánnyal; ütközésnél kérdés.
