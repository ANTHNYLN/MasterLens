#!/usr/bin/env python3
"""Generates docs/logo.svg (the same geometry as Source/ui/Logo.cpp)."""

import math

CX = CY = 512.0
R = 312.0


def point(angle):
    return CX + R * math.cos(angle), CY + R * math.sin(angle)


a0, a1 = 0.42, 2.0 * math.pi - 0.68
x0, y0 = point(a0)
x1, y1 = point(a1)
ring = f"M {x0:.1f} {y0:.1f} A {R:.0f} {R:.0f} 0 1 1 {x1:.1f} {y1:.1f}"

b0, b1 = -0.92, -0.18
bx0, by0 = point(b0)
bx1, by1 = point(b1)
accent = f"M {bx0:.1f} {by0:.1f} A {R:.0f} {R:.0f} 0 0 1 {bx1:.1f} {by1:.1f}"

blades = []
for i in range(6):
    angle = i * math.pi / 3.0 + 0.36
    r0, r1 = R * 0.62, R * 0.96
    blades.append(
        f'<line x1="{CX + r0 * math.cos(angle):.1f}" y1="{CY + r0 * math.sin(angle):.1f}" '
        f'x2="{CX + r1 * math.cos(angle):.1f}" y2="{CY + r1 * math.sin(angle):.1f}"/>'
    )

berry_r = 0.078 * 1024.0
stem_x = CX - 0.010 * 1024.0
stem_y = CY - 0.196 * 1024.0
stem_w = 0.020 * 1024.0
stem_h = 0.070 * 1024.0
leaf_x = CX + 0.020 * 1024.0
leaf_y = CY - 0.212 * 1024.0
leaf_w = 0.105 * 1024.0
leaf_h = 0.042 * 1024.0
leaf_cx = leaf_x + leaf_w / 2.0
leaf_cy = leaf_y + leaf_h / 2.0

svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" role="img" aria-label="MasterLens by BerryStudio">
  <defs>
    <linearGradient id="brand" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#5b9bff"/>
      <stop offset="1" stop-color="#8b7bff"/>
    </linearGradient>
    <linearGradient id="sheen" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ffffff" stop-opacity="0.22"/>
      <stop offset="0.55" stop-color="#ffffff" stop-opacity="0"/>
    </linearGradient>
  </defs>
  <rect width="1024" height="1024" rx="261" fill="url(#brand)"/>
  <rect x="20" y="20" width="984" height="492" rx="240" fill="url(#sheen)"/>
  <path d="{ring}" fill="none" stroke="#ffffff" stroke-opacity="0.94" stroke-width="53" stroke-linecap="round"/>
  <g stroke="#ffffff" stroke-opacity="0.30" stroke-width="27" stroke-linecap="round">
    {''.join(blades)}
  </g>
  <g fill="#ffffff">
    <circle cx="{CX}" cy="{CY - 0.062 * 1024:.1f}" r="{berry_r:.2f}"/>
    <circle cx="{CX - 0.060 * 1024:.1f}" cy="{CY + 0.054 * 1024:.1f}" r="{berry_r:.2f}"/>
    <circle cx="{CX + 0.060 * 1024:.1f}" cy="{CY + 0.054 * 1024:.1f}" r="{berry_r:.2f}"/>
    <rect x="{stem_x:.1f}" y="{stem_y:.1f}" width="{stem_w:.1f}" height="{stem_h:.1f}" rx="{stem_w / 2:.1f}"/>
    <ellipse cx="{leaf_cx:.1f}" cy="{leaf_cy:.1f}" rx="{leaf_w / 2:.1f}" ry="{leaf_h / 2:.1f}"
             transform="rotate(-33.2 {leaf_cx:.1f} {leaf_cy:.1f})"/>
  </g>
  <path d="{accent}" fill="none" stroke="#46c8d8" stroke-width="53" stroke-linecap="round"/>
</svg>
'''

with open("docs/logo.svg", "w", encoding="utf-8") as handle:
    handle.write(svg)

print("wrote docs/logo.svg", len(svg))
