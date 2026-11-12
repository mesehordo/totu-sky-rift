"""Tippek a főmenühöz és a betöltőképernyőhöz (véletlenszerűen; a főmenüben egyszerre legfeljebb 7)."""
import random

TIPS = [
    # irányítás
    "F8: a következő tétlen munkás kijelölése.",
    "Szóköz: ugrás a bázisra vagy az utolsó riasztáshoz.",
    "Shifttel több parancsot is sorba tehetsz.",
    "Ctrl+1…9: csoport mentése, 1…9: kijelölése.",
    "Dupla klikk egy egységen: az összes azonos típus.",
    "Jobb klikk egy erőforrásra: a munkás gyűjteni kezd.",
    "F1: súgó és az összes gyorsbillentyű.",
    "F2–F4: a hőseid kijelölése.",
    "A parancsrács betűi (W E R T…) gyorsbillentyűk.",
    "Középső egérgomb: a kamera húzása.",
    "+ és −: a játéksebesség állítása.",
    "F9: gyorsbetöltés, F5: gyorsmentés.",
    # gazdaság
    "A sok egység kevesebb aranyat hoz (Hadtáp).",
    "A tétlen munkás pénzt veszít: figyeld a jelzőt!",
    "A meteorit a különleges egységek és kutatások ára.",
    "Az Égkő-fényben a varázslók erősebbek.",
    "Az Égkő közelében a pajzsok gyorsabban töltődnek.",
    "A létszámkorlátnál nem képezhetsz új egységet.",
    # harc
    "Minden egységtípusnak van erőssége és gyengéje.",
    "A lovasság a lövészek ellen erős, a lándzsa ellen gyenge.",
    "Az ostromgépek az épületek ellen a legjobbak.",
    "A hős szintet lép, képességet tanul, tárgyat visel.",
    "Az elesett hős a főépületben újraéleszthető.",
    "A főépület gombjáról indul az Égi kegy csapása.",
    "Az Égi kegy csak látható pontra csaphat le.",
    "A Felemelkedéskor választott irány végleges.",
    "A Diplomácia csarnokában szövetséges népet választhatsz.",
    # népek
    "Szürke elfek: épületeik maguktól épülnek fel.",
    "Szürke elfek: a Napszentélynél fohászkodó munkás Áhítatot ad.",
    "Szürke elfek: a Napfúró magától is termel.",
    "Ursanok: a telepesek riadóra fegyvert ragadnak.",
    "Ursanok: a helyőrségbe húzódott munkás biztonságban van.",
    "Croftianok: a mélységen álló fák lassan elrothadnak.",
    "Croftianok: a szolga a Fertőzött tárnába olvadhat.",
    "Wullof: a csapolt fa hordás nélkül ad fát.",
    "Wullof: a tábor felszedhető és továbbvándorolhat.",
    "Vasbirodalom: alapozáson az épület teljes életerővel áll.",
    "Vasbirodalom: alapozás nélkül az épület lassan kopik.",
    "Vasbirodalom: a szobrok bónuszt adnak, amíg állnak.",
    "Huk’ucha: a saját tavacskán a békák gyorsabbak.",
    "Huk’ucha: az Ikrarakó csomóiból ebihalak kelnek ki.",
    "Huk’ucha: a Tóteremtő szentélynél énekelnek a Tóásók.",
    "Ooni: a lámpásfényben gyógyulnak az egységeid.",
    "Ooni: a Torii-kapupár között átkelhet a sereg.",
    "Ooni: a harci kedv elfogyhat – a dobos visszaadja.",
]


def pick(n=7, rng=None):
    """n különböző, véletlenszerű tipp."""
    rng = rng or random.Random()
    return rng.sample(TIPS, min(n, len(TIPS)))


def one(rng=None):
    return (rng or random.Random()).choice(TIPS)
