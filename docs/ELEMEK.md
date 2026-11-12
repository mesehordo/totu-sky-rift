# Elemi sebzés

A `tools/elemek.py` állítja elő (javítás: `data/elements.json`, „kezi” rész).

| Elem | Hatás | Időtartam | Erősség |
|---|---|---|---|
| Tűz | Égés | 3.0 mp | a találat 25%-a másodpercenként (2.0–25.0), páncélon át |
| Jég | Fagyás | 2.0 mp | 40% lassulás |
| Villám | Sokk | 0.4 mp | bénulás; egy célponton legfeljebb 3 mp-enként |
| Méreg | Mérgezés | 4.0 mp | a találat 15%-a másodpercenként (1.5–15.0), gépeken nem |
| Víz | Átázott | 5.0 mp | eloltja az égést; a villám kétszer hosszabb sokkot ad rá |
| Gyökér | Gúzsba kötve | 2.5 mp | nem mozdulhat, de támadhat |
| Álom | Alszik | 6.0 mp | az első sebzésre felébred |
| Rémület | Megrémült | 1.6 mp | 3 mezőnyit menekül az okozója elől |
| Vakítás | Elvakult | 4.0 mp | távolsági támadásainak 50%-a mellémegy |
| Szél | Hátralökés | – mp | a terület közepétől kifelé taszít |
| Zúzás | Páncéltörés | 6.0 mp | −2 páncél (épületen is) |
| Némítás | Elnémítva | 3.0 mp | nem varázsolhat |
| Jelölés | Megjelölt | 8.0 mp | +20% sebzést kap, a köd alól is látszik |

## Képességek

- **Tűz** (7): Napcsóva (`gp_napcsova`), Napvihar (`h_szelai_napvihar`), Meteorhívás (`meteorhivas`), Napkitörés (`napkitores`), Napvihar (`napvihar`), Tűzfal (`tuz_tuzfal`), Tűzköpés (`tuzkopes`)
- **Jég** (8): Fagyköpés (`fagykopes`), Megfagyott idő (`h_hja_fagyott_ido`), Jégszilánk (`h_hja_szilank`), Hóvihar (`h_hja_vihar`), Hosszú tél (`h_ragna_hosszutel`), Jeges hullám (`pin_jeghullam`), Fagydobbantás (`tok_fagydobbantas`), Hóvihar (`tok_hovihar`)
- **Villám** (4): Ordan villáma (`gp_ordan_villama`), Tengu-villám (`h_kar_villam`), Villámcsapás (`oon_villamcsapas`), Villámcsapás (`villamcsapas`)
- **Méreg** (9): Csápcsapás (`h_keth_csap`), Bomlasztó tüske (`h_spo_tuske`), Spórafelhő (`h_ssa_felho`), Savsugár (`h_vor_sugar`), Savköpés (`savkopes`), Savtócsa (`savtocsa`), Spóraeső (`sporaeso`), Spórarobbanás (`sporarobbanas`), Spóratüske (`sporatuske`)
- **Víz** (6): Árhullám (`gp_arhullam`), Árasztás (`h_kva_arasztas`), Özönvíz (`h_kva_ozonviz`), Árhullám (`huk_arhullam`), Árhullám (`nii_arhullam`), Vízörvény (`nil_orveny`)
- **Gyökér** (7): Gyökérgáncs (`gal_gyokergancs`), Gyökérkötés (`gyokerkotes`), Gyökérháló (`h_gya_halo`), Gyökérrengés (`h_gya_rengés`), Gyökérkötés (`h_mor_gyokerkotes`), Fonalháló (`mic_fonalhalo`), Gyökérdobbantás (`ten_gyokerdobbantas`)
- **Álom** (1): Álomspóra (`alomspora`)
- **Rémület** (3): Nian-üvöltés (`h_shi_uvoltes`), Medveüvöltés (`medveuvoltes`), Tollkorona-üvöltés (`tollkorona_uvoltes`)
- **Vakítás** (1): Mészrobbanás (`gho_meszrobbanas`)
- **Szél** (5): Isteni szél (`gp_isteni_szel`), Szélörvény (`h_kar_szel`), Tájfun (`h_kar_tajfun`), Szélvihar (`oon_szelvihar`), Levélörvény (`ten_levelorveny`)
- **Zúzás** (3): Földrengetés (`h_aka_rengetes`), Kitinrobbanás (`h_vor_robbanas`), Gőzkitörés (`mho_gozkitores`)
- **Némítás** (3): Leválasztás (`h_szelai_levalasztas`), Ősök köde (`h_var_kod`), Robbanó rúna (`shr_robbanorunak`)
- **Jelölés** (4): Csillagdárda (`csillagdarda`), Vadászszigony (`h_bja_szigony`), Célzott lövés (`h_nera_loves`), Nyomkereső nyíl (`h_tava_nyil`)

## Egységek fegyvere

- **Tűz** (4): Napszóró pap (`elf_napszoro`), Szikraőr (`mho_szikraor`), Hőszövő (`tuz_hoszovo`), Parázsíjász (`tuz_parazsijasz`)
- **Jég** (6): Ár-apály sámán (`pin_arapaly_saman`), Fagyóriás (`tok_fagyorias`), Hóvihar-busó (`tok_hovihar_buso`), Jégvető törpe (`tok_jegveto_turpe`), Zúzmaratörpe (`tok_zuzmaraturpe`), Hjalmar Fagyjós (`urs_hos_hjalmar`)
- **Villám** (3): Karasu, a tengu bölcs (`oon_hos_karasu`), Viharhívó tengu (`oon_viharhivo`), Viharkakadu (`wul_mennydorgesmadar`)
- **Méreg** (16): Keth (`cro_hos_keth`), Ssarha, a Raj Hangja (`cro_hos_ssarha`), Ostromtaréj (`cro_ostromtaraj`), Savköpő (`cro_savkopo`), Szárnyas rém (`cro_szarnyas_rem`), Tüskeköpő (`cro_tuskekopo`), Fullánclándzsás (`dar_fullanclandzsas`), Spóraköpő (`gal_sporakopo`), Tüskegomba-lövész (`mic_tuskegomba_lovesz`), Sporin, a Bomlasztó (`myc_hos_sporin`), Lebegő pöfeteg (`myc_lebego_pofeteg`), Óriáspöfeteg (`myc_oriaspofeteg`), Pöfeteg (`myc_pofeteg`), Rothasztó (`myc_rothaszto`), Tüskegomba-lövész (`myc_tuskegomba_lovesz`), Nága szövetséges (`nil_naga`)
- **Víz** (3): Áradás papja (`huk_aradas_papja`), Árapálypap (`nii_arapalypap`), Makara-idéző (`nil_makara_idezo`)
- **Zúzás** (8): Sziklagörgető (`gho_sziklagorgeto`), Teknőshátas mozsár (`huk_teknos_mozsar`), Onager-mecha (`imp_onager`), Akahana, a Vörös Buzogány (`oon_hos_akahana`), Kőhajító oni (`oon_kohajito`), Ostromszán (`urs_ostromszan`), Rúnaóriás (`urs_runaorias`), Termeszvár-óriás (`wul_totemorias`)
- **Jelölés** (2): Vadászkarmos (`cro_vadaszkarmos`), Jaguár-orvlövész (`elf_jaguar`)
