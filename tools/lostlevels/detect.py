#!/usr/bin/env python3
"""Find the general layout of a Bonanza Bros. arcade stage in a map picture.

  python3 tools/lostlevels/detect.py MAP.png [out.json] [overlay.png]

Finds, from the picture alone:
  * floors: the light floor slabs that run across the whole building (rows where most
    of the width is near-white), so the bands between them are the floors, top first;
  * the building's left and right ends on each floor;
  * stairs: the arcade draws them as white steps on a diagonal: in a floor's band, the top
    edge of the white goes up (or down) about one pixel per pixel over a long stretch.
    rising +x = up to the right ("dir 2" in the game's data), -x = up to the left ("dir 1");
  * walls/pillars: narrow white columns from the floor's ceiling to its floor.
Writes JSON (all in picture pixels) and, if asked, the picture with what was found drawn
on it. Uses Pillow only.
"""
import json
import sys
from PIL import Image, ImageDraw


def light(p):
    r, g, b = p[:3]
    return r > 225 and g > 225 and b > 225 and max(r, g, b) - min(r, g, b) < 24


def detect(path):
    im = Image.open(path).convert('RGB')
    W, H = im.size
    px = im.load()
    # floors: slab rows = most of the width light
    pale = lambda p: sum(p[:3]) > 640                 # slabs can be tinted (the roof's is pink)
    slab = [sum(1 for x in range(0, W, 2) if pale(px[x, y])) * 2 / W > 0.6 for y in range(H)]
    slabs = []                                   # [y0, y1] of each slab
    y = 0
    while y < H:
        if slab[y]:
            y0 = y
            while y < H and slab[y]:
                y += 1
            if y - y0 >= 4:
                slabs.append([y0, y - 1])
        y += 1
    # floor bands: from a slab's bottom to the next slab's top (the first band: from the top
    # of the building down to the first slab; the last: down to the ground)
    tops = [0] + [b + 1 for _, b in slabs]
    bottoms = [a - 1 for a, _ in slabs]
    # the ground under the last floor: the last row that still has building in it
    ground = max(y for y in range(H) if sum(1 for x in range(0, W, 4) if sum(px[x, y][:3]) > 150) > W / 16)
    bands = []
    for t, b in zip(tops, bottoms + [ground]):
        if b - t > 60:
            bands.append([t, b])
    floors = []
    for fi, (t, b) in enumerate(bands):
        h = b - t
        # building ends on this floor: columns with content in the band's lower half
        cols = [x for x in range(W) if sum(1 for y in range(t + h // 2, b, 3) if sum(px[x, y]) > 120) > h // 8]
        x0, x1 = (min(cols), max(cols)) if cols else (0, W - 1)
        # top of the white (with white below it for a few pixels) in each column
        top = []
        for x in range(W):
            v = None
            for y in range(t + 2, b - 2):
                if light(px[x, y]) and all(light(px[x, y + k]) for k in (3, 6)):
                    v = y
                    break
            top.append(v)
        # walls/pillars: white from (near) the ceiling to (near) the floor, narrow
        tall = [x for x in range(W) if top[x] is not None and top[x] < t + h * 0.15 and
                sum(1 for y in range(t, b, 3) if light(px[x, y])) > (b - t) / 3 * 0.85]
        walls = []
        for x in tall:
            if walls and x - walls[-1][1] <= 2:
                walls[-1][1] = x
            else:
                walls.append([x, x])
        walls = [[a, c] for a, c in walls if c - a <= 48]
        # stairs: long runs where the white's top edge moves ~1 px per px
        stairs = []
        x = x0
        while x < x1:
            best = None
            for sgn in (-1, 1):
                # follow the edge from x while it keeps going the same way, allowing steps
                xs, ys = [], []
                k = x
                last = top[x]
                if last is None:
                    break
                while k < x1 and top[k] is not None and abs(top[k] - last) <= 18:
                    d = top[k] - last
                    if d * sgn < -2:           # going the other way: stop
                        break
                    xs.append(k)
                    ys.append(top[k])
                    last = top[k]
                    k += 1
                if len(xs) > 60:
                    rise = (ys[-1] - ys[0]) * sgn
                    slope = rise / (xs[-1] - xs[0])
                    if rise > h * 0.45 and 0.45 < slope < 1.8:
                        cand = (xs[0], xs[-1], sgn, rise)
                        if not best or cand[1] - cand[0] > best[1] - best[0]:
                            best = cand
            if best:
                a, c, sgn, rise = best
                # top edge going down to the right (sgn 1) = the steps rise to the left
                stairs.append({'x0': a, 'x1': c, 'dir': 1 if sgn > 0 else 2, 'rise': rise})
                x = c + 1
            else:
                x += 2
        floors.append({'top': t, 'bottom': b, 'x0': x0, 'x1': x1,
                       'walls': [(a + c) // 2 for a, c in walls], 'stairs': stairs})
    return {'image': path, 'width': W, 'height': H, 'slabs': slabs, 'floors': floors}


def overlay(res, src, out):
    im = Image.open(src).convert('RGB')
    d = ImageDraw.Draw(im)
    for f in res['floors']:
        d.rectangle([f['x0'], f['top'], f['x1'], f['bottom']], outline=(255, 255, 0), width=2)
        for x in f['walls']:
            d.line([x, f['top'], x, f['bottom']], fill=(255, 0, 255), width=3)
        for s in f['stairs']:
            y0, y1 = (f['bottom'], f['top']) if s['dir'] == 2 else (f['top'], f['bottom'])
            d.line([s['x0'], y0, s['x1'], y1], fill=(0, 220, 255), width=5)
            d.text((s['x0'], f['top'] + 4), 'UP ' + ('>' if s['dir'] == 2 else '<'), fill=(0, 0, 0))
    im.save(out)


if __name__ == '__main__':
    r = detect(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else None
    if out:
        json.dump(r, open(out, 'w'), indent=1)
    if len(sys.argv) > 3:
        overlay(r, sys.argv[1], sys.argv[3])
    for i, f in enumerate(r['floors']):
        print('floor %d (top first): y %d-%d, x %d-%d, %d walls, stairs %s' % (
            i, f['top'], f['bottom'], f['x0'], f['x1'], len(f['walls']),
            ', '.join('%d-%d up%s' % (s['x0'], s['x1'], '>' if s['dir'] == 2 else '<') for s in f['stairs'])))
