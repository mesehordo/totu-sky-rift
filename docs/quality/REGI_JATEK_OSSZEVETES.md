# A régi (Python) játék kinézete és az új motor – összevetés

Kelt: 2026-11-19. Forrás: a régi `rts/ui.py` rajzolói és két kép ugyanarról a helyzetről (Ursanok, a bázis az első
percben). A tételekről a tulajdonos **egyesével** dönt: átvesszük, módosítva vesszük át, vagy marad az új.

| # | Tétel | Régi játék | Új motor most | Döntés |
|---|---|---|---|---|
| 1 | Kijelölés jele | „pecsét” a figura alatt: lágy belső fény, sötét kontúrú kettős gyűrű, négy nyílhegy a tengelyeken, a csapat színében, a figura méretéhez igazítva (`sel_sigil`) | lapos zöld folt (24 × 10 képpont), minden egységnél ugyanakkora | – |
| 2 | Gyülekezőpont | lengő, fecskefarkú zászló a csapat színében, saját árnyékkal; az épülettől menetelő nyilak vezetnek oda, a végén kis pecsét (`draw_rally_flag`, `draw_rally_path`) | a pont megvan (parancs), de a pályán nem látszik | – |
| 3 | Mozgáscél jelzése | a kijelölt csoporttól a célig menetelő zöld nyilak, a célnál befelé mutató pecsét (`draw_move_indicator`) | nincs | – |
| 4 | Életerő-csík | 4 px; zöld > 60 % > sárga > 30 % > piros; fölötte kék pajzscsík, alatta lila energiacsík (saját egységnél), idézett lénynél „N mp” visszaszámlálás; az egér alatti egységnél is | 3 px, folytonos zöld → sárga → piros; csak kijelölve vagy sérülten; pajzs, energia, visszaszámlálás nincs | – |
| 5 | Csapatszín az épületeken | a lobogók, vásznak a játékos színében | a főépület vászna a kép eredeti (piros) színében | – |
| 6 | Terep | földutak és fű lágy átmenetekkel, talajdíszek (virág, gomba, bokor, kő, ág) | egyszínű fűminta, díszek nélkül | – |
| 7 | Légkör | lebegő szentjánosbogarak, pollen, falevél (a pálya témája szerint) | nincs (csak az időjárás részecskéi) | – |
| 8 | Cipelt nyersanyag | kis zsák/köteg a munkáson a nyersanyag színében | nincs | – |
| 9 | Talajjelek | ének / kapcsolt képesség, rejtőzés jele a figura alatt; hősnél talajjel és korona | nincs | – |
| 10 | Küldetésjelek | célgyűrű és jelzőfény felirattal a pályán | csak a célok listája | – |
| 11 | Holttestek, tárgyak | holttestek (lélekidézőknél), a földön heverő tárgyak | a szimulációban megvannak, a pályán nem látszanak | – |
| 12 | Építés | lerakási rács, tervezett (sorba állított) épületek, munkahelyek, alapozás, tavacska | kísértetkép zöld/piros színnel | – |
| 13 | Minimap | terepkép, köd, pöttyök, kamerakeret | megvan; a két kép között nincs lényeges eltérés | – |

A régi játék szabálya csak akkor nyer, ha nem ütközik a tanulmánnyal; ütközésnél kérdés.
