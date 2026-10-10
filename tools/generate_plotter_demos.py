"""Generate center-returning single-path SVG demos and matching flash point tables.

All feature-to-feature travel is deliberately included as visible line work because
this plotter has no pen lift. The MCU consumes only generated integer tables.
"""
import math
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class PathBuilder:
    def __init__(self):
        self.points = [(0, 0)]

    def add(self, point):
        point = (round(point[0]), round(point[1]))
        if point != self.points[-1]:
            self.points.append(point)

    def route(self, points):
        for point in points:
            self.add(point)

    def loop(self, points):
        if points:
            self.route(points)
            self.add(points[0])

    def cubic_points(self, p0, p1, p2, p3, count=18):
        points = [p0]
        for i in range(1, count + 1):
            t = i / count
            u = 1 - t
            points.append((u**3*p0[0] + 3*u*u*t*p1[0] + 3*u*t*t*p2[0] + t**3*p3[0],
                           u**3*p0[1] + 3*u*u*t*p1[1] + 3*u*t*t*p2[1] + t**3*p3[1]))
        return points

    def cubic(self, p0, p1, p2, p3, count=18):
        self.route(self.cubic_points(p0,p1,p2,p3,count))

    def branch(self, points):
        """Trace a detail from the current point and retrace it to stay one-stroke."""
        points = list(points)
        if not points:
            return
        anchor = self.points[-1]
        if points[0] != anchor:
            points.insert(0, anchor)
        self.route(points[1:])
        self.route(reversed(points[1:-1]))
        self.add(anchor)

    def branch_cubic(self, control1, control2, end, count=12):
        start = self.points[-1]
        self.branch(self.cubic_points(start, control1, control2, end, count))

    def branch_from_baseline(self, baseline, points):
        """Reach a detail by retracing the already-visible main contour."""
        points = list(points)
        if not points:
            return
        hint = points[0]
        anchor_index = min(range(len(baseline)),
                           key=lambda i: (baseline[i][0]-hint[0])**2 + (baseline[i][1]-hint[1])**2)
        anchor = baseline[anchor_index]
        self.route(baseline[1:anchor_index+1])
        points[0] = anchor
        self.route(points[1:])
        self.route(reversed(points[1:-1]))
        self.route(reversed(baseline[1:anchor_index+1]))

    def branch_cubic_from_baseline(self, baseline, hint, control1, control2, end, count=12):
        anchor_index = min(range(len(baseline)),
                           key=lambda i: (baseline[i][0]-hint[0])**2 + (baseline[i][1]-hint[1])**2)
        anchor = baseline[anchor_index]
        self.branch_from_baseline(baseline, self.cubic_points(anchor, control1, control2, end, count))

    def ellipse(self, cx, cy, rx, ry, count=32, angle=0):
        points = []
        ca, sa = math.cos(angle), math.sin(angle)
        for i in range(count):
            t = 2 * math.pi * i / count
            x, y = rx * math.cos(t), ry * math.sin(t)
            points.append((cx + x*ca - y*sa, cy + x*sa + y*ca))
        self.loop(points)

    def close_at_origin(self):
        self.add((0, 0))
        assert self.points[0] == self.points[-1] == (0, 0)
        assert len(self.points) <= 2048, len(self.points)
        assert all(-2800 <= x <= 2800 and -1840 <= y <= 1840 for x, y in self.points)
        return self.points


def offset(points, cx, cy, scale=1.0):
    return [(cx + x*scale, cy + y*scale) for x, y in points]


def flower():
    b = PathBuilder()
    for i in range(1501):
        t = 2 * math.pi * i / 1500
        radius = math.sin(6*t) * (0.82 + 0.18*math.cos(24*t))
        b.add((2800*radius*math.cos(t), 1840*radius*math.sin(t)))
    return b.close_at_origin()


def carrier():
    b = PathBuilder()
    b.loop([(-2600,520),(-2250,680),(2450,680),(2580,540),(2350,220),
            (1880,-520),(1500,-820),(-1880,-820),(-2260,-500),(-2600,520)])
    # flight deck edge, runway lanes, catapult and landing markings
    b.route([(-2250,555),(2200,555)])
    b.route([(-1850,410),(1750,410)])
    b.route([(1950,370),(2240,270)])
    for x in (-1750,-1300,-850,-400,50,500,950,1400):
        b.route([(x,480),(x+130,455)])
    b.route([(-2050,300),(-1820,260),(-1650,300),(-1820,340),(-2050,300)])
    # island / bridge / radar mast
    b.loop([(650,680),(720,930),(900,1030),(1350,1030),(1470,920),(1450,680)])
    b.route([(790,780),(1430,780)])
    b.route([(860,930),(1320,930)])
    for x in (900,1010,1120,1230,1340):
        b.route([(x,805),(x+35,805)])
    b.route([(1070,1030),(1080,1440),(920,1470),(1230,1485),(1080,1440)])
    b.ellipse(1080,1530,170,22,24)
    b.route([(930,1330),(1230,1330),(1070,1380),(930,1330)])
    # parked deck aircraft, each a top-view continuous silhouette
    plane=[(0,180),(70,30),(330,-20),(360,-80),(90,-60),(75,-260),(180,-330),
           (180,-380),(0,-340),(-180,-380),(-180,-330),(-75,-260),(-90,-60),
           (-360,-80),(-330,-20),(-70,30),(0,180)]
    b.loop(offset(plane,-1150,100,0.42))
    b.loop(offset(plane,-250,80,0.35))
    b.loop(offset(plane,1550,40,0.35))
    # stern prop wash and hull windows
    b.route([(1800,-650),(2150,-500),(2300,-300)])
    for x in (-1700,-1300,-900,-500,-100,300,700,1100,1450):
        b.ellipse(x,-700,28,20,12)
    b.route([(-450,680),(-350,820),(-250,680)])
    return b.close_at_origin()


def fighter():
    b = PathBuilder()
    # outer planform: pointed nose, swept wings, tailplanes and twin tail fins
    b.loop([(0,1740),(130,1480),(190,950),(420,500),(2480,110),(2520,-80),
            (2130,-170),(750,-5),(680,-680),(1120,-1020),(1080,-1120),(180,-990),
            (0,-1510),(-180,-990),(-1080,-1120),(-1120,-1020),(-680,-680),
            (-750,-5),(-2130,-170),(-2520,-80),(-2480,110),(-420,500),
            (-190,950),(-130,1480),(0,1740)])
    # canopy, fuselage, wing panels, engine and stabilizer detail
    b.ellipse(0,800,150,430,38)
    b.route([(0,1500),(0,-1370)])
    b.route([(-440,430),(-1950,80),(-780,200)])
    b.route([(440,430),(1950,80),(780,200)])
    b.route([(-610,-360),(-310,-240),(0,-190),(310,-240),(610,-360)])
    b.route([(-940,-880),(-350,-690),(0,-720),(350,-690),(940,-880)])
    b.ellipse(-220,-910,105,160,24)
    b.ellipse(220,-910,105,160,24)
    b.route([(-470,1090),(-250,990),(0,970),(250,990),(470,1090)])
    for side in (-1,1):
        b.route([(side*520,300),(side*950,170),(side*1260,135)])
        b.route([(side*760,-560),(side*990,-640),(side*1050,-850)])
    # guidance lines / pylons
    for x in (-1500,-1150,1150,1500):
        b.route([(x,10),(x+(-120 if x<0 else 120),-35)])
    return b.close_at_origin()


def character():
    b = PathBuilder()
    # original full-body standing heroine, wide pose for landscape board
    silhouette=[(0,1690),(-210,1570),(-500,1620),(-680,1450),(-620,1220),(-920,1090),
      (-1480,1320),(-1810,1190),(-1570,900),(-1050,720),(-700,780),(-550,650),
      (-760,400),(-1150,240),(-1550,210),(-1180,100),(-850,-10),(-730,-330),
      (-890,-950),(-1100,-1500),(-780,-1580),(-390,-1500),(-300,-940),(-120,-570),
      (0,-430),(120,-570),(300,-940),(390,-1500),(780,-1580),(1100,-1500),
      (890,-950),(730,-330),(850,-10),(1180,100),(1550,210),(1150,240),(760,400),
      (550,650),(700,780),(1050,720),(1570,900),(1810,1190),(1480,1320),(920,1090),
      (620,1220),(680,1450),(500,1620),(210,1570),(0,1690)]
    b.loop(silhouette)
    # hair twin tails and ribbons
    b.loop([(-480,1330),(-840,1510),(-1210,1640),(-1110,1350),(-970,1100),(-700,1040)])
    b.loop([(480,1330),(840,1510),(1210,1640),(1110,1350),(970,1100),(700,1040)])
    b.route([(-1000,1500),(-1350,1610),(-1190,1300),(-1000,1500)])
    b.route([(1000,1500),(1350,1610),(1190,1300),(1000,1500)])
    # face, eyes, brows, nose, smile and bangs
    b.ellipse(0,1250,390,390,40)
    for side in (-1,1):
        b.ellipse(side*170,1280,68,92,24)
        b.ellipse(side*170,1280,23,45,18)
        b.route([(side*95,1435),(side*205,1470),(side*290,1435)])
    b.route([(0,1260),(-42,1180),(0,1160),(42,1180)])
    b.cubic((-105,1080),(-30,1030),(30,1030),(105,1080),10)
    b.route([(-390,1450),(-240,1550),(-100,1440),(30,1580),(170,1430),(330,1510),(390,1390)])
    # collar, necktie, chest bow and dress seam
    b.route([(-300,900),(-180,720),(0,800),(180,720),(300,900)])
    b.route([(-160,720),(0,520),(160,720),(0,770),(-160,720)])
    b.route([(0,520),(-75,350),(0,110),(75,350),(0,520)])
    b.route([(-520,600),(-360,300),(0,220),(360,300),(520,600)])
    b.route([(-680,240),(-500,-100),(-350,-250),(0,-330),(350,-250),(500,-100),(680,240)])
    # skirt pleats, stockings, boot details
    for x in (-520,-330,-150,150,330,520):
        b.route([(x,80),(x*0.75,-260),(x*0.85,-330)])
    b.route([(-320,-930),(-80,-880),(0,-900),(80,-880),(320,-930)])
    b.route([(-360,-1390),(-170,-1360),(0,-1390),(170,-1360),(360,-1390)])
    b.route([(-780,-1540),(-500,-1510),(-400,-1570),(-780,-1580)])
    b.route([(780,-1540),(500,-1510),(400,-1570),(780,-1580)])
    # arms, cuffs and gloves
    b.route([(-650,600),(-950,520),(-1250,640),(-1500,850)])
    b.route([(650,600),(950,520),(1250,640),(1500,850)])
    for x in (-1500,1500):
        b.ellipse(x,850,115,135,24)
        for j in range(3):
            b.route([(x+j*35-35,900),(x+j*40-45,1040)])
    return b.close_at_origin()


def dragon():
    b = PathBuilder()
    # coiled serpentine torso and tapering tail
    b.cubic((0,0),(-1250,-250),(-1800,650),(-900,850),24)
    b.cubic((-900,850),(0,1080),(1000,620),(620,250),24)
    b.cubic((620,250),(300,-120),(-450,180),(-300,530),20)
    b.cubic((-300,530),(-100,820),(380,700),(600,850),18)
    b.cubic((600,850),(900,1040),(1150,1190),(1450,1050),18)
    b.cubic((1450,1050),(1680,960),(1850,1010),(2070,1120),16)
    # dragon head, muzzle, horns, jaw, whiskers
    b.loop([(1460,1090),(1520,1420),(1770,1580),(2020,1460),(2240,1490),
            (2140,1320),(2360,1240),(2140,1110),(2070,920),(1850,870),
            (1690,980),(1460,1090)])
    b.route([(1740,1450),(1630,1740),(1850,1560)])
    b.route([(1950,1450),(2160,1730),(2070,1430)])
    b.ellipse(1990,1320,38,52,20)
    b.route([(2250,1260),(2570,1350),(2320,1190),(2600,1110),(2250,1120)])
    b.cubic((1790,930),(2020,760),(2260,820),(2400,1010),16)
    b.route([(1800,980),(2100,980),(2260,1030)])
    # wings as two membrane fans
    b.loop([(880,760),(1150,1200),(1030,1730),(1390,1450),(1580,1690),
            (1570,1230),(1760,1420),(1660,1110),(1450,950),(880,760)])
    b.route([(1150,1200),(1420,1210),(1570,1230)])
    b.route([(1030,1730),(1350,1320),(1580,1690)])
    b.loop([(-450,650),(-850,1180),(-790,1670),(-1180,1410),(-1380,1600),
            (-1320,1170),(-1530,1320),(-1410,1030),(-1120,850),(-450,650)])
    b.route([(-850,1180),(-1120,1140),(-1320,1170)])
    b.route([(-790,1670),(-1060,1290),(-1380,1600)])
    # legs, talons, back spines and scales
    b.route([(250,20),(650,-250),(930,-300),(1110,-120),(1000,-430),(690,-540),(450,-300)])
    b.route([(-950,680),(-1250,420),(-1540,490),(-1660,720),(-1450,760),(-1390,600)])
    for i in range(9):
        x=420+i*120
        b.route([(x,900-i*18),(x+80,1040-i*15),(x+150,960-i*15)])
    for i in range(4):
        b.ellipse(-650+i*370,835-i*35,75,45,18)
    b.route([(-1640,670),(-1740,600),(-1800,680),(-1740,735)])
    return b.close_at_origin()


def heart():
    b = PathBuilder()
    # anatomical/mechanical heart outer contour, shaped with sampled cubics
    b.cubic((0,-1550),(-330,-1280),(-1950,-200),(-1950,680),28)
    b.cubic((-1950,680),(-1950,1450),(-850,1710),(0,930),26)
    b.cubic((0,930),(850,1710),(1950,1450),(1950,680),26)
    b.cubic((1950,680),(1950,-200),(330,-1280),(0,-1550),28)
    # aortic arch and vessels
    b.cubic((-100,850),(-250,1250),(-450,1700),(-100,1770),20)
    b.cubic((-100,1770),(260,1810),(380,1540),(180,1320),18)
    b.cubic((180,1320),(0,1120),(160,960),(430,900),16)
    b.cubic((430,900),(760,1030),(970,1370),(1190,1540),20)
    b.cubic((1190,1540),(1430,1710),(1690,1570),(1540,1350),16)
    # chambers, valves, center gear and radial teeth
    b.loop([(-1180,700),(-920,1060),(-430,900),(-290,390),(-660,-230),(-1120,80),(-1180,700)])
    b.loop([(1180,700),(920,1060),(430,900),(290,390),(660,-230),(1120,80),(1180,700)])
    b.ellipse(0,300,420,420,48)
    b.ellipse(0,300,250,250,36)
    b.ellipse(0,300,75,75,24)
    for i in range(12):
        t=2*math.pi*i/12
        b.route([(330*math.cos(t),300+330*math.sin(t)),
                 (475*math.cos(t),300+475*math.sin(t))])
    b.route([(-1100,-300),(-650,-570),(-300,-520),(0,-720),(300,-520),(650,-570),(1100,-300)])
    b.route([(-650,800),(-280,600),(0,760),(280,600),(650,800)])
    # branching arteries and clockwork connectors
    for side in (-1,1):
        b.cubic((side*250,780),(side*500,1110),(side*760,1250),(side*1030,1420),14)
        b.route([(side*1380,900),(side*1650,1020),(side*1740,760)])
        b.ellipse(side*1350,390,150,150,28)
        b.route([(side*1200,390),(side*1500,390)])
        b.route([(side*1350,240),(side*1350,540)])
    b.route([(-220,-1000),(0,-840),(220,-1000)])
    return b.close_at_origin()


def together():
    """Convert the curated kiss-portrait trace into uniform motor coordinates."""
    source = ROOT / 'assets' / 'one-stroke-together-trace.svg'
    root = ET.parse(source).getroot()
    path = root.find('.//{http://www.w3.org/2000/svg}path')
    if path is None:
        raise ValueError(f'{source} must contain a traced SVG path')

    data = path.attrib.get('d', '')
    matches = re.findall(r'([ML])\s*(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)', data)
    residue = re.sub(r'[ML]\s*-?\d+(?:\.\d+)?,-?\d+(?:\.\d+)?', '', data)
    if not matches or residue.strip() or matches[0][0] != 'M' or sum(c == 'M' for c, _, _ in matches) != 1:
        raise ValueError(f'{source} must be one continuous M/L polyline')

    # The reference starts/ends where the profiles meet. Keep equal X/Y scale
    # so the preview and the physical drawing preserve the source proportions.
    start = (float(matches[0][1]), float(matches[0][2]))
    finish = (float(matches[-1][1]), float(matches[-1][2]))
    if start != finish:
        raise ValueError(f'{source} must return to its start point')

    b = PathBuilder()
    b.route((round((float(x) - 235.0) * 7.0),
             round((178.0 - float(y)) * 7.0))
            for _, x, y in matches)
    return b.close_at_origin()


def sail_badge():
    b = PathBuilder()
    # Badge outer rim; this and every subsequent detail remains one connected route.
    ring = []
    for i in range(181):
        angle = math.pi / 2 + 2 * math.pi * i / 180
        ring.append((1770 * math.cos(angle), 1770 * math.sin(angle)))
    b.loop(ring)
    # Simplified calligraphic/ring lettering: stylized strokes, not filled type.
    b.route([(0,1770),(110,1510),(35,1300),(-90,1500),(10,1680),(130,1440),
             (380,1580),(500,1360),(420,1180),(310,1390),(500,1540),
             (720,1450),(860,1250),(770,1130),(660,1340),(850,1430),
             (1050,1240),(1190,1320),(1080,1150),(970,1240),
             (1390,850),(1210,900),(1430,1010),(1370,1130),(1240,1080),
             (1510,410),(1330,480),(1550,520),(1470,650),(1340,630)])
    # Lower ring initials T U S, drawn as a single outline-like monoline.
    b.route([(1320,-1050),(1580,-1370),(1280,-1190),(1460,-1450),
             (1040,-1510),(880,-1340),(1010,-1200),(1170,-1390),
             (620,-1600),(420,-1360),(650,-1270),(730,-1570),
             (60,-1650),(60,-1370),(-40,-1370),(160,-1370),
             (160,-1650),(150,-1370),(170,-1370),
             (-420,-1600),(-620,-1330),(-420,-1300),(-520,-1450),(-680,-1430),
             (-570,-1580),(-390,-1560)])
    # Inner medallion and stylized two-tone sail silhouette.
    inner = []
    for i in range(145):
        angle = math.pi / 2 + 2 * math.pi * i / 144
        inner.append((1080 * math.cos(angle), 1080 * math.sin(angle)))
    b.loop(inner)
    b.loop([(-610,-610),(-470,390),(400,960),(160,-50),(-610,-610)])
    b.route([(-250,-540),(390,780),(610,-180),(250,-285),(-250,-540)])
    b.ellipse(-320,720,82,82,24)
    # Hull/wave swoosh and a small wake line.
    b.cubic((-760,-480),(-260,-210),(290,-185),(900,-330),26)
    b.cubic((-760,-480),(-260,-730),(300,-740),(900,-330),24)
    b.route([(-560,-590),(-260,-760),(80,-795),(500,-670)])
    # Compact single-stroke 1933 date mark.
    digits = [
        [(-650,-900),(-520,-810),(-520,-1110),(-690,-1110),(-390,-1110)],
        [(-300,-820),(-80,-820),(-80,-960),(-280,-960),(-300,-1110),(-60,-1110)],
        [(20,-820),(250,-820),(250,-960),(30,-960),(250,-960),(250,-1110),(20,-1110)],
        [(340,-820),(570,-820),(570,-960),(360,-960),(570,-960),(570,-1110),(340,-1110)]
    ]
    for digit in digits:
        b.route(digit)
    return b.close_at_origin()


DEMOS = [
    ('flower', 'Flower', flower, '#087da8'),
    ('carrier', 'Aircraft Carrier', carrier, '#136c8c'),
    ('fighter', 'Fighter Jet', fighter, '#8a4ca8'),
    ('character', 'Original Standing Character', character, '#cf4d78'),
    ('dragon', 'Dragon', dragon, '#25845d'),
    ('heart', 'Mechanical Heart', heart, '#c4493e'),
    ('sail-badge', '1933 Sail Badge', sail_badge, '#078ecb'),
    ('together', 'Together', together, '#111111'),
]

for key, title, make, color in DEMOS:
    points = make()
    if key == 'character':
        # Keep the original standing pose, but spread it laterally for the wide canvas.
        points = [(round(x * 1.35), round(y * 0.90)) for x, y in points]
    inc = ROOT / f'User/plotter/plotter_{key}_points.inc'
    inc.write_text(f'/* Generated by tools/generate_plotter_demos.py; {len(points)} points in motor steps. */\n' +
                   ',\n'.join(f'    {{{x}, {y}}}' for x, y in points) + ',\n')
    commands = ' '.join(f'{"M" if i == 0 else "L"} {x},{y}' for i, (x,y) in enumerate(points))
    svg = (ROOT / 'assets' / f'one-stroke-{key}.svg')
    svg.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-3500 -2300 7000 4600" width="1000" height="657">\n'
                   f'  <title>{title} — one continuous center-returning stroke</title>\n'
                   f'  <g transform="scale(1,-1)"><path d="{commands}" fill="none" stroke="{color}" stroke-width="18" stroke-linecap="round" stroke-linejoin="round"/></g>\n'
                   '</svg>\n')
    print(f'{key}: {len(points)} points')
