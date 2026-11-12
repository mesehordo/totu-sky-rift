// F3 A/B mérés – C# mag. A tools/ab/python/abcore.py sorról sorra megfelelő átirata (lásd tools/ab/SPEC.md).
// Futtatás: dotnet run -c Release --project tools/ab/csharp -- tools/ab/scenarios/center_500.json [--ticks N] [--out f.json]
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;

static class C
{
    public const double TILE = 32.0;
    public static readonly double SQ2 = Math.Sqrt(2.0);
    public static readonly int[] DX = { 1, -1, 0, 0, 1, 1, -1, -1 };
    public static readonly int[] DY = { 0, 0, 1, -1, 1, -1, 1, -1 };
    public static readonly double[] DC = { 1.0, 1.0, 1.0, 1.0, SQ2, SQ2, SQ2, SQ2 };
    public const double CELL = 128.0;
    public const int PATH_BUDGET = 6, MAX_NODES = 9000, OCC_NODES = 512, FOW_EVERY = 6, HASH_EVERY = 300;
    public static int F(double v) => (int)Math.Floor(v);
    public static double Dist(double ax, double ay, double bx, double by)
    {
        double dx = ax - bx, dy = ay - by;
        return Math.Sqrt(dx * dx + dy * dy);
    }
}

sealed class UType
{
    public double Radius, Speed, Hp, Armor, Damage, Range, Cooldown, ProjSpeed, Splash, Sight;
    public bool Projectile;
    public int UT;
}

struct Pt { public double X, Y; public Pt(double x, double y) { X = x; Y = y; } }

sealed class Unit
{
    public int Id, Team, T;
    public double X, Y, Hp, Cd, StuckT, Lpx, Lpy, WaitT, ScanT, RepathT, R;
    public List<Pt> Path = new List<Pt>();
    public bool HasGoal; public double Gx, Gy;
    public bool Pending, Queued, Started, Done, Alive = true, Moving;
    public int Repaths;
    public Unit Sub;
}

sealed class Proj
{
    public double X, Y, Tx, Ty, Dmg, Speed, Splash;
    public Unit Tgt;
    public int Team, Ut;
    public bool Alive = true;
}

struct Occ { public double X, Y, RR; }

sealed class Heap
{
    double[] f = new double[1024], g = new double[1024];
    int[] n = new int[1024];
    public int Count;
    static bool Less(double f1, double g1, int n1, double f2, double g2, int n2) =>
        f1 < f2 || (f1 == f2 && (g1 < g2 || (g1 == g2 && n1 < n2)));
    public void Clear() { Count = 0; }
    void Swap(int a, int b)
    {
        (f[a], f[b]) = (f[b], f[a]); (g[a], g[b]) = (g[b], g[a]); (n[a], n[b]) = (n[b], n[a]);
    }
    public void Push(double ff, double gg, int nn)
    {
        if (Count == f.Length)
        {
            Array.Resize(ref f, Count * 2); Array.Resize(ref g, Count * 2); Array.Resize(ref n, Count * 2);
        }
        f[Count] = ff; g[Count] = gg; n[Count] = nn;
        int k = Count++;
        while (k > 0)
        {
            int p = (k - 1) >> 1;
            if (Less(f[k], g[k], n[k], f[p], g[p], n[p])) { Swap(k, p); k = p; } else break;
        }
    }
    public void Pop(out double ff, out double gg, out int nn)
    {
        ff = f[0]; gg = g[0]; nn = n[0];
        Count--;
        if (Count > 0)
        {
            f[0] = f[Count]; g[0] = g[Count]; n[0] = n[Count];
            int k = 0;
            while (true)
            {
                int l = 2 * k + 1, r = 2 * k + 2, m = k;
                if (l < Count && Less(f[l], g[l], n[l], f[m], g[m], n[m])) m = l;
                if (r < Count && Less(f[r], g[r], n[r], f[m], g[m], n[m])) m = r;
                if (m == k) break;
                Swap(k, m); k = m;
            }
        }
    }
}

sealed class Sim
{
    public int W, H, Tick, Cw, Ch;
    byte[] block;
    public double Dt, DestX, DestY, MaxR;
    public UType[] Types;
    double[][] matrix;
    public List<Unit> Units = new List<Unit>();
    List<Proj> projs = new List<Proj>();
    List<Unit> queue = new List<Unit>();
    int qhead;
    List<Unit>[] cells;
    Dictionary<double, byte[]> clear = new Dictionary<double, byte[]>();
    byte[][] vis = new byte[8][];
    double[] gcost; int[] came, stamp, memoStamp; byte[] memoVal; int gen;
    Heap heap = new Heap();
    List<Unit> qbuf = new List<Unit>();
    public long PathRequests, AstarCalls, AstarExpanded, Kills, Arrived;

    public Sim(JsonElement sc)
    {
        W = sc.GetProperty("w").GetInt32(); H = sc.GetProperty("h").GetInt32();
        block = Convert.FromBase64String(sc.GetProperty("block").GetString());
        Dt = 1.0 / sc.GetProperty("hz").GetDouble();
        var tl = new List<UType>();
        foreach (var t in sc.GetProperty("types").EnumerateArray())
            tl.Add(new UType
            {
                Radius = t.GetProperty("radius").GetDouble(), Speed = t.GetProperty("speed").GetDouble(),
                Hp = t.GetProperty("hp").GetDouble(), Armor = t.GetProperty("armor").GetDouble(),
                Damage = t.GetProperty("damage").GetDouble(), Range = t.GetProperty("range").GetDouble(),
                Cooldown = t.GetProperty("cooldown").GetDouble(), Projectile = t.GetProperty("projectile").GetBoolean(),
                ProjSpeed = t.GetProperty("proj_speed").GetDouble(), Splash = t.GetProperty("splash").GetDouble(),
                Sight = t.GetProperty("sight").GetDouble(), UT = t.GetProperty("utype").GetInt32()
            });
        Types = tl.ToArray();
        var ml = new List<double[]>();
        foreach (var row in sc.GetProperty("matrix").EnumerateArray())
        {
            var r = new List<double>();
            foreach (var v in row.EnumerateArray()) r.Add(v.GetDouble());
            ml.Add(r.ToArray());
        }
        matrix = ml.ToArray();
        var dest = sc.GetProperty("dest");
        DestX = dest[0].GetDouble(); DestY = dest[1].GetDouble();
        int i = 0;
        foreach (var u in sc.GetProperty("units").EnumerateArray())
        {
            int t = u.GetProperty("type").GetInt32();
            var un = new Unit
            {
                Id = ++i, Team = u.GetProperty("team").GetInt32(), T = t, X = u.GetProperty("x").GetDouble(),
                Y = u.GetProperty("y").GetDouble(), Hp = Types[t].Hp, R = Types[t].Radius
            };
            un.Lpx = un.X; un.Lpy = un.Y; un.ScanT = (un.Id % 8) * 0.05;
            Units.Add(un);
        }
        MaxR = 0; foreach (var t in Types) MaxR = Math.Max(MaxR, t.Radius);
        Cw = C.F(W * C.TILE / C.CELL) + 1; Ch = C.F(H * C.TILE / C.CELL) + 1;
        cells = new List<Unit>[Cw * Ch];
        for (int k = 0; k < cells.Length; k++) cells[k] = new List<Unit>();
        for (int k = 0; k < 8; k++) vis[k] = new byte[W * H];
        int n = W * H;
        gcost = new double[n]; came = new int[n]; stamp = new int[n]; memoStamp = new int[n]; memoVal = new byte[n];
        UpdateFow();
    }

    bool Passable(int tx, int ty) => tx >= 0 && tx < W && ty >= 0 && ty < H && block[ty * W + tx] == 0;

    bool Fits(double x, double y, double r)
    {
        if (x - r < 0 || y - r < 0 || x + r > W * C.TILE || y + r > H * C.TILE) return false;
        int ty0 = C.F((y - r) / C.TILE), ty1 = C.F((y + r) / C.TILE);
        int tx0 = C.F((x - r) / C.TILE), tx1 = C.F((x + r) / C.TILE);
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++)
            {
                if (Passable(tx, ty)) continue;
                double dx = Math.Max(Math.Max(tx * C.TILE - x, 0.0), x - (tx + 1) * C.TILE);
                double dy = Math.Max(Math.Max(ty * C.TILE - y, 0.0), y - (ty + 1) * C.TILE);
                if (dx * dx + dy * dy < r * r) return false;
            }
        return true;
    }

    bool TileOk(int tx, int ty, double r)
    {
        if (!clear.TryGetValue(r, out var grid))
        {
            grid = new byte[W * H];
            Array.Fill(grid, (byte)2);
            clear[r] = grid;
        }
        int i = ty * W + tx;
        byte v = grid[i];
        if (v == 2) { v = (byte)(Fits((tx + 0.5) * C.TILE, (ty + 0.5) * C.TILE, r) ? 1 : 0); grid[i] = v; }
        return v == 1;
    }

    static bool Occupied(List<Occ> occ, double x, double y)
    {
        foreach (var o in occ)
        {
            double dx = x - o.X, dy = y - o.Y;
            if (dx * dx + dy * dy < o.RR) return true;
        }
        return false;
    }

    bool ClearLine(double x0, double y0, double x1, double y1, double r, List<Occ> occ)
    {
        double dx = x1 - x0, dy = y1 - y0;
        double d = Math.Sqrt(dx * dx + dy * dy);
        if (d < 1.0) return true;
        int steps = C.F(d / 4.0) + 1;
        for (int i = 0; i <= steps; i++)
        {
            double t = (double)i / steps;
            double px = x0 + dx * t, py = y0 + dy * t;
            if (!Fits(px, py, r) || (occ != null && Occupied(occ, px, py))) return false;
        }
        return true;
    }

    bool NearestFree(int gx, int gy, int sx, int sy, out int ox, out int oy)
    {
        for (int ring = 1; ring < 13; ring++)
        {
            bool found = false; int bd = 0; ox = oy = 0;
            for (int ty = gy - ring; ty <= gy + ring; ty++)
                for (int tx = gx - ring; tx <= gx + ring; tx++)
                {
                    if (Math.Max(Math.Abs(tx - gx), Math.Abs(ty - gy)) != ring || !Passable(tx, ty)) continue;
                    int d = (tx - sx) * (tx - sx) + (ty - sy) * (ty - sy);
                    if (!found || d < bd) { found = true; bd = d; ox = tx; oy = ty; }
                }
            if (found) return true;
        }
        ox = oy = 0;
        return false;
    }

    List<(int, int)> FindPath(int sx, int sy, int gx, int gy, double r, List<Occ> occ, int maxNodes)
    {
        var outp = new List<(int, int)>();
        AstarCalls++;
        if (!Passable(gx, gy))
        {
            if (!NearestFree(gx, gy, sx, sy, out gx, out gy)) return outp;
        }
        int start = sy * W + sx, goal = gy * W + gx;
        if (start == goal) return outp;
        gen++;
        int lgen = gen;
        bool Valid(int x, int y)
        {
            int k = y * W + x;
            if (memoStamp[k] == lgen) return memoVal[k] == 1;
            bool v = TileOk(x, y, r);
            if (v && occ != null) v = !Occupied(occ, (x + 0.5) * C.TILE, (y + 0.5) * C.TILE);
            memoStamp[k] = lgen; memoVal[k] = (byte)(v ? 1 : 0);
            return v;
        }
        double Hd(int x, int y)
        {
            int dx = Math.Abs(x - gx), dy = Math.Abs(y - gy);
            return (dx + dy) + (C.SQ2 - 2.0) * Math.Min(dx, dy);
        }
        heap.Clear();
        stamp[start] = lgen; gcost[start] = 0.0; came[start] = -1;
        heap.Push(Hd(sx, sy), 0.0, start);
        int best = start; double besth = Hd(sx, sy);
        int expanded = 0;
        while (heap.Count > 0 && expanded < maxNodes)
        {
            heap.Pop(out _, out double g, out int n);
            if (n == goal) { best = goal; break; }
            if (g > gcost[n]) continue;
            expanded++;
            int x = n % W, y = n / W;
            for (int d = 0; d < 8; d++)
            {
                int dx = C.DX[d], dy = C.DY[d];
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                if (!Valid(nx, ny)) continue;
                if (dx != 0 && dy != 0 && (!Valid(nx, y) || !Valid(x, ny))) continue;
                int ni = ny * W + nx;
                double ng = g + C.DC[d];
                if (stamp[ni] != lgen || ng < gcost[ni])
                {
                    stamp[ni] = lgen; gcost[ni] = ng; came[ni] = n;
                    double hh = Hd(nx, ny);
                    heap.Push(ng + hh, ng, ni);
                    if (hh < besth) { best = ni; besth = hh; }
                }
            }
        }
        AstarExpanded += expanded;
        int m = best;
        while (m != start && m != -1) { outp.Add((m % W, m / W)); m = came[m]; }
        outp.Reverse();
        return outp;
    }

    List<Pt> Smooth(double sx, double sy, List<Pt> pts, double r, List<Occ> occ)
    {
        var o = new List<Pt>();
        double cx = sx, cy = sy;
        int i = 0, n = pts.Count;
        while (i < n)
        {
            int j = Math.Min(n - 1, i + 6);
            while (j > i && !ClearLine(cx, cy, pts[j].X, pts[j].Y, r, occ)) j--;
            o.Add(pts[j]);
            cx = pts[j].X; cy = pts[j].Y;
            i = j + 1;
        }
        return o;
    }

    static List<Pt> Centers(List<(int, int)> tiles)
    {
        var p = new List<Pt>(tiles.Count);
        foreach (var (a, b) in tiles) p.Add(new Pt((a + 0.5) * C.TILE, (b + 0.5) * C.TILE));
        return p;
    }

    List<Pt> UnitPath(Unit u)
    {
        double gx = u.Gx, gy = u.Gy, r = u.R;
        int sx = C.F(u.X / C.TILE), sy = C.F(u.Y / C.TILE);
        int tx = C.F(gx / C.TILE), ty = C.F(gy / C.TILE);
        if (u.Repaths == 0 && ClearLine(u.X, u.Y, gx, gy, r, null)) return new List<Pt> { new Pt(gx, gy) };
        List<Occ> occ = null;
        if (u.Repaths > 0)
        {
            double reach = C.TILE * 4;
            occ = new List<Occ>();
            foreach (var v in Query(u.X, u.Y, reach))
                if (v != u && v.Alive && C.Dist(v.X, v.Y, u.X, u.Y) < reach)
                {
                    double rr = r + v.R - 0.01;
                    occ.Add(new Occ { X = v.X, Y = v.Y, RR = rr * rr });
                }
            if (occ.Count > 0)
            {
                var tiles = FindPath(sx, sy, tx, ty, r, occ, C.OCC_NODES);
                if (tiles.Count > 0 && tiles[tiles.Count - 1] == (tx, ty))
                {
                    var pts = Centers(tiles);
                    var last = pts[pts.Count - 1];
                    if (ClearLine(last.X, last.Y, gx, gy, r, occ))
                    {
                        if (!(last.X == gx && last.Y == gy)) pts.Add(new Pt(gx, gy));
                        return Smooth(u.X, u.Y, pts, r, occ);
                    }
                }
            }
            else occ = null;
        }
        if (occ == null && ClearLine(u.X, u.Y, gx, gy, r, null)) return new List<Pt> { new Pt(gx, gy) };
        var t2 = FindPath(sx, sy, tx, ty, r, null, C.MAX_NODES);
        var p2 = Centers(t2);
        if (p2.Count > 0 && !(p2[p2.Count - 1].X == gx && p2[p2.Count - 1].Y == gy) && Passable(tx, ty)
            && ClearLine(p2[p2.Count - 1].X, p2[p2.Count - 1].Y, gx, gy, r, null))
            p2.Add(new Pt(gx, gy));
        else if (p2.Count == 0 && ClearLine(u.X, u.Y, gx, gy, r, null)) return new List<Pt> { new Pt(gx, gy) };
        return Smooth(u.X, u.Y, p2, r, null);
    }

    void Rebuild()
    {
        foreach (var c in cells) c.Clear();
        foreach (var u in Units) if (u.Alive) cells[CellOf(u.X, u.Y)].Add(u);
    }

    int CellOf(double x, double y)
    {
        int cx = Math.Min(Cw - 1, Math.Max(0, C.F(x / C.CELL)));
        int cy = Math.Min(Ch - 1, Math.Max(0, C.F(y / C.CELL)));
        return cy * Cw + cx;
    }

    List<Unit> Query(double x, double y, double r)
    {
        var o = new List<Unit>();
        int cx0 = Math.Max(0, C.F((x - r) / C.CELL)), cx1 = Math.Min(Cw - 1, C.F((x + r) / C.CELL));
        int cy0 = Math.Max(0, C.F((y - r) / C.CELL)), cy1 = Math.Min(Ch - 1, C.F((y + r) / C.CELL));
        for (int cy = cy0; cy <= cy1; cy++)
            for (int cx = cx0; cx <= cx1; cx++) o.AddRange(cells[cy * Cw + cx]);
        return o;
    }

    void SetPath(Unit u, double gx, double gy)
    {
        u.HasGoal = true; u.Gx = gx; u.Gy = gy;
        u.StuckT = 0.0; u.Lpx = u.X; u.Lpy = u.Y;
        u.Path = new List<Pt>();
        u.Pending = true;
        if (!u.Queued) { u.Queued = true; queue.Add(u); PathRequests++; }
    }

    void ProcessPaths()
    {
        int budget = C.PATH_BUDGET;
        while (qhead < queue.Count && budget > 0)
        {
            var u = queue[qhead++];
            u.Queued = false;
            if (!u.Alive || !u.Pending || !u.HasGoal) { u.Pending = false; continue; }
            budget -= u.Repaths != 0 ? 3 : 1;
            u.Path = UnitPath(u);
            u.Pending = false;
        }
        if (qhead > 4096) { queue.RemoveRange(0, qhead); qhead = 0; }
    }

    Unit Blocker(Unit u, double nx, double ny)
    {
        foreach (var v in Query(nx, ny, u.R + MaxR))
        {
            if (v == u || !v.Alive) continue;
            double m = u.R + v.R - 0.5;
            double dn = C.Dist(nx, ny, v.X, v.Y);
            if (dn < m && dn < C.Dist(u.X, u.Y, v.X, v.Y)) return v;
        }
        return null;
    }

    bool Follow(Unit u)
    {
        if (u.Pending) return false;
        if (u.Path.Count == 0) return true;
        double px = u.Path[0].X, py = u.Path[0].Y;
        double dx = px - u.X, dy = py - u.Y;
        double d = Math.Sqrt(dx * dx + dy * dy);
        double sp = Types[u.T].Speed;
        double step = sp * Dt;
        double nx, ny;
        if (d <= step) { nx = px; ny = py; } else { nx = u.X + dx / d * step; ny = u.Y + dy / d * step; }
        bool sameTile = C.F(nx / C.TILE) == C.F(u.X / C.TILE) && C.F(ny / C.TILE) == C.F(u.Y / C.TILE);
        if (!Fits(nx, ny, u.R) && (!sameTile || Fits(u.X, u.Y, u.R)))
        {
            u.Repaths = Math.Min(8, u.Repaths + 1);
            double gx = u.HasGoal ? u.Gx : u.Path[u.Path.Count - 1].X, gy = u.HasGoal ? u.Gy : u.Path[u.Path.Count - 1].Y;
            SetPath(u, gx, gy);
            return false;
        }
        if (Blocker(u, nx, ny) != null) u.WaitT += Dt;
        else if (d <= step)
        {
            u.X = px; u.Y = py; u.Path.RemoveAt(0); u.Moving = true;
            u.WaitT = Math.Max(0.0, u.WaitT - Dt * 0.5);
        }
        else
        {
            u.X = nx; u.Y = ny; u.Moving = true;
            u.WaitT = Math.Max(0.0, u.WaitT - Dt * 0.5);
        }
        u.StuckT += Dt;
        if (u.StuckT >= 0.8 + Math.Min(4, u.Repaths) * 0.15)
        {
            double ox = u.Lpx, oy = u.Lpy;
            u.Lpx = u.X; u.Lpy = u.Y; u.StuckT = 0.0;
            double moved = C.Dist(u.X, u.Y, ox, oy);
            double progress = C.Dist(px, py, ox, oy) - C.Dist(px, py, u.X, u.Y);
            if (u.Path.Count > 0 && (moved < sp * 0.15 || (u.WaitT > 0.6 && progress < sp * 0.12)))
            {
                u.Repaths = Math.Min(8, u.Repaths + 1);
                double gx = u.HasGoal ? u.Gx : u.Path[u.Path.Count - 1].X, gy = u.HasGoal ? u.Gy : u.Path[u.Path.Count - 1].Y;
                SetPath(u, gx, gy);
            }
            else if (moved >= sp * 0.15) u.Repaths = 0;
        }
        return u.Path.Count == 0 && !u.Pending;
    }

    static double Gap(Unit a, Unit b) => Math.Max(0.0, C.Dist(a.X, a.Y, b.X, b.Y) - a.R - b.R);

    bool Visible(int team, Unit e)
    {
        int tx = C.F(e.X / C.TILE), ty = C.F(e.Y / C.TILE);
        return tx >= 0 && tx < W && ty >= 0 && ty < H && vis[team][ty * W + tx] == 1;
    }

    Unit FindEnemy(Unit u, double radius)
    {
        Unit best = null; double bestv = 0.0;
        foreach (var e in Query(u.X, u.Y, radius + 32.0))
        {
            if (e.Team == u.Team || !e.Alive || !Visible(u.Team, e)) continue;
            double d = Gap(u, e);
            if (d > radius) continue;
            if (best == null || d < bestv) { best = e; bestv = d; }
        }
        return best;
    }

    void Apply(int team, int ut, Unit tgt, double dmg)
    {
        if (!tgt.Alive) return;
        int tt = Types[tgt.T].UT;
        double m = (ut >= 0 && tt >= 0) ? matrix[ut][tt] : 1.0;
        double d = Math.Max(0.5, dmg * m - Types[tgt.T].Armor);
        tgt.Hp -= d;
        if (tgt.Hp <= 0.0) { tgt.Alive = false; Kills++; }
    }

    void Hit(int team, int ut, Unit tgt, double dmg, double splash, double x, double y)
    {
        if (tgt != null) Apply(team, ut, tgt, dmg);
        if (splash > 0.0)
            foreach (var e in Query(x, y, splash + MaxR))
            {
                if (e == tgt || !e.Alive || e.Team == team) continue;
                if (C.Dist(e.X, e.Y, x, y) - e.R <= splash) Apply(team, ut, e, dmg * 0.5);
            }
    }

    void TryAttack(Unit u, Unit tgt)
    {
        if (u.Cd > 0.0) return;
        var ty = Types[u.T];
        u.Cd = ty.Cooldown;
        if (ty.Projectile)
            projs.Add(new Proj
            {
                X = u.X, Y = u.Y, Tgt = tgt, Tx = tgt.X, Ty = tgt.Y, Dmg = ty.Damage, Team = u.Team, Ut = ty.UT,
                Speed = ty.ProjSpeed, Splash = ty.Splash
            });
        else Hit(u.Team, ty.UT, tgt, ty.Damage, ty.Splash, tgt.X, tgt.Y);
    }

    void Engage(Unit u, Unit tgt)
    {
        var ty = Types[u.T];
        if (Gap(u, tgt) <= ty.Range + 3.0)
        {
            u.Path = new List<Pt>(); u.Pending = false;
            TryAttack(u, tgt);
            return;
        }
        u.RepathT -= Dt;
        if ((u.Path.Count == 0 && !u.Pending) || u.RepathT <= 0.0)
        {
            if (u.RepathT <= 0.0 || u.Path.Count == 0) { u.RepathT = 0.8; SetPath(u, tgt.X, tgt.Y); }
        }
        Follow(u);
    }

    void UpdateUnit(Unit u)
    {
        var ty = Types[u.T];
        if (u.Cd > 0.0) u.Cd -= Dt;
        u.Moving = false;
        var sub = u.Sub;
        if (sub != null && (!sub.Alive || Gap(u, sub) > ty.Sight * 1.6)) { sub = u.Sub = null; u.Started = false; }
        if (sub == null)
        {
            u.ScanT -= Dt;
            if (u.ScanT <= 0.0)
            {
                u.ScanT = 0.4;
                var e = FindEnemy(u, ty.Sight);
                if (e != null) { u.Sub = e; return; }
            }
            if (u.Done) return;
            if (!u.Started || !u.HasGoal) { u.Started = true; SetPath(u, DestX, DestY); return; }
            if (Follow(u)) { u.Done = true; Arrived++; }
            return;
        }
        Engage(u, sub);
    }

    void UpdateProjs()
    {
        foreach (var p in projs)
        {
            if (p.Tgt.Alive) { p.Tx = p.Tgt.X; p.Ty = p.Tgt.Y; }
            double dx = p.Tx - p.X, dy = p.Ty - p.Y;
            double d = Math.Sqrt(dx * dx + dy * dy);
            double step = p.Speed * Dt;
            if (d <= step) { p.Alive = false; Hit(p.Team, p.Ut, p.Tgt.Alive ? p.Tgt : null, p.Dmg, p.Splash, p.Tx, p.Ty); }
            else { p.X += dx / d * step; p.Y += dy / d * step; }
        }
    }

    void Separate()
    {
        double WW = W * C.TILE, HH = H * C.TILE;
        var near = new List<Unit>();
        for (int ci = 0; ci < Cw * Ch; ci++)
        {
            var lst = cells[ci];
            if (lst.Count == 0) continue;
            int cx = ci % Cw, cy = ci / Cw;
            near.Clear();
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                {
                    int x2 = cx + dx, y2 = cy + dy;
                    if (x2 >= 0 && x2 < Cw && y2 >= 0 && y2 < Ch) near.AddRange(cells[y2 * Cw + x2]);
                }
            foreach (var u in lst)
            {
                if (!u.Alive) continue;
                foreach (var v in near)
                {
                    if (v.Id <= u.Id || !v.Alive) continue;
                    double ddx = v.X - u.X, ddy = v.Y - u.Y;
                    double m = u.R + v.R;
                    if (Math.Abs(ddx) >= m || Math.Abs(ddy) >= m) continue;
                    double d = Math.Sqrt(ddx * ddx + ddy * ddy);
                    if (d >= m) continue;
                    if (d < 0.01)
                    {
                        if ((u.Id + v.Id) % 2 == 0) { ddx = 1.0; ddy = 0.0; } else { ddx = 0.0; ddy = 1.0; }
                        d = 1.0;
                    }
                    double ov = (m - d) * 0.5;
                    double ku = u.Moving ? 1.0 : 0.0, kv = v.Moving ? 1.0 : 0.0;
                    if (ku + kv == 0.0) { ku = 1.0; kv = 1.0; }
                    double wu = ku / (ku + kv), wv = kv / (ku + kv);
                    double nx = ddx / d, ny = ddy / d;
                    double ux = u.X - nx * ov * wu, uy = u.Y - ny * ov * wu;
                    double vx = v.X + nx * ov * wv, vy = v.Y + ny * ov * wv;
                    if (u.R <= ux && ux <= WW - u.R && u.R <= uy && uy <= HH - u.R && Fits(ux, uy, u.R)) { u.X = ux; u.Y = uy; }
                    if (v.R <= vx && vx <= WW - v.R && v.R <= vy && vy <= HH - v.R && Fits(vx, vy, v.R)) { v.X = vx; v.Y = vy; }
                }
            }
        }
    }

    void UpdateFow()
    {
        for (int t = 0; t < 8; t++) Array.Clear(vis[t]);
        foreach (var u in Units)
        {
            if (!u.Alive) continue;
            double s = Types[u.T].Sight, ss = s * s;
            var v = vis[u.Team];
            int tx0 = Math.Max(0, C.F((u.X - s) / C.TILE)), tx1 = Math.Min(W - 1, C.F((u.X + s) / C.TILE));
            int ty0 = Math.Max(0, C.F((u.Y - s) / C.TILE)), ty1 = Math.Min(H - 1, C.F((u.Y + s) / C.TILE));
            for (int ty = ty0; ty <= ty1; ty++)
            {
                double dy = (ty + 0.5) * C.TILE - u.Y;
                for (int tx = tx0; tx <= tx1; tx++)
                {
                    double dx = (tx + 0.5) * C.TILE - u.X;
                    if (dx * dx + dy * dy <= ss) v[ty * W + tx] = 1;
                }
            }
        }
    }

    public void Step()
    {
        Tick++;
        Rebuild();
        ProcessPaths();
        foreach (var u in Units) if (u.Alive) UpdateUnit(u);
        UpdateProjs();
        Separate();
        Units.RemoveAll(u => !u.Alive);
        projs.RemoveAll(p => !p.Alive);
        if (Tick % C.FOW_EVERY == 0) UpdateFow();
    }

    public ulong StateHash()
    {
        ulong h = 1469598103934665603UL;
        foreach (var u in Units)
        {
            long[] vals = { u.Id, (long)Math.Floor(u.X * 16.0), (long)Math.Floor(u.Y * 16.0), (long)Math.Floor(u.Hp * 16.0) };
            foreach (var v in vals) unchecked { h = (h ^ (ulong)v) * 1099511628211UL; }
        }
        return h;
    }
}

static class Program
{
    static double Pct(List<double> vals, double p)
    {
        var s = new List<double>(vals); s.Sort();
        int i = (int)Math.Min(s.Count - 1, Math.Max(0, Math.Round(p / 100.0 * (s.Count - 1))));
        return Math.Round(s[i], 3);
    }

    static int Main(string[] args)
    {
        string path = args[0], outp = null; int ticks = -1;
        for (int i = 1; i < args.Length; i++)
        {
            if (args[i] == "--ticks") ticks = int.Parse(args[i + 1]);
            if (args[i] == "--out") outp = args[i + 1];
        }
        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        var sc = doc.RootElement;
        var sim = new Sim(sc);
        int n = ticks > 0 ? ticks : sc.GetProperty("ticks").GetInt32();
        int startUnits = sim.Units.Count;
        var times = new List<double>(n);
        var hashes = new List<string>();
        long alloc0 = GC.GetTotalAllocatedBytes(true);
        var pause0 = GC.GetTotalPauseDuration();
        int[] gc0 = { GC.CollectionCount(0), GC.CollectionCount(1), GC.CollectionCount(2) };
        var all = Stopwatch.StartNew();
        var sw = new Stopwatch();
        for (int k = 0; k < n; k++)
        {
            sw.Restart();
            sim.Step();
            times.Add(sw.Elapsed.TotalMilliseconds);
            if (sim.Tick % C.HASH_EVERY == 0) hashes.Add($"[{sim.Tick},\"{sim.StateHash():x16}\"]");
        }
        double total = all.Elapsed.TotalSeconds;
        long alloc = GC.GetTotalAllocatedBytes(true) - alloc0;
        var pause = GC.GetTotalPauseDuration() - pause0;
        double mean = 0; foreach (var t in times) mean += t; mean /= times.Count;
        double max = 0; foreach (var t in times) max = Math.Max(max, t);
        var ci = CultureInfo.InvariantCulture;
        var sb = new StringBuilder();
        sb.Append("{\"lang\":\"csharp\",\"version\":\"").Append(Environment.Version).Append("\",");
        sb.Append("\"scenario\":\"").Append(sc.GetProperty("name").GetString()).Append("\",");
        sb.AppendFormat(ci, "\"units_start\":{0},\"ticks\":{1},\"total_s\":{2},", startUnits, n, Math.Round(total, 3));
        sb.AppendFormat(ci, "\"tick_ms\":{{\"p50\":{0},\"p95\":{1},\"p99\":{2},\"max\":{3},\"mean\":{4}}},",
            Pct(times, 50), Pct(times, 95), Pct(times, 99), Math.Round(max, 3), Math.Round(mean, 3));
        sb.AppendFormat(ci, "\"alive_end\":{0},\"path_requests\":{1},\"astar_calls\":{2},\"astar_expanded\":{3},\"kills\":{4},\"arrived\":{5},",
            sim.Units.Count, sim.PathRequests, sim.AstarCalls, sim.AstarExpanded, sim.Kills, sim.Arrived);
        sb.AppendFormat(ci, "\"peak_mem_mib\":{0},", Math.Round(Process.GetCurrentProcess().PeakWorkingSet64 / 1048576.0, 1));
        sb.AppendFormat(ci, "\"gc\":{{\"gen0\":{0},\"gen1\":{1},\"gen2\":{2},\"pause_ms_total\":{3},\"alloc_bytes_per_tick\":{4}}},",
            GC.CollectionCount(0) - gc0[0], GC.CollectionCount(1) - gc0[1], GC.CollectionCount(2) - gc0[2],
            Math.Round(pause.TotalMilliseconds, 3), alloc / Math.Max(1, n));
        sb.Append("\"hashes\":[").Append(string.Join(",", hashes)).Append("]}");
        var text = sb.ToString();
        if (outp != null) File.WriteAllText(outp, text);
        Console.WriteLine(text);
        return 0;
    }
}
