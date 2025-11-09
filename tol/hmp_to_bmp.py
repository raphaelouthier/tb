#! /bin/python3

import sys, struct
from PIL import Image

if len(sys.argv) != 6 or ("-h" in sys.argv):
    print("hmp_to_bmp src dst tim_nb tck_nb vol_ref")
    exit(0)
assert len(sys.argv) == 6


src_pth = sys.argv[1]
dst_pth = sys.argv[2]
tim_nb = int(sys.argv[3])
tck_nb = int(sys.argv[4])
vol_ref = int(sys.argv[5])

tck_idx = tck_nb - 1;
tim_idx = 0;

float_values = []

dst = Image.new('RGB', (tim_nb, tck_nb)) 

with open(src_pth, 'rb') as src:
    while True:
        flt_bin = src.read(8) 
        if not flt_bin:
            break
        
        try:
            flt = struct.unpack('d', flt_bin)[0] 
            assert flt <= vol_ref
            assert flt >= -vol_ref
            val = int((flt * 255.) / vol_ref) 
            if val < 0:
                pxl = (0, 0, -val)
            else:
                pxl = (val, 0, 0)
            assert tim_idx < tim_nb
            print(val)
            dst.putpixel((tim_idx, tck_idx), pxl)
            if (tck_idx == 0):
                tck_idx = tck_nb
                tim_idx += 1;
            tck_idx -= 1;
        except struct.error:
            print("Malformed heatmap.");
            break

assert tck_idx == tck_nb - 1
assert tim_idx == tim_nb

dst.save(dst_pth) 

