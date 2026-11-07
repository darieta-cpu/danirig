"""Genera l'icona di DaniRig (manopola cromata su fondo scuro con tacche oro).
Uso: python3 make_icon.py   ->  icon_1024.png, icon_256.png, icon.ico  (richiede Pillow)"""
import math
from PIL import Image, ImageDraw, ImageFilter

S = 2048
GOLD = (0xD4, 0xB1, 0x5F)

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))

img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

# fondo: quadrato arrotondato con gradiente verticale
bg = Image.new("RGBA", (S, S))
d = ImageDraw.Draw(bg)
for y in range(S):
    d.line([(0, y), (S, y)], fill=lerp((0x34, 0x39, 0x41), (0x0a, 0x0b, 0x0d), y / S) + (255,))
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([40, 40, S - 40, S - 40], radius=int(S * 0.225), fill=255)
img.paste(bg, (0, 0), mask)
d = ImageDraw.Draw(img)
d.rounded_rectangle([70, 70, S - 70, S - 70], radius=int(S * 0.205), outline=GOLD + (190,), width=14)

cx, cy = S / 2, S / 2 + 20
# tacche
for i in range(11):
    a = math.radians(-135 + i * 27)
    big = i % 5 == 0
    r1, r2 = S * 0.385, S * (0.445 if big else 0.425)
    d.line([(cx + r1 * math.sin(a), cy - r1 * math.cos(a)), (cx + r2 * math.sin(a), cy - r2 * math.cos(a))],
           fill=GOLD + (255,), width=26 if big else 16)

# ombra della manopola
R = S * 0.305
sh = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(sh).ellipse([cx - R, cy - R + 46, cx + R, cy + R + 46], fill=(0, 0, 0, 200))
img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(34)))

# corpo cromato: cerchi concentrici con centro che si sposta verso l'alto a sinistra
d = ImageDraw.Draw(img)
N = 160
for k in range(N):
    f = k / (N - 1)
    r = R * (1 - f * 0.97)
    ox, oy = -R * 0.20 * f, -R * 0.22 * f
    c = lerp((0x4a, 0x4f, 0x57), (0xf4, 0xf5, 0xf7), f ** 1.4)
    d.ellipse([cx + ox - r, cy + oy - r, cx + ox + r, cy + oy + r], fill=c + (255,))
d.ellipse([cx - R, cy - R, cx + R, cy + R], outline=(0, 0, 0, 230), width=12)

# cappuccio centrale (più scuro, in rilievo)
r2 = R * 0.64
for k in range(100):
    f = k / 99
    r = r2 * (1 - f * 0.9)
    ox, oy = r2 * 0.10 * f, r2 * 0.12 * f
    c = lerp((0xd9, 0xdc, 0xe1), (0x7d, 0x82, 0x8a), f ** 0.9)
    d.ellipse([cx + ox - r, cy + oy - r, cx + ox + r, cy + oy + r], fill=c + (255,))
d.ellipse([cx - r2, cy - r2, cx + r2, cy + r2], outline=(0, 0, 0, 140), width=8)

# indicatore oro
a = math.radians(-38)
p1, p2 = R * 0.16, R * 0.93
d.line([(cx + p1 * math.sin(a), cy - p1 * math.cos(a)), (cx + p2 * math.sin(a), cy - p2 * math.cos(a))], fill=GOLD + (255,), width=58)
for sgn in (0, 1):
    q = p2 if sgn else p1
    d.ellipse([cx + q * math.sin(a) - 29, cy - q * math.cos(a) - 29, cx + q * math.sin(a) + 29, cy - q * math.cos(a) + 29], fill=GOLD + (255,))

# riflesso
gl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(gl).ellipse([cx - R * 0.78, cy - R * 0.95, cx + R * 0.15, cy - R * 0.25], fill=(255, 255, 255, 46))
img.alpha_composite(gl.filter(ImageFilter.GaussianBlur(40)))

img.resize((1024, 1024), Image.LANCZOS).save("icon_1024.png")
img.resize((256, 256), Image.LANCZOS).save("icon_256.png")
img.resize((256, 256), Image.LANCZOS).save("icon.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print("ok")
