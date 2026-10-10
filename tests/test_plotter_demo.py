from pathlib import Path
import subprocess
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEMO_ASSETS = {
    'FLOWER': ('one-stroke-flower.svg', 'flower'),
    'CARRIER': ('one-stroke-carrier.svg', 'carrier'),
    'FIGHTER': ('one-stroke-fighter.svg', 'fighter'),
    'CHARACTER': ('one-stroke-character.svg', 'character'),
    'DRAGON': ('one-stroke-dragon.svg', 'dragon'),
    'HEART': ('one-stroke-heart.svg', 'heart'),
    'SAIL BADGE': ('one-stroke-sail-badge.svg', 'sail-badge'),
    'TOGETHER': ('one-stroke-together.svg', 'together'),
    'SAIL BADGE': ('one-stroke-sail-badge.svg', 'sail-badge'),
    'TOGETHER': ('one-stroke-together.svg', 'together'),
}


def test_all_demos_are_continuous_bounded_and_return_to_center(tmp_path):
    c = tmp_path / 'demo.c'
    c.write_text(r'''
#include <assert.h>
#include <string.h>
#include "plotter_demo.h"
int main(void) {
    PlotterPath_t path;
    PlotterCanvas_t canvas = {24,220,433,285};
    PlotterDda_t dda;
    unsigned demo, i;
    assert(PlotterDemo_Count() == PLOTTER_DEMO_COUNT && PLOTTER_DEMO_COUNT == 8);
    assert(!PlotterDemo_Load(0, &canvas, PLOTTER_DEMO_FLOWER));
    assert(!PlotterDemo_Load(&path, 0, PLOTTER_DEMO_FLOWER));
    for (demo=0; demo<PLOTTER_DEMO_COUNT; ++demo) {
        uint8_t mask;
        int8_t dx, dy;
        int x=0, y=0;
        unsigned events=0;
        assert(PlotterDemo_Name((PlotterDemoId_t)demo)[0] != '\0');
        assert(PlotterDemo_Load(&path, &canvas, (PlotterDemoId_t)demo));
        assert(path.state == PLOTTER_PATH_READY && path.count > 100 && path.count <= 2048);
        assert(path.points[0].x_steps == 0 && path.points[0].y_steps == 0);
        assert(path.points[path.count-1].x_steps == 0 && path.points[path.count-1].y_steps == 0);
        for(i=0;i<path.count;i++) {
            assert(path.points[i].x_steps >= -2800 && path.points[i].x_steps <= 2800);
            assert(path.points[i].y_steps >= -1840 && path.points[i].y_steps <= 1840);
        }
        PlotterDda_Begin(&dda,&path);
        while(PlotterDda_Next(&dda,&mask,&dx,&dy)) {
            if(mask & PLOTTER_AXIS_X) x+=dx;
            if(mask & PLOTTER_AXIS_Y) y+=dy;
            assert(x>=-2800 && x<=2800 && y>=-1840 && y<=1840);
            assert(++events < 500000);
        }
        assert(events > 1000 && x==0 && y==0);
    }
    return 0;
}
''')
    exe = tmp_path / 'demo'
    result = subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I',str(ROOT/'User/plotter'),str(c),str(ROOT/'User/plotter/plotter_demo.c'),str(ROOT/'User/plotter/plotter_path.c'),'-o',str(exe)],capture_output=True,text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(exe)],capture_output=True,text=True)
    assert result.returncode == 0, result.stderr


def test_svg_and_firmware_point_tables_match_for_every_demo():
    for name, (filename, slug) in DEMO_ASSETS.items():
        svg = ET.parse(ROOT/'assets'/filename)
        paths = svg.findall('.//{http://www.w3.org/2000/svg}path')
        assert len(paths) == 1, filename
        data = paths[0].attrib['d']
        assert data.count('M') == 1 and 'Z' not in data, filename
        svg_points = [(int(x), int(y)) for x,y in re.findall(r'[ML]\s*(-?\d+),(-?\d+)', data)]
        table = (ROOT/f'User/plotter/plotter_{slug}_points.inc').read_text()
        firmware_points = [(int(x),int(y)) for x,y in re.findall(r'\{\s*(-?\d+),\s*(-?\d+)\s*\}',table)]
        assert svg_points == firmware_points
        assert svg_points[0] == svg_points[-1] == (0,0)
        if name == 'TOGETHER':
            x_span = max(x for x, _ in svg_points) - min(x for x, _ in svg_points)
            y_span = max(y for _, y in svg_points) - min(y for _, y in svg_points)
            assert x_span > y_span, 'couple illustration should use a horizontal composition'
            assert len(svg_points) > 250, 'couple drawing should preserve face and hair detail'
        if name == 'CHARACTER':
            x_span = max(x for x, _ in svg_points) - min(x for x, _ in svg_points)
            y_span = max(y for _, y in svg_points) - min(y for _, y in svg_points)
            assert x_span > y_span, 'full-body character should be laid out laterally'


def test_together_geometry_tracks_the_reference_portrait_composition():
    svg = ET.parse(ROOT/'assets/one-stroke-together.svg')
    path = svg.find('.//{http://www.w3.org/2000/svg}path')
    data = path.attrib['d']
    points = [(int(x), int(y)) for x,y in re.findall(r'[ML]\s*(-?\d+),(-?\d+)', data)]
    x_span = max(x for x, _ in points) - min(x for x, _ in points)
    y_span = max(y for _, y in points) - min(y for _, y in points)
    upper_extent = max(y for _, y in points)
    lower_extent = abs(min(y for _, y in points))

    # In the reference, the couple occupies a near-portrait silhouette and
    # the joined profiles sit above the longer shoulder/body contours.
    assert 1.08 <= x_span / y_span <= 1.30
    assert 0.48 <= upper_extent / lower_extent <= 0.70


def test_together_firmware_path_is_generated_from_curated_reference_trace():
    source = ET.parse(ROOT/'assets/one-stroke-together-trace.svg')
    source_paths = source.findall('.//{http://www.w3.org/2000/svg}path')
    assert len(source_paths) == 1
    source_data = source_paths[0].attrib['d']
    source_points = [(float(x), float(y)) for x,y in
                     re.findall(r'[ML]\s*(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)', source_data)]
    assert source_points[0] == source_points[-1] == (235.0, 178.0)

    expected = []
    for x, y in source_points:
        point = (round((x - 235.0) * 7.0), round((178.0 - y) * 7.0))
        if not expected or point != expected[-1]:
            expected.append(point)

    output = ET.parse(ROOT/'assets/one-stroke-together.svg')
    path = output.find('.//{http://www.w3.org/2000/svg}path')
    output_points = [(int(x), int(y)) for x,y in
                     re.findall(r'[ML]\s*(-?\d+),(-?\d+)', path.attrib['d'])]
    assert output_points == expected
