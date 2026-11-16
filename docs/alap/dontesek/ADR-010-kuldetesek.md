# ADR-010: Küldetésrendszer és szerkesztő (M11)

**Státusz:** ELFOGADOTT – a tulajdonos döntése 2026-11-15 (D45)
**Érintett:** M11 (küldetések, szerkesztő), M12 (győzelmi feltételek, hadjárattérkép), M13 (modok: a küldetés modolható).
Alap: [KULDETESEK_TERV](../KULDETESEK_TERV.md), a tanulmány 14–15. fejezete, ADR-005 (adatformátum), ADR-007 (a
szimuláció és a hash), ADR-009 (hálózat: minden játékosbemenet parancs).

## Referenciák

- **Warcraft 3:** saját World Editor (terep, objektumok, régiók, kamerák, triggerek, objektumszerkesztő); a grafikus
  triggerből JASS-kód lesz; átvezető-mód (szélesvásznú sáv, a bemenet tiltva, a játék fut tovább).
- **Age of Empires 2:** játékba épített forgatókönyv-szerkesztő; tisztán adatalapú triggerek (feltétel + hatás).
- **0 A.D.:** saját szerkesztő (Atlas) a motorral; a triggerek szkriptek (JavaScript).

## Döntés

1. **Saját, játékba épített szerkesztő** a motor rajzolójával (D44): terep-ecset, objektumok, régiók, útvonalak,
   kamerapontok, triggerek, párbeszéd, célok; „kipróbálás innen”. A szerkesztő a küldetést változatlanul visszamenti
   (kanonikus JSON).
2. **Küldetéscsomag** (`content/<mod>/missions/<azonosító>/mission.json`): pálya (a meglévő pályaformátum),
   játékosok, névvel ellátott egységek, régiók, útvonalak, kamerapontok, változók, célok, triggerek, párbeszédek,
   átvezetők, opcionálisan egy Lua-szkript. A szövegek fordítási kulcsok (`.po`).
3. **Trigger-motor (adat):** a trigger = esemény + feltételek + akciók, `id`, `enabled`, `repeat`, `cooldown`,
   `priority`. A motor a szimuláción belül, determinisztikusan fut (`SimHooks`): az állapota (változók, célok,
   triggerek, időzítők, futó átvezetők és párbeszédek) a mentés és a hash része. Betöltéskor és a szerkesztőben
   ellenőrzött (ismeretlen típus, hiányzó hivatkozás, elérhetetlen cél hiba); tickenként futási korlát.
4. **Lua (homokozott):** a küldetés szkriptje ugyanazt a trigger-motort éri el (`Mission.*`), és saját triggert is
   regisztrálhat. Fájl-, hálózat- és rendszerhozzáférés nincs (M3).
5. **Párbeszéd:** csomópontok (beszélő, portré, szövegkulcs, következő csomópont vagy válaszok; a válasz változót
   állíthat). A játékos választása **parancs** (`Command::Kind::Mission`), így hálózaton és visszajátszásban is
   determinisztikus.
6. **Átvezető:** időzített lépések; a játékszabályt érintő lépés (egység mozgatása, megjelenése, felfedés) a
   szimulációban fut, a látvány (szélesvásznú sáv, kamera, felirat) a kliens dolga a pillanatképből. A bemenet az
   átvezető alatt tiltva, a kihagyás nem hagy ki kötelező játékszabály-akciót (a kihagyás azonnal lefuttatja őket).
7. **Célok és küldetésnapló:** elsődleges (mind kell a győzelemhez), másodlagos, rejtett; a győzelem és a bukás akció.
8. **Hadjáratállapot** (átvitt hősök, tárgyak, jelzők, döntések) a küldetéseken átívelő profilban – a hadjárat
   megvalósításakor.

## Események, feltételek, akciók (az első készlet)

- **Események:** `start`, `time` (tick, ismételve is), `enter_region` / `leave_region` (játékos vagy névvel adott
  egység), `unit_dies` (név vagy játékos + sablon), `count_le` / `count_ge` (játékos, sablon, darab),
  `building_done`, `research_done`, `dialogue_end`, `choice`, `objective_done`, `var_changed`, `timer`.
- **Feltételek:** `var` (összehasonlítás), `alive`, `in_region`, `count`, `objective`, `resource`, `and/or/not`.
- **Akciók:** `dialogue`, `objective` (megjelenít, teljesít, bukik), `spawn` (csoport, régióba), `order` (mozgás,
  támadó mozgás, járőr útvonalon), `kill`, `remove`, `set_owner`, `reveal` (régió), `resources`, `set_var`,
  `add_var`, `enable` / `disable` (trigger), `timer`, `cutscene`, `camera`, `message`, `win`, `lose`.
