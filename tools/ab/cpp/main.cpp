// F3 A/B mérés – C++20 mag. A tools/ab/python/abcore.py sorról sorra megfelelő átirata (lásd tools/ab/SPEC.md).
// Fordítás: cmake -S tools/ab/cpp -B tools/ab/cpp/build -G "MinGW Makefiles" && cmake --build tools/ab/cpp/build
// Futtatás: tools/ab/cpp/build/abcore.exe tools/ab/scenarios/center_500.json [--ticks N] [--out f.json]
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

// ---- minimális JSON-olvasó (csak a forgatókönyvfájlhoz) -------------------------------------------------------
struct J {
    enum T { NUL, NUM, BOOL, STR, ARR, OBJ } t = NUL;
    double num = 0; bool b = false; std::string str;
    std::vector<J> arr; std::vector<std::pair<std::string, J>> obj;
    const J& operator[](const char* k) const {
        for (auto& p : obj) if (p.first == k) return p.second;
        std::fprintf(stderr, "hiányzó kulcs: %s\n", k); std::exit(2);
    }
    const J& operator[](size_t i) const { return arr[i]; }
};
struct JP {
    const std::string& s; size_t i = 0;
    explicit JP(const std::string& src) : s(src) {}
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) i++; }
    J val() {
        ws(); J j;
        char c = s[i];
        if (c == '{') {
            j.t = J::OBJ; i++; ws();
            if (s[i] == '}') { i++; return j; }
            while (true) { ws(); std::string k = string(); ws(); i++; J v = val(); j.obj.emplace_back(k, std::move(v)); ws(); if (s[i] == ',') { i++; continue; } i++; break; }
        } else if (c == '[') {
            j.t = J::ARR; i++; ws();
            if (s[i] == ']') { i++; return j; }
            while (true) { j.arr.push_back(val()); ws(); if (s[i] == ',') { i++; continue; } i++; break; }
        } else if (c == '"') { j.t = J::STR; j.str = string(); }
        else if (c == 't') { j.t = J::BOOL; j.b = true; i += 4; }
        else if (c == 'f') { j.t = J::BOOL; j.b = false; i += 5; }
        else if (c == 'n') { i += 4; }
        else { j.t = J::NUM; size_t e = i; while (e < s.size() && std::strchr("+-0123456789.eE", s[e])) e++; j.num = std::strtod(s.substr(i, e - i).c_str(), nullptr); i = e; }
        return j;
    }
    std::string string() {
        std::string o; i++;
        while (s[i] != '"') {
            if (s[i] == '\\') {
                i++;
                if (s[i] == 'u') {  // \uXXXX → UTF-8
                    unsigned cp = std::strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16); i += 5;
                    if (cp < 0x80) o += char(cp);
                    else if (cp < 0x800) { o += char(0xC0 | (cp >> 6)); o += char(0x80 | (cp & 0x3F)); }
                    else { o += char(0xE0 | (cp >> 12)); o += char(0x80 | ((cp >> 6) & 0x3F)); o += char(0x80 | (cp & 0x3F)); }
                    continue;
                }
                o += s[i++];
            } else o += s[i++];
        }
        i++;
        return o;
    }
};
static std::vector<uint8_t> b64(const std::string& in) {
    std::vector<uint8_t> out; int val = 0, bits = -8;
    for (unsigned char c : in) {
        int d;
        if (c >= 'A' && c <= 'Z') d = c - 'A'; else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52; else if (c == '+') d = 62; else if (c == '/') d = 63; else continue;
        val = (val << 6) + d; bits += 6;
        if (bits >= 0) { out.push_back(uint8_t((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}

// ---- mag ----------------------------------------------------------------------------------------------------
static const double TILE = 32.0;
static const double SQ2 = std::sqrt(2.0);
static const int DXS[8] = {1, -1, 0, 0, 1, 1, -1, -1};
static const int DYS[8] = {0, 0, 1, -1, 1, -1, 1, -1};
static const double CELL = 128.0;
static const int PATH_BUDGET = 6, MAX_NODES = 9000, OCC_NODES = 512, FOW_EVERY = 6, HASH_EVERY = 300;
static inline int F(double v) { return (int)std::floor(v); }
static inline double Dist(double ax, double ay, double bx, double by) { double dx = ax - bx, dy = ay - by; return std::sqrt(dx * dx + dy * dy); }

struct UType { double radius, speed, hp, armor, damage, range, cooldown, proj_speed, splash, sight; bool projectile; int ut; };
struct Pt { double x, y; };
struct Unit {
    int id, team, t; double x, y, hp, r;
    double cd = 0, stuck_t = 0, lpx = 0, lpy = 0, wait_t = 0, scan_t = 0, repath_t = 0;
    std::vector<Pt> path; bool has_goal = false; double gx = 0, gy = 0;
    bool pending = false, queued = false, started = false, done = false, alive = true, moving = false;
    int repaths = 0; Unit* sub = nullptr;
};
struct Proj { double x, y, tx, ty, dmg, speed, splash; Unit* tgt; int team, ut; bool alive = true; };
struct Occ { double x, y, rr; };

struct Heap {
    struct E { double f, g; int n; };
    std::vector<E> a; int n = 0;
    static bool less(const E& x, const E& y) { return x.f < y.f || (x.f == y.f && (x.g < y.g || (x.g == y.g && x.n < y.n))); }
    void clear() { n = 0; }
    void push(double f, double g, int i) {
        if (n == (int)a.size()) a.push_back({f, g, i}); else a[n] = {f, g, i};
        int k = n++;
        while (k > 0) { int p = (k - 1) >> 1; if (less(a[k], a[p])) { std::swap(a[k], a[p]); k = p; } else break; }
    }
    E pop() {
        E top = a[0]; n--;
        if (n > 0) {
            a[0] = a[n]; int k = 0;
            while (true) {
                int l = 2 * k + 1, r = 2 * k + 2, m = k;
                if (l < n && less(a[l], a[m])) m = l;
                if (r < n && less(a[r], a[m])) m = r;
                if (m == k) break;
                std::swap(a[k], a[m]); k = m;
            }
        }
        return top;
    }
};

struct Sim {
    int w, h, tick = 0, cw, ch, gen = 0; size_t qhead = 0;
    std::vector<uint8_t> block;
    double dt, destx, desty, max_r = 0;
    std::vector<UType> types; std::vector<std::vector<double>> matrix;
    std::vector<std::unique_ptr<Unit>> pool; std::vector<Unit*> units; std::vector<Proj> projs; std::vector<Unit*> queue;
    std::vector<std::vector<Unit*>> cells;
    std::map<double, std::vector<uint8_t>> clear;
    std::vector<uint8_t> vis[8];
    std::vector<double> gcost; std::vector<int> came, stamp, memo_stamp; std::vector<uint8_t> memo_val;
    Heap heap;
    long long path_requests = 0, astar_calls = 0, astar_expanded = 0, kills = 0, arrived = 0;

    explicit Sim(const J& sc) {
        w = (int)sc["w"].num; h = (int)sc["h"].num;
        block = b64(sc["block"].str);
        dt = 1.0 / sc["hz"].num;
        for (auto& t : sc["types"].arr)
            types.push_back({t["radius"].num, t["speed"].num, t["hp"].num, t["armor"].num, t["damage"].num, t["range"].num,
                             t["cooldown"].num, t["proj_speed"].num, t["splash"].num, t["sight"].num, t["projectile"].b, (int)t["utype"].num});
        for (auto& row : sc["matrix"].arr) { std::vector<double> r; for (auto& v : row.arr) r.push_back(v.num); matrix.push_back(r); }
        destx = sc["dest"].arr[0].num; desty = sc["dest"].arr[1].num;
        int i = 0;
        for (auto& u : sc["units"].arr) {
            int t = (int)u["type"].num;
            auto un = std::make_unique<Unit>();
            un->id = ++i; un->team = (int)u["team"].num; un->t = t; un->x = u["x"].num; un->y = u["y"].num;
            un->hp = types[t].hp; un->r = types[t].radius; un->lpx = un->x; un->lpy = un->y; un->scan_t = (un->id % 8) * 0.05;
            units.push_back(un.get()); pool.push_back(std::move(un));
        }
        for (auto& t : types) max_r = std::max(max_r, t.radius);
        cw = F(w * TILE / CELL) + 1; ch = F(h * TILE / CELL) + 1;
        cells.resize(cw * ch);
        for (auto& v : vis) v.assign(w * h, 0);
        int n = w * h;
        gcost.assign(n, 0.0); came.assign(n, 0); stamp.assign(n, 0); memo_stamp.assign(n, 0); memo_val.assign(n, 0);
        update_fow();
    }

    bool passable(int tx, int ty) const { return tx >= 0 && tx < w && ty >= 0 && ty < h && block[ty * w + tx] == 0; }

    bool fits(double x, double y, double r) const {
        if (x - r < 0 || y - r < 0 || x + r > w * TILE || y + r > h * TILE) return false;
        int ty0 = F((y - r) / TILE), ty1 = F((y + r) / TILE), tx0 = F((x - r) / TILE), tx1 = F((x + r) / TILE);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) {
                if (passable(tx, ty)) continue;
                double dx = std::max(std::max(tx * TILE - x, 0.0), x - (tx + 1) * TILE);
                double dy = std::max(std::max(ty * TILE - y, 0.0), y - (ty + 1) * TILE);
                if (dx * dx + dy * dy < r * r) return false;
            }
        return true;
    }

    bool tile_ok(int tx, int ty, double r) {
        auto it = clear.find(r);
        if (it == clear.end()) it = clear.emplace(r, std::vector<uint8_t>(w * h, 2)).first;
        auto& grid = it->second; int i = ty * w + tx; uint8_t v = grid[i];
        if (v == 2) { v = fits((tx + 0.5) * TILE, (ty + 0.5) * TILE, r) ? 1 : 0; grid[i] = v; }
        return v == 1;
    }

    static bool occupied(const std::vector<Occ>* occ, double x, double y) {
        for (auto& o : *occ) { double dx = x - o.x, dy = y - o.y; if (dx * dx + dy * dy < o.rr) return true; }
        return false;
    }

    bool clear_line(double x0, double y0, double x1, double y1, double r, const std::vector<Occ>* occ) const {
        double dx = x1 - x0, dy = y1 - y0, d = std::sqrt(dx * dx + dy * dy);
        if (d < 1.0) return true;
        int steps = F(d / 4.0) + 1;
        for (int i = 0; i <= steps; i++) {
            double t = (double)i / steps, px = x0 + dx * t, py = y0 + dy * t;
            if (!fits(px, py, r) || (occ && occupied(occ, px, py))) return false;
        }
        return true;
    }

    bool nearest_free(int gx, int gy, int sx, int sy, int& ox, int& oy) const {
        for (int ring = 1; ring < 13; ring++) {
            bool found = false; int bd = 0;
            for (int ty = gy - ring; ty <= gy + ring; ty++)
                for (int tx = gx - ring; tx <= gx + ring; tx++) {
                    if (std::max(std::abs(tx - gx), std::abs(ty - gy)) != ring || !passable(tx, ty)) continue;
                    int d = (tx - sx) * (tx - sx) + (ty - sy) * (ty - sy);
                    if (!found || d < bd) { found = true; bd = d; ox = tx; oy = ty; }
                }
            if (found) return true;
        }
        return false;
    }

    std::vector<std::pair<int, int>> find_path(int sx, int sy, int gx, int gy, double r, const std::vector<Occ>* occ, int max_nodes) {
        std::vector<std::pair<int, int>> out;
        astar_calls++;
        if (!passable(gx, gy)) { int ox, oy; if (!nearest_free(gx, gy, sx, sy, ox, oy)) return out; gx = ox; gy = oy; }
        int start = sy * w + sx, goal = gy * w + gx;
        if (start == goal) return out;
        int lgen = ++gen;
        auto valid = [&](int x, int y) {
            int k = y * w + x;
            if (memo_stamp[k] == lgen) return memo_val[k] == 1;
            bool v = tile_ok(x, y, r);
            if (v && occ) v = !occupied(occ, (x + 0.5) * TILE, (y + 0.5) * TILE);
            memo_stamp[k] = lgen; memo_val[k] = v ? 1 : 0;
            return v;
        };
        auto hd = [&](int x, int y) { int dx = std::abs(x - gx), dy = std::abs(y - gy); return (double)(dx + dy) + (SQ2 - 2.0) * std::min(dx, dy); };
        heap.clear();
        stamp[start] = lgen; gcost[start] = 0.0; came[start] = -1;
        heap.push(hd(sx, sy), 0.0, start);
        int best = start; double besth = hd(sx, sy); int expanded = 0;
        while (heap.n && expanded < max_nodes) {
            Heap::E e = heap.pop(); double g = e.g; int n = e.n;
            if (n == goal) { best = goal; break; }
            if (g > gcost[n]) continue;
            expanded++;
            int x = n % w, y = n / w;
            for (int d = 0; d < 8; d++) {
                int dx = DXS[d], dy = DYS[d], nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                if (!valid(nx, ny)) continue;
                if (dx != 0 && dy != 0 && (!valid(nx, y) || !valid(x, ny))) continue;
                int ni = ny * w + nx; double ng = g + (d < 4 ? 1.0 : SQ2);
                if (stamp[ni] != lgen || ng < gcost[ni]) {
                    stamp[ni] = lgen; gcost[ni] = ng; came[ni] = n;
                    double hh = hd(nx, ny);
                    heap.push(ng + hh, ng, ni);
                    if (hh < besth) { best = ni; besth = hh; }
                }
            }
        }
        astar_expanded += expanded;
        int m = best;
        while (m != start && m != -1) { out.emplace_back(m % w, m / w); m = came[m]; }
        std::reverse(out.begin(), out.end());
        return out;
    }

    std::vector<Pt> smooth(double sx, double sy, const std::vector<Pt>& pts, double r, const std::vector<Occ>* occ) const {
        std::vector<Pt> o; double cx = sx, cy = sy; int i = 0, n = (int)pts.size();
        while (i < n) {
            int j = std::min(n - 1, i + 6);
            while (j > i && !clear_line(cx, cy, pts[j].x, pts[j].y, r, occ)) j--;
            o.push_back(pts[j]); cx = pts[j].x; cy = pts[j].y; i = j + 1;
        }
        return o;
    }

    static std::vector<Pt> centers(const std::vector<std::pair<int, int>>& tiles) {
        std::vector<Pt> p; p.reserve(tiles.size());
        for (auto& t : tiles) p.push_back({(t.first + 0.5) * TILE, (t.second + 0.5) * TILE});
        return p;
    }

    std::vector<Pt> unit_path(Unit* u) {
        double gx = u->gx, gy = u->gy, r = u->r;
        int sx = F(u->x / TILE), sy = F(u->y / TILE), tx = F(gx / TILE), ty = F(gy / TILE);
        if (u->repaths == 0 && clear_line(u->x, u->y, gx, gy, r, nullptr)) return {{gx, gy}};
        std::vector<Occ> occv; bool has_occ = false;
        if (u->repaths > 0) {
            double reach = TILE * 4;
            for (Unit* v : query(u->x, u->y, reach))
                if (v != u && v->alive && Dist(v->x, v->y, u->x, u->y) < reach) { double rr = r + v->r - 0.01; occv.push_back({v->x, v->y, rr * rr}); }
            if (!occv.empty()) {
                has_occ = true;
                auto tiles = find_path(sx, sy, tx, ty, r, &occv, OCC_NODES);
                if (!tiles.empty() && tiles.back() == std::make_pair(tx, ty)) {
                    auto pts = centers(tiles); Pt last = pts.back();
                    if (clear_line(last.x, last.y, gx, gy, r, &occv)) {
                        if (!(last.x == gx && last.y == gy)) pts.push_back({gx, gy});
                        return smooth(u->x, u->y, pts, r, &occv);
                    }
                }
            }
        }
        if (!has_occ && clear_line(u->x, u->y, gx, gy, r, nullptr)) return {{gx, gy}};
        auto t2 = find_path(sx, sy, tx, ty, r, nullptr, MAX_NODES);
        auto p2 = centers(t2);
        if (!p2.empty() && !(p2.back().x == gx && p2.back().y == gy) && passable(tx, ty) && clear_line(p2.back().x, p2.back().y, gx, gy, r, nullptr))
            p2.push_back({gx, gy});
        else if (p2.empty() && clear_line(u->x, u->y, gx, gy, r, nullptr)) return {{gx, gy}};
        return smooth(u->x, u->y, p2, r, nullptr);
    }

    void rebuild() {
        for (auto& c : cells) c.clear();
        for (Unit* u : units) if (u->alive) cells[cell_of(u->x, u->y)].push_back(u);
    }
    int cell_of(double x, double y) const {
        int cx = std::min(cw - 1, std::max(0, F(x / CELL))), cy = std::min(ch - 1, std::max(0, F(y / CELL)));
        return cy * cw + cx;
    }
    std::vector<Unit*> query(double x, double y, double r) const {
        std::vector<Unit*> o;
        int cx0 = std::max(0, F((x - r) / CELL)), cx1 = std::min(cw - 1, F((x + r) / CELL));
        int cy0 = std::max(0, F((y - r) / CELL)), cy1 = std::min(ch - 1, F((y + r) / CELL));
        for (int cy = cy0; cy <= cy1; cy++)
            for (int cx = cx0; cx <= cx1; cx++) { auto& c = cells[cy * cw + cx]; o.insert(o.end(), c.begin(), c.end()); }
        return o;
    }

    void set_path(Unit* u, double gx, double gy) {
        u->has_goal = true; u->gx = gx; u->gy = gy; u->stuck_t = 0.0; u->lpx = u->x; u->lpy = u->y;
        u->path.clear(); u->pending = true;
        if (!u->queued) { u->queued = true; queue.push_back(u); path_requests++; }
    }

    void process_paths() {
        int budget = PATH_BUDGET;
        while (qhead < queue.size() && budget > 0) {
            Unit* u = queue[qhead++]; u->queued = false;
            if (!u->alive || !u->pending || !u->has_goal) { u->pending = false; continue; }
            budget -= u->repaths ? 3 : 1;
            u->path = unit_path(u); u->pending = false;
        }
        if (qhead > 4096) { queue.erase(queue.begin(), queue.begin() + qhead); qhead = 0; }
    }

    Unit* blocker(Unit* u, double nx, double ny) const {
        for (Unit* v : query(nx, ny, u->r + max_r)) {
            if (v == u || !v->alive) continue;
            double m = u->r + v->r - 0.5, dn = Dist(nx, ny, v->x, v->y);
            if (dn < m && dn < Dist(u->x, u->y, v->x, v->y)) return v;
        }
        return nullptr;
    }

    bool follow(Unit* u) {
        if (u->pending) return false;
        if (u->path.empty()) return true;
        double px = u->path[0].x, py = u->path[0].y, dx = px - u->x, dy = py - u->y, d = std::sqrt(dx * dx + dy * dy);
        double sp = types[u->t].speed, step = sp * dt, nx, ny;
        if (d <= step) { nx = px; ny = py; } else { nx = u->x + dx / d * step; ny = u->y + dy / d * step; }
        bool same = F(nx / TILE) == F(u->x / TILE) && F(ny / TILE) == F(u->y / TILE);
        if (!fits(nx, ny, u->r) && (!same || fits(u->x, u->y, u->r))) {
            u->repaths = std::min(8, u->repaths + 1);
            double gx = u->has_goal ? u->gx : u->path.back().x, gy = u->has_goal ? u->gy : u->path.back().y;
            set_path(u, gx, gy); return false;
        }
        if (blocker(u, nx, ny)) u->wait_t += dt;
        else if (d <= step) { u->x = px; u->y = py; u->path.erase(u->path.begin()); u->moving = true; u->wait_t = std::max(0.0, u->wait_t - dt * 0.5); }
        else { u->x = nx; u->y = ny; u->moving = true; u->wait_t = std::max(0.0, u->wait_t - dt * 0.5); }
        u->stuck_t += dt;
        if (u->stuck_t >= 0.8 + std::min(4, u->repaths) * 0.15) {
            double ox = u->lpx, oy = u->lpy;
            u->lpx = u->x; u->lpy = u->y; u->stuck_t = 0.0;
            double moved = Dist(u->x, u->y, ox, oy), progress = Dist(px, py, ox, oy) - Dist(px, py, u->x, u->y);
            if (!u->path.empty() && (moved < sp * 0.15 || (u->wait_t > 0.6 && progress < sp * 0.12))) {
                u->repaths = std::min(8, u->repaths + 1);
                double gx = u->has_goal ? u->gx : u->path.back().x, gy = u->has_goal ? u->gy : u->path.back().y;
                set_path(u, gx, gy);
            } else if (moved >= sp * 0.15) u->repaths = 0;
        }
        return u->path.empty() && !u->pending;
    }

    static double gap(const Unit* a, const Unit* b) { return std::max(0.0, Dist(a->x, a->y, b->x, b->y) - a->r - b->r); }
    bool visible(int team, const Unit* e) const {
        int tx = F(e->x / TILE), ty = F(e->y / TILE);
        return tx >= 0 && tx < w && ty >= 0 && ty < h && vis[team][ty * w + tx] == 1;
    }
    Unit* find_enemy(Unit* u, double radius) const {
        Unit* best = nullptr; double bestv = 0.0;
        for (Unit* e : query(u->x, u->y, radius + 32.0)) {
            if (e->team == u->team || !e->alive || !visible(u->team, e)) continue;
            double d = gap(u, e);
            if (d > radius) continue;
            if (!best || d < bestv) { best = e; bestv = d; }
        }
        return best;
    }
    void apply(int, int ut, Unit* tgt, double dmg) {
        if (!tgt->alive) return;
        int tt = types[tgt->t].ut;
        double m = (ut >= 0 && tt >= 0) ? matrix[ut][tt] : 1.0;
        double d = std::max(0.5, dmg * m - types[tgt->t].armor);
        tgt->hp -= d;
        if (tgt->hp <= 0.0) { tgt->alive = false; kills++; }
    }
    void hit(int team, int ut, Unit* tgt, double dmg, double splash, double x, double y) {
        if (tgt) apply(team, ut, tgt, dmg);
        if (splash > 0.0)
            for (Unit* e : query(x, y, splash + max_r)) {
                if (e == tgt || !e->alive || e->team == team) continue;
                if (Dist(e->x, e->y, x, y) - e->r <= splash) apply(team, ut, e, dmg * 0.5);
            }
    }
    void try_attack(Unit* u, Unit* tgt) {
        if (u->cd > 0.0) return;
        const UType& ty = types[u->t];
        u->cd = ty.cooldown;
        if (ty.projectile) projs.push_back({u->x, u->y, tgt->x, tgt->y, ty.damage, ty.proj_speed, ty.splash, tgt, u->team, ty.ut, true});
        else hit(u->team, ty.ut, tgt, ty.damage, ty.splash, tgt->x, tgt->y);
    }
    void engage(Unit* u, Unit* tgt) {
        const UType& ty = types[u->t];
        if (gap(u, tgt) <= ty.range + 3.0) { u->path.clear(); u->pending = false; try_attack(u, tgt); return; }
        u->repath_t -= dt;
        if ((u->path.empty() && !u->pending) || u->repath_t <= 0.0)
            if (u->repath_t <= 0.0 || u->path.empty()) { u->repath_t = 0.8; set_path(u, tgt->x, tgt->y); }
        follow(u);
    }
    void update_unit(Unit* u) {
        const UType& ty = types[u->t];
        if (u->cd > 0.0) u->cd -= dt;
        u->moving = false;
        Unit* sub = u->sub;
        if (sub && (!sub->alive || gap(u, sub) > ty.sight * 1.6)) { sub = u->sub = nullptr; u->started = false; }
        if (!sub) {
            u->scan_t -= dt;
            if (u->scan_t <= 0.0) { u->scan_t = 0.4; Unit* e = find_enemy(u, ty.sight); if (e) { u->sub = e; return; } }
            if (u->done) return;
            if (!u->started || !u->has_goal) { u->started = true; set_path(u, destx, desty); return; }
            if (follow(u)) { u->done = true; arrived++; }
            return;
        }
        engage(u, sub);
    }
    void update_projs() {
        for (auto& p : projs) {
            if (p.tgt->alive) { p.tx = p.tgt->x; p.ty = p.tgt->y; }
            double dx = p.tx - p.x, dy = p.ty - p.y, d = std::sqrt(dx * dx + dy * dy), step = p.speed * dt;
            if (d <= step) { p.alive = false; hit(p.team, p.ut, p.tgt->alive ? p.tgt : nullptr, p.dmg, p.splash, p.tx, p.ty); }
            else { p.x += dx / d * step; p.y += dy / d * step; }
        }
    }
    void separate() {
        double WW = w * TILE, HH = h * TILE; std::vector<Unit*> nearby;
        for (int ci = 0; ci < cw * ch; ci++) {
            auto& lst = cells[ci];
            if (lst.empty()) continue;
            int cx = ci % cw, cy = ci / cw;
            nearby.clear();
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int x2 = cx + dx, y2 = cy + dy;
                    if (x2 >= 0 && x2 < cw && y2 >= 0 && y2 < ch) { auto& c = cells[y2 * cw + x2]; nearby.insert(nearby.end(), c.begin(), c.end()); }
                }
            for (Unit* u : lst) {
                if (!u->alive) continue;
                for (Unit* v : nearby) {
                    if (v->id <= u->id || !v->alive) continue;
                    double ddx = v->x - u->x, ddy = v->y - u->y, m = u->r + v->r;
                    if (std::abs(ddx) >= m || std::abs(ddy) >= m) continue;
                    double d = std::sqrt(ddx * ddx + ddy * ddy);
                    if (d >= m) continue;
                    if (d < 0.01) { if ((u->id + v->id) % 2 == 0) { ddx = 1.0; ddy = 0.0; } else { ddx = 0.0; ddy = 1.0; } d = 1.0; }
                    double ov = (m - d) * 0.5, ku = u->moving ? 1.0 : 0.0, kv = v->moving ? 1.0 : 0.0;
                    if (ku + kv == 0.0) { ku = 1.0; kv = 1.0; }
                    double wu = ku / (ku + kv), wv = kv / (ku + kv), nx = ddx / d, ny = ddy / d;
                    double ux = u->x - nx * ov * wu, uy = u->y - ny * ov * wu, vx = v->x + nx * ov * wv, vy = v->y + ny * ov * wv;
                    if (u->r <= ux && ux <= WW - u->r && u->r <= uy && uy <= HH - u->r && fits(ux, uy, u->r)) { u->x = ux; u->y = uy; }
                    if (v->r <= vx && vx <= WW - v->r && v->r <= vy && vy <= HH - v->r && fits(vx, vy, v->r)) { v->x = vx; v->y = vy; }
                }
            }
        }
    }
    void update_fow() {
        for (auto& v : vis) std::fill(v.begin(), v.end(), 0);
        for (Unit* u : units) {
            if (!u->alive) continue;
            double s = types[u->t].sight, ss = s * s; auto& v = vis[u->team];
            int tx0 = std::max(0, F((u->x - s) / TILE)), tx1 = std::min(w - 1, F((u->x + s) / TILE));
            int ty0 = std::max(0, F((u->y - s) / TILE)), ty1 = std::min(h - 1, F((u->y + s) / TILE));
            for (int ty = ty0; ty <= ty1; ty++) {
                double dy = (ty + 0.5) * TILE - u->y;
                for (int tx = tx0; tx <= tx1; tx++) { double dx = (tx + 0.5) * TILE - u->x; if (dx * dx + dy * dy <= ss) v[ty * w + tx] = 1; }
            }
        }
    }
    void step() {
        tick++;
        rebuild();
        process_paths();
        for (Unit* u : units) if (u->alive) update_unit(u);
        update_projs();
        separate();
        units.erase(std::remove_if(units.begin(), units.end(), [](Unit* u) { return !u->alive; }), units.end());
        projs.erase(std::remove_if(projs.begin(), projs.end(), [](const Proj& p) { return !p.alive; }), projs.end());
        if (tick % FOW_EVERY == 0) update_fow();
    }
    uint64_t state_hash() const {
        uint64_t hsh = 1469598103934665603ULL;
        for (Unit* u : units) {
            long long vals[4] = {u->id, (long long)std::floor(u->x * 16.0), (long long)std::floor(u->y * 16.0), (long long)std::floor(u->hp * 16.0)};
            for (long long v : vals) hsh = (hsh ^ (uint64_t)v) * 1099511628211ULL;
        }
        return hsh;
    }
};

static double pct(std::vector<double> v, double p) {
    std::sort(v.begin(), v.end());
    long i = (long)std::nearbyint(p / 100.0 * (double)(v.size() - 1));
    i = std::max(0L, std::min((long)v.size() - 1, i));
    return std::nearbyint(v[i] * 1000.0) / 1000.0;
}

int main(int argc, char** argv) {
    std::string path = argv[1], outp; int ticks = -1;
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "--ticks") ticks = std::atoi(argv[i + 1]);
        if (std::string(argv[i]) == "--out") outp = argv[i + 1];
    }
    std::ifstream f(path, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); std::string src = ss.str();
    JP jp(src); J sc = jp.val();
    Sim sim(sc);
    int n = ticks > 0 ? ticks : (int)sc["ticks"].num;
    size_t start_units = sim.units.size();
    std::vector<double> times; times.reserve(n);
    std::string hashes;
    auto t_all = std::chrono::steady_clock::now();
    for (int k = 0; k < n; k++) {
        auto t0 = std::chrono::steady_clock::now();
        sim.step();
        times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        if (sim.tick % HASH_EVERY == 0) {
            char buf[64]; std::snprintf(buf, sizeof buf, "%s[%d,\"%016llx\"]", hashes.empty() ? "" : ",", sim.tick, (unsigned long long)sim.state_hash());
            hashes += buf;
        }
    }
    double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t_all).count();
    double mean = 0, mx = 0; for (double t : times) { mean += t; mx = std::max(mx, t); } mean /= times.size();
    double peak = 0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc; if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) peak = pmc.PeakWorkingSetSize / 1048576.0;
#endif
    char buf[1024];
    std::snprintf(buf, sizeof buf,
        "{\"lang\":\"cpp\",\"version\":\"gcc %d.%d\",\"scenario\":\"%s\",\"units_start\":%zu,\"ticks\":%d,\"total_s\":%.3f,"
        "\"tick_ms\":{\"p50\":%.3f,\"p95\":%.3f,\"p99\":%.3f,\"max\":%.3f,\"mean\":%.3f},"
        "\"alive_end\":%zu,\"path_requests\":%lld,\"astar_calls\":%lld,\"astar_expanded\":%lld,\"kills\":%lld,\"arrived\":%lld,"
        "\"peak_mem_mib\":%.1f,\"hashes\":[",
        __GNUC__, __GNUC_MINOR__, sc["name"].str.c_str(), start_units, n, total, pct(times, 50), pct(times, 95), pct(times, 99), mx, mean,
        sim.units.size(), sim.path_requests, sim.astar_calls, sim.astar_expanded, sim.kills, sim.arrived, peak);
    std::string text = std::string(buf) + hashes + "]}";
    if (!outp.empty()) { std::ofstream o(outp, std::ios::binary); o << text; }
    std::printf("%s\n", text.c_str());
    return 0;
}
