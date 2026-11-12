// Rajzolási próba (raylib-cs, OpenGL 3.3): sok animált TOTU-egység + terep + HUD-váz 1280×720-ban, képkockaidő-mérés.
// Futtatás: dotnet run -c Release -- <atlasz mappa> <all|typical> <egységszám> <mp> [kimenet.json]
// Az atlaszt a tools/ab/render/build_atlas.py készíti a játék grafikájából (nem része a repónak).
using System.Diagnostics;
using System.Globalization;
using System.Numerics;
using System.Text.Json;
using Raylib_cs;

static class Probe
{
    struct Frame { public Rectangle Src; public Vector2 Off; }
    sealed class UnitType { public Frame[][] Walk = new Frame[8][]; public Frame[][] Attack = new Frame[8][]; public float WalkFps, AttackFps; }
    struct U { public float X, Y, Tx, Ty, Speed, Phase; public int Type, Dir; public bool Attacking; }

    static int Main(string[] args)
    {
        string dir = args[0], mode = args[1];
        int n = int.Parse(args[2]); double seconds = double.Parse(args[3], CultureInfo.InvariantCulture);
        string? outp = args.Length > 4 ? args[4] : null;
        const int SW = 1280, SH = 720, HUD = 180;

        using var doc = JsonDocument.Parse(File.ReadAllText(Path.Combine(dir, "atlas.json")));
        var types = new List<UnitType>();
        foreach (var u in doc.RootElement.GetProperty("units").EnumerateObject())
        {
            var t = new UnitType { WalkFps = u.Value.GetProperty("walk_fps").GetSingle(), AttackFps = u.Value.GetProperty("attack_fps").GetSingle() };
            for (int d = 0; d < 8; d++)
            {
                t.Walk[d] = Load(u.Value.GetProperty("walk")[d]);
                t.Attack[d] = Load(u.Value.GetProperty("attack")[d]);
            }
            types.Add(t);
        }

        Raylib.SetTraceLogLevel(TraceLogLevel.Info);
        Raylib.InitWindow(SW, SH, "TOTU rajzolási próba");
        Raylib.SetTargetFPS(0);                       // korlátlan: a nyers teljesítményt mérjük (nincs vsync)
        var atlas = Raylib.LoadTexture(Path.Combine(dir, "atlas.png"));
        Raylib.SetTextureFilter(atlas, TextureFilter.Bilinear);
        var tileset = Raylib.LoadTexture(Path.Combine(dir, "tileset.png"));

        // „all”: mind a képernyőn (kicsinyítve) – legrosszabb eset; „typical”: 4608 px-es világ, kamera a közepén.
        bool all = mode == "all";
        float scale = all ? 0.30f : 0.55f;
        float worldW = all ? SW : 4608, worldH = all ? SH - HUD : 4608;
        float camX = all ? 0 : worldW / 2 - SW / 2, camY = all ? 0 : worldH / 2 - (SH - HUD) / 2;
        var rnd = new Random(12345);
        var units = new U[n];
        for (int i = 0; i < n; i++)
        {
            float x, y;
            if (all) { x = rnd.NextSingle() * worldW; y = 20 + rnd.NextSingle() * (worldH - 20); }
            else
            {   // seregek: a kamera körül sűrűbben, hogy a látható szám ~25–35% legyen
                double a = rnd.NextDouble() * Math.PI * 2, r = Math.Abs(Gauss(rnd)) * 1200;
                x = (float)(worldW / 2 + Math.Cos(a) * r); y = (float)(worldH / 2 + Math.Sin(a) * r);
            }
            units[i] = new U { X = x, Y = y, Tx = x, Ty = y, Speed = 40 + rnd.NextSingle() * 50, Type = i % types.Count,
                               Phase = rnd.NextSingle() * 10, Attacking = rnd.NextDouble() < 0.3 };
        }
        var order = new int[n]; var keys = new float[n];
        var times = new List<double>(); var sw = new Stopwatch(); var clock = Stopwatch.StartNew();
        double warmup = 2.0; int visibleSum = 0, measured = 0;

        while (!Raylib.WindowShouldClose() && clock.Elapsed.TotalSeconds < warmup + seconds)
        {
            sw.Restart();
            float dt = Math.Min(0.05f, Raylib.GetFrameTime()), now = (float)clock.Elapsed.TotalSeconds;
            // mozgás és irány (8 irány: E, SE, S, SW, W, NW, N, NE – mint a sprite-lapokon)
            for (int i = 0; i < n; i++)
            {
                ref var u = ref units[i];
                float dx = u.Tx - u.X, dy = u.Ty - u.Y, d = MathF.Sqrt(dx * dx + dy * dy);
                if (d < 4 || rnd.NextDouble() < 0.002)
                {
                    u.Tx = Math.Clamp(u.X + (rnd.NextSingle() - 0.5f) * 300, 0, worldW);
                    u.Ty = Math.Clamp(u.Y + (rnd.NextSingle() - 0.5f) * 300, 20, worldH);
                    continue;
                }
                if (!u.Attacking) { u.X += dx / d * u.Speed * dt; u.Y += dy / d * u.Speed * dt; }
                float ang = MathF.Atan2(dy, dx);
                u.Dir = ((int)MathF.Round(ang / (MathF.PI / 4)) + 8) % 8;
            }
            // láthatóság (kivágás) és Y-rendezés
            int vis = 0;
            for (int i = 0; i < n; i++)
            {
                float sx = (units[i].X - camX), sy = (units[i].Y - camY);
                if (sx < -80 || sx > SW + 80 || sy < -80 || sy > SH - HUD + 120) continue;
                order[vis] = i; keys[vis] = units[i].Y; vis++;
            }
            Array.Sort(keys, order, 0, vis);

            Raylib.BeginDrawing();
            Raylib.ClearBackground(Color.Black);
            // terep: 64×64-es csempék a tileset első csempéjéből
            var tsrc = new Rectangle(0, 0, 64, 64);
            for (int ty = 0; ty < (SH - HUD) / 48 + 2; ty++)
                for (int tx = 0; tx < SW / 48 + 2; tx++)
                    Raylib.DrawTexturePro(tileset, tsrc, new Rectangle(tx * 48 - camX % 48, ty * 48 - camY % 48, 48, 48), Vector2.Zero, 0, Color.White);
            // egységek
            for (int k = 0; k < vis; k++)
            {
                ref var u = ref units[order[k]];
                var t = types[u.Type];
                var frames = u.Attacking ? t.Attack[u.Dir] : t.Walk[u.Dir];
                int fi = (int)((now + u.Phase) * (u.Attacking ? t.AttackFps : t.WalkFps)) % frames.Length;
                var f = frames[fi];
                float px = u.X - camX + f.Off.X * scale, py = u.Y - camY + f.Off.Y * scale;
                Raylib.DrawEllipse((int)(u.X - camX), (int)(u.Y - camY), 14 * scale * 2, 5 * scale * 2, new Color(0, 0, 0, 70));
                Raylib.DrawTexturePro(atlas, f.Src, new Rectangle(px, py, f.Src.Width * scale, f.Src.Height * scale), Vector2.Zero, 0, Color.White);
            }
            // HUD-váz: alsó sáv, minitérkép minden egységgel, 2×3 + 4×3 + 4×3 rács, felső sáv, szöveg
            Raylib.DrawRectangle(0, SH - HUD, SW, HUD, new Color(40, 32, 24, 255));
            Raylib.DrawRectangle(10, SH - HUD + 8, 164, 164, new Color(30, 60, 30, 255));
            for (int i = 0; i < n; i++)
                Raylib.DrawRectangle(10 + (int)(units[i].X / worldW * 162), SH - HUD + 8 + (int)(units[i].Y / worldH * 162), 2, 2, i % 2 == 0 ? Color.Red : Color.SkyBlue);
            Grid(200, SH - HUD + 14, 2, 3, 46); Grid(330, SH - HUD + 14, 4, 3, 46); Grid(1080, SH - HUD + 14, 4, 3, 46);
            Raylib.DrawRectangle(0, 0, SW, 28, new Color(40, 32, 24, 230));
            Raylib.DrawText($"FPS {Raylib.GetFPS()}  látható {vis}/{n}  mód {mode}", 12, 6, 18, Color.Gold);
            for (int i = 0; i < 4; i++) Raylib.DrawText($"{1000 + i * 37}", 520 + i * 120, 6, 18, Color.RayWhite);
            Raylib.EndDrawing();

            double ms = sw.Elapsed.TotalMilliseconds;
            if (clock.Elapsed.TotalSeconds > warmup) { times.Add(ms); visibleSum += vis; measured++; }
        }
        string gl = Rlgl.GetVersion().ToString();
        Raylib.UnloadTexture(atlas); Raylib.UnloadTexture(tileset);
        Raylib.CloseWindow();

        times.Sort();
        double P(double p) => Math.Round(times[(int)Math.Min(times.Count - 1, Math.Round(p / 100 * (times.Count - 1)))], 3);
        double mean = times.Average();
        var ci = CultureInfo.InvariantCulture;
        string res = string.Format(ci,
            "{{\"mode\":\"{0}\",\"units\":{1},\"visible_avg\":{2},\"frames\":{3},\"fps_avg\":{4},\"frame_ms\":{{\"p50\":{5},\"p95\":{6},\"p99\":{7},\"max\":{8},\"mean\":{9}}},\"gl\":\"{10}\",\"scale\":{11}}}",
            mode, n, measured > 0 ? visibleSum / measured : 0, times.Count, Math.Round(1000 / mean, 1), P(50), P(95), P(99), Math.Round(times[^1], 3), Math.Round(mean, 3), gl, scale);
        Console.WriteLine(res);
        if (outp != null) File.WriteAllText(outp, res);
        return 0;
    }

    static Frame[] Load(JsonElement arr)
    {
        var l = new List<Frame>();
        foreach (var f in arr.EnumerateArray())
        {
            var r = f.GetProperty("r"); var o = f.GetProperty("o");
            l.Add(new Frame { Src = new Rectangle(r[0].GetSingle(), r[1].GetSingle(), r[2].GetSingle(), r[3].GetSingle()),
                              Off = new Vector2(o[0].GetSingle(), o[1].GetSingle()) });
        }
        return l.ToArray();
    }

    static void Grid(int x, int y, int cols, int rows, int s)
    {
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
            {
                Raylib.DrawRectangle(x + c * (s + 4), y + r * (s + 4), s, s, new Color(70, 58, 44, 255));
                Raylib.DrawRectangleLines(x + c * (s + 4), y + r * (s + 4), s, s, Color.Gold);
            }
    }

    static double Gauss(Random r) => Math.Sqrt(-2 * Math.Log(1 - r.NextDouble())) * Math.Cos(2 * Math.PI * r.NextDouble());
}
