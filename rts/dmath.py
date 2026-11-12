"""Determinisztikus matematika a szimulációhoz (C#-port, PORT_TERV.md P0).

A szimuláció csak ezeket a függvényeket használja a `math.hypot`, `math.dist`, `math.sin`, `math.cos`,
`math.atan2` és a lebegőpontos `x ** 2` helyett. Mindegyik csak `+ - * /`, `sqrt`, `floor` és előjelkezelés
műveletekből áll, ezért a C#-oldali `DMath` ugyanebben a sorrendben számolva bitre azonos eredményt ad
(az IEEE 754 ezeket a műveleteket pontosan kerekítve írja elő; a platformkönyvtárak `sin`/`hypot`/`pow`
függvényei ezt nem garantálják – Windows-on pl. `x ** 2 != x * x` minden ~500. esetben).

FONTOS: a kifejezések sorrendjét és zárójelezését a C#-átiratnak pontosan követnie kell.
A szinusz/koszinusz/arkusztangens az fdlibm (Sun Microsystems, 1993; szabadon felhasználható) kerneleit követi.
"""
from math import copysign, floor, sqrt

PI = 3.14159265358979311600e+00
TAU = 2.0 * PI


def sq(x):
    return x * x


def hypot(dx, dy):
    return sqrt(dx * dx + dy * dy)


def dist(p, q):
    dx = p[0] - q[0]
    dy = p[1] - q[1]
    return sqrt(dx * dx + dy * dy)


# --- szinusz, koszinusz -------------------------------------------------------------------------------------

_INV_PIO2 = 6.36619772367581382433e-01
_PIO2_1 = 1.57079632673412561417e+00     # pi/2 első 33 bitje
_PIO2_1T = 6.07710050650619224932e-11    # pi/2 - _PIO2_1

_S1 = -1.66666666666666324348e-01
_S2 = 8.33333333332248946124e-03
_S3 = -1.98412698298579493134e-04
_S4 = 2.75573137070700676789e-06
_S5 = -2.50507602534068634195e-08
_S6 = 1.58969099521155010221e-10

_C1 = 4.16666666666666019037e-02
_C2 = -1.38888888888741095749e-03
_C3 = 2.48015872894767294178e-05
_C4 = -2.75573143513906633035e-07
_C5 = 2.08757232129817482790e-09
_C6 = -1.13596475577881948265e-11


def _ksin(x):
    z = x * x
    v = z * x
    r = _S2 + z * (_S3 + z * (_S4 + z * (_S5 + z * _S6)))
    return x + v * (_S1 + z * r)


def _kcos(x):
    z = x * x
    r = z * (_C1 + z * (_C2 + z * (_C3 + z * (_C4 + z * (_C5 + z * _C6)))))
    hz = 0.5 * z
    w = 1.0 - hz
    return w + (((1.0 - w) - hz) + z * r)


def _reduce(x):
    """x = k·(pi/2) + r, |r| <= ~pi/4. Pontos a játékban előforduló szögekre (|x| < 2^20)."""
    k = floor(x * _INV_PIO2 + 0.5)
    r = (x - k * _PIO2_1) - k * _PIO2_1T
    return int(k) & 3, r


def sin(x):
    n, r = _reduce(x)
    if n == 0:
        return _ksin(r)
    if n == 1:
        return _kcos(r)
    if n == 2:
        return -_ksin(r)
    return -_kcos(r)


def cos(x):
    n, r = _reduce(x)
    if n == 0:
        return _kcos(r)
    if n == 1:
        return -_ksin(r)
    if n == 2:
        return -_kcos(r)
    return _ksin(r)


# --- arkusztangens ------------------------------------------------------------------------------------------

_ATANHI = (4.63647609000806093515e-01, 7.85398163397448278999e-01,
           9.82793723247329054082e-01, 1.57079632679489655800e+00)
_ATANLO = (2.26987774529616870924e-17, 3.06161699786838301793e-17,
           1.39033110312309984516e-17, 6.12323399573676603587e-17)
_AT0 = 3.33333333333329318027e-01
_AT1 = -1.99999999998764832476e-01
_AT2 = 1.42857142725034663711e-01
_AT3 = -1.11111104054623557880e-01
_AT4 = 9.09088713343650656196e-02
_AT5 = -7.69187620504482999495e-02
_AT6 = 6.66107313738753120669e-02
_AT7 = -5.83357013379057348645e-02
_AT8 = 4.97687799461593236017e-02
_AT9 = -3.65315727442169155270e-02
_AT10 = 1.62858201153657823623e-02
_PI_LO = 1.2246467991473531772e-16
_TWO66 = 7.37869762948382064640e+19      # 2^66
_TWOM27 = 7.45058059692382812500e-09     # 2^-27


def atan(x):
    ax = -x if x < 0.0 else x
    if ax >= _TWO66:
        z = _ATANHI[3] + _ATANLO[3]
        return -z if x < 0.0 else z
    if ax < 0.4375:
        if ax < _TWOM27:
            return x
        i = -1
    elif ax < 1.1875:
        if ax < 0.6875:
            i = 0
            ax = (2.0 * ax - 1.0) / (2.0 + ax)
        else:
            i = 1
            ax = (ax - 1.0) / (ax + 1.0)
    elif ax < 2.4375:
        i = 2
        ax = (ax - 1.5) / (1.0 + 1.5 * ax)
    else:
        i = 3
        ax = -1.0 / ax
    z = ax * ax
    w = z * z
    s1 = z * (_AT0 + w * (_AT2 + w * (_AT4 + w * (_AT6 + w * (_AT8 + w * _AT10)))))
    s2 = w * (_AT1 + w * (_AT3 + w * (_AT5 + w * (_AT7 + w * _AT9))))
    if i < 0:
        return x - x * (s1 + s2)
    z = _ATANHI[i] - ((ax * (s1 + s2) - _ATANLO[i]) - ax)
    return -z if x < 0.0 else z


def atan2(y, x):
    if x != x or y != y:                # NaN
        return x + y
    if y == 0.0:
        if copysign(1.0, x) > 0.0:      # x > 0 vagy +0
            return y
        return copysign(PI, y)
    if x == 0.0:
        return copysign(0.5 * PI, y)
    z = atan(abs(y / x))
    if x > 0.0:
        return z if y > 0.0 else -z
    if y > 0.0:
        return PI - (z - _PI_LO)
    return (z - _PI_LO) - PI
