#!/usr/bin/env python3
"""Builds rano/source/levels.h from simple level descriptions.

Map legend (15 rows, row 14 is the bottom of the screen, 48px tiles):
  #  ground          B  stone block      -  wooden ledge (jump through from below)
  S  spring          ^  spikes           o  gem             h  heart
  C  lantern (checkpoint)                G  golden lotus (level goal)
  P  player start    e  beetle           w  wasp            M  moving ledge (left end, starts moving right)
  N  moving ledge that starts at the far end, moving left
"""
import os

ROWS = 15


class Level:
    def __init__(self, width):
        self.w = width
        self.g = [[' '] * width for _ in range(ROWS)]

    def ground(self, x0, x1, top=12):
        for x in range(x0, x1 + 1):
            for y in range(top, ROWS):
                self.g[y][x] = '#'

    def ceiling(self, x0, x1, bottom=1):
        for x in range(x0, x1 + 1):
            for y in range(0, bottom + 1):
                self.g[y][x] = 'B'

    def put(self, x, y, ch):
        self.g[y][x] = ch

    def row(self, x0, x1, y, ch):
        for x in range(x0, x1 + 1):
            self.g[y][x] = ch

    def rows(self):
        return [''.join(r) for r in self.g]


def level1():
    L = Level(190)
    L.put(3, 11, 'P')
    L.ground(0, 30)
    L.row(7, 10, 10, 'o')
    L.put(15, 11, 'e')
    L.ground(21, 27, 10)           # small hill
    L.row(22, 26, 8, 'o')
    # gap 31-33
    L.row(31, 33, 9, 'o')
    L.ground(34, 62)
    L.row(38, 41, 9, '-')
    L.row(38, 41, 8, 'o')
    L.put(45, 11, 'e')
    L.row(49, 50, 11, '^')
    L.row(49, 50, 9, 'o')
    L.put(55, 11, 'e')
    L.put(60, 11, 'C')
    # gap 63-65
    L.ground(66, 92, 11)
    L.put(71, 10, 'S')
    L.row(73, 79, 4, '-')
    L.row(73, 79, 3, 'o')
    L.put(80, 10, 'h')
    L.put(84, 7, 'w')
    L.put(88, 10, 'e')
    # gap 93-95
    L.row(93, 95, 8, 'o')
    L.ground(96, 113)
    L.put(101, 11, 'e')
    L.put(108, 11, 'e')
    L.ground(112, 113, 11)
    L.ground(114, 115, 10)
    L.ground(116, 126, 9)
    L.row(118, 124, 6, 'o')
    L.put(122, 5, 'w')
    L.ground(127, 152)
    L.put(132, 11, 'C')
    L.row(137, 139, 11, '^')
    L.row(142, 145, 9, 'B')
    L.row(142, 145, 8, 'o')
    L.put(148, 11, 'e')
    # big gap 153-162 with a moving ledge
    L.put(154, 10, 'M')
    L.row(156, 159, 8, 'o')
    L.ground(163, 189)
    L.put(168, 11, 'e')
    L.row(170, 174, 9, 'o')
    L.put(181, 11, 'G')
    return L


def level2():
    L = Level(200)
    L.ceiling(0, 199, 1)
    L.put(3, 11, 'P')
    L.ground(0, 24)
    L.row(8, 11, 9, 'B')
    L.row(8, 11, 8, 'o')
    L.put(14, 11, 'e')
    L.put(19, 11, 'e')
    # gap 25-27
    L.ground(28, 50)
    L.row(32, 34, 11, '^')
    L.row(32, 34, 9, 'o')
    L.put(39, 8, 'w')
    L.row(43, 46, 9, '-')
    L.row(43, 46, 8, 'o')
    L.put(48, 11, 'e')
    # gap 51-54 with a stone stepping block
    L.put(52, 10, 'B')
    L.put(53, 10, 'B')
    L.ground(55, 80, 11)
    L.put(58, 10, 'C')
    L.put(63, 10, 'e')
    L.row(66, 67, 10, '^')
    L.put(70, 10, 'e')
    L.row(73, 76, 8, '-')
    L.row(73, 76, 7, 'o')
    L.put(78, 10, 'h')
    # spike pit 81-88 crossed on ledges
    L.ground(81, 88, 13)
    L.row(81, 88, 12, '^')
    L.row(82, 83, 9, '-')
    L.row(86, 87, 9, '-')
    L.row(82, 87, 7, 'o')
    L.ground(89, 112)
    L.put(94, 11, 'e')
    L.put(98, 8, 'w')
    L.row(102, 103, 11, '^')
    L.put(108, 11, 'e')
    L.put(110, 11, 'C')
    # moving ledge over a pit 113-122
    L.put(114, 10, 'M')
    L.row(115, 120, 8, 'o')
    L.ground(123, 150)
    L.row(127, 129, 10, 'B')
    L.row(127, 129, 9, 'o')
    L.put(132, 11, 'e')
    L.put(136, 11, 'e')
    L.put(140, 8, 'w')
    L.row(144, 145, 11, '^')
    L.put(148, 11, 'S')
    L.row(150, 156, 5, '-')
    L.row(150, 156, 4, 'o')
    # gap 151-157 (spring ledge carries you over)
    L.ground(158, 199)
    L.put(162, 11, 'e')
    L.put(166, 7, 'w')
    L.row(170, 171, 11, '^')
    L.put(176, 11, 'e')
    L.row(180, 184, 9, 'o')
    L.put(190, 11, 'G')
    return L


def level3():
    L = Level(210)
    L.put(3, 11, 'P')
    L.ground(0, 16)
    L.row(8, 12, 9, 'o')
    L.put(12, 11, 'e')
    # floating ledges
    L.row(19, 22, 11, '-')
    L.row(25, 28, 9, '-')
    L.row(25, 28, 8, 'o')
    L.row(31, 34, 10, '-')
    L.ground(37, 52)
    L.put(41, 7, 'w')
    L.put(46, 11, 'e')
    L.put(50, 11, 'C')
    L.put(55, 10, 'M')
    L.row(56, 61, 8, 'o')
    L.ground(65, 80, 11)
    L.put(69, 10, 'S')
    L.row(71, 76, 3, '-')
    L.row(71, 76, 2, 'o')
    L.put(74, 10, 'e')
    L.put(78, 7, 'w')
    L.row(83, 85, 9, 'B')
    L.row(88, 90, 7, 'B')
    L.row(88, 90, 6, 'o')
    L.row(93, 95, 9, 'B')
    L.put(94, 8, 'h')
    L.ground(98, 118)
    L.put(102, 11, 'e')
    L.row(106, 107, 11, '^')
    L.put(110, 11, 'e')
    L.put(113, 7, 'w')
    L.put(116, 11, 'C')
    L.put(120, 10, 'M')
    L.put(130, 8, 'N')
    L.row(121, 136, 6, 'o')
    L.ground(140, 160, 11)
    L.put(144, 10, 'e')
    L.put(148, 10, 'e')
    L.put(151, 6, 'w')
    L.row(154, 155, 10, '^')
    L.put(158, 10, 'S')
    L.row(160, 166, 4, '-')
    L.row(160, 166, 3, 'o')
    L.row(164, 167, 10, '-')
    L.row(170, 173, 9, '-')
    L.put(172, 6, 'w')
    L.ground(176, 209)
    L.put(180, 11, 'e')
    L.put(185, 11, 'e')
    L.row(188, 189, 11, '^')
    L.row(192, 198, 9, 'o')
    L.put(201, 11, 'G')
    return L


LEVELS = [
    ("LA CHARCA", 0, level1()),
    ("LA CUEVA", 1, level2()),
    ("LAS NUBES", 2, level3()),
]

out = ["/* Generated by tools/make_levels.py - edit that file instead */",
       "#define NUM_LEVELS %d" % len(LEVELS), "",
       "typedef struct { const char *name; int theme; int width; const char *rows[15]; } LevelDef;", "",
       "static const LevelDef level_defs[NUM_LEVELS] = {"]
for name, theme, L in LEVELS:
    out.append('  { "%s", %d, %d, {' % (name, theme, L.w))
    for r in L.rows():
        out.append('    "%s",' % r)
    out.append("  } },")
out.append("};")
path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "source", "levels.h")
with open(path, "w") as f:
    f.write("\n".join(out) + "\n")
print("wrote", path)
